# Database và lưu trữ

Tài liệu này mô tả dữ liệu lâu dài của server: lược đồ SQLite, cách giữ RAM và DB khớp nhau, cách tạo admin đầu tiên và cách nạp câu hỏi.

- Phân loại dữ liệu nào để ở DB, RAM hay file: xem [SPEC.md](SPEC.md) mục 1.
- Lý do chọn SQLite: xem [PROTOCOL.md](PROTOCOL.md) mục 1.6.

---

## 1. Lược đồ

Sơ đồ quan hệ: [diagrams/database-er.md](diagrams/database-er.md)

### 1.1. Pragma khi mở mỗi kết nối DB

```sql
PRAGMA foreign_keys = ON;      -- SQLite mặc định TẮT khóa ngoại, phải bật mỗi lần mở
PRAGMA journal_mode = WAL;     -- 1 luồng ghi + nhiều luồng đọc cùng lúc
PRAGMA synchronous = NORMAL;   -- an toàn với WAL, nhanh hơn FULL
PRAGMA busy_timeout = 5000;    -- chờ tối đa 5s nếu DB đang bị khóa, thay vì lỗi ngay
```

### 1.2. Các bảng

```sql
-- Tài khoản
CREATE TABLE users (
  id            INTEGER PRIMARY KEY,
  username      TEXT    NOT NULL UNIQUE COLLATE NOCASE,
  pass_hash     BLOB    NOT NULL,
  salt          BLOB    NOT NULL,
  iterations    INTEGER NOT NULL,
  role          TEXT    NOT NULL DEFAULT 'PLAYER' CHECK (role IN ('PLAYER', 'ADMIN')),
  locked        INTEGER NOT NULL DEFAULT 0 CHECK (locked IN (0, 1)),
  created_at    INTEGER NOT NULL,
  last_login_at INTEGER
);

-- Đội
CREATE TABLE teams (
  id         INTEGER PRIMARY KEY,
  name       TEXT    NOT NULL UNIQUE,
  captain_id INTEGER NOT NULL REFERENCES users(id),
  created_at INTEGER NOT NULL
);

-- Thành viên đội
CREATE TABLE team_members (
  team_id   INTEGER NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
  user_id   INTEGER NOT NULL UNIQUE REFERENCES users(id),
  slot      INTEGER NOT NULL CHECK (slot BETWEEN 0 AND 2),
  joined_at INTEGER NOT NULL,
  PRIMARY KEY (team_id, slot)
);

-- Ngân hàng câu hỏi
CREATE TABLE questions (
  id       INTEGER PRIMARY KEY,
  tier     TEXT    NOT NULL CHECK (tier IN ('BRONZE', 'SILVER', 'GOLD')),
  question TEXT    NOT NULL,
  choice_a TEXT    NOT NULL,
  choice_b TEXT    NOT NULL,
  choice_c TEXT    NOT NULL,
  choice_d TEXT    NOT NULL,
  answer   TEXT    NOT NULL CHECK (answer IN ('A', 'B', 'C', 'D')),
  active   INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);
CREATE INDEX idx_questions_tier ON questions(tier, active);

-- Lịch sử trận đấu
CREATE TABLE matches (
  id          INTEGER PRIMARY KEY,
  team_a_id   INTEGER NOT NULL,          -- không FK: đội bị xóa vẫn giữ lịch sử
  team_b_id   INTEGER NOT NULL,
  team_a_name TEXT    NOT NULL,          -- lưu tên tại thời điểm đấu
  team_b_name TEXT    NOT NULL,
  winner_id   INTEGER NOT NULL,
  reason      TEXT    NOT NULL CHECK (reason IN ('ELIMINATED', 'FORFEIT')),
  turn_time   INTEGER NOT NULL,
  chest_time  INTEGER NOT NULL,
  start_coin  INTEGER NOT NULL,
  turns       INTEGER NOT NULL,          -- tổng số lượt đã chơi
  started_at  INTEGER NOT NULL,
  ended_at    INTEGER NOT NULL
);
CREATE INDEX idx_matches_team_a ON matches(team_a_id);
CREATE INDEX idx_matches_team_b ON matches(team_b_id);

-- Người chơi trong từng trận
CREATE TABLE match_players (
  match_id INTEGER NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
  user_id  INTEGER NOT NULL,             -- không FK, cùng lý do như trên
  username TEXT    NOT NULL,
  team_id  INTEGER NOT NULL,
  slot     INTEGER NOT NULL,
  survived INTEGER NOT NULL CHECK (survived IN (0, 1)),
  damage_dealt INTEGER NOT NULL,
  PRIMARY KEY (match_id, user_id)
);
CREATE INDEX idx_match_players_user ON match_players(user_id);
```

Mọi thời điểm đều là **unix giây** (`INTEGER`). Không dùng `DATETIME` để tránh lệch múi giờ.

### 1.3. Giải thích các ràng buộc

| Ràng buộc | Bảo đảm luật nào |
|---|---|
| `users.username UNIQUE COLLATE NOCASE` | Không có 2 tài khoản `Son` và `son` |
| `team_members.user_id UNIQUE` | Mỗi người chỉ ở 1 đội |
| `PRIMARY KEY (team_id, slot)` + `CHECK (slot BETWEEN 0 AND 2)` | Mỗi slot đúng 1 người, tối đa 3 người mỗi đội |
| `teams.captain_id` (không phải cột cờ trong `team_members`) | Mỗi đội có đúng 1 đội trưởng |
| `ON DELETE CASCADE` trên `team_members` | Xóa đội thì tự xóa thành viên |
| `questions.active` | Xóa mềm: admin xóa câu hỏi không làm mất dữ liệu cũ |
| `matches` không có khóa ngoại tới `teams` | Đội bị xóa vẫn còn lịch sử trận đấu |

Đội trưởng phải là thành viên của chính đội đó. SQLite không diễn đạt được ràng buộc này bằng `CHECK`, nên lớp `storage` kiểm tra trong cùng transaction khi tạo đội hoặc chuyển quyền.

### 1.4. Phiên bản lược đồ

- Lưu số phiên bản trong `PRAGMA user_version`. Lược đồ hiện tại là **phiên bản 1**.
- Khi server khởi động:
  - DB trống (`user_version = 0`): chạy toàn bộ `CREATE TABLE` trong 1 transaction, rồi đặt `user_version = 1`.
  - DB đã có phiên bản cũ hơn: chạy lần lượt các bước nâng cấp.
  - DB có phiên bản mới hơn server: từ chối khởi động, tránh làm hỏng dữ liệu.
- Lược đồ chỉ dùng tính năng có từ **SQLite 3.31** (bản đi kèm Ubuntu 20.04). Vì vậy **không dùng** `RETURNING` (cần 3.35). Lấy id vừa tạo bằng `sqlite3_last_insert_rowid()`.

---

## 2. RAM và DB khớp nhau thế nào

### 2.1. Quy tắc

1. **Lúc khởi động, server nạp toàn bộ dữ liệu lâu dài vào RAM**: users (trừ `pass_hash` và `salt`), teams, team_members, và các câu hỏi đang `active`.
2. **Luồng chính chỉ đọc từ RAM**, không bao giờ chờ DB để trả lời một câu hỏi kiểu "đội này có đủ người chưa".
3. **Ghi theo kiểu RAM trước, DB sau:**
   1. Luồng chính kiểm tra điều kiện trên RAM.
   2. Luồng chính sửa RAM.
   3. Luồng chính trả response và gửi event.
   4. Luồng chính đẩy lệnh ghi vào **hàng đợi ghi**.
4. **Hàng đợi ghi chỉ có 1 luồng xử lý (DB writer)**, chạy theo đúng thứ tự FIFO. Nhờ vậy thứ tự ghi xuống DB giống hệt thứ tự thay đổi trong RAM. Ví dụ "vào đội" luôn được ghi trước "rời đội".
5. Các việc **cần kết quả từ DB** thì đi qua thread pool đọc, không qua DB writer:
   - `LOGIN`: đọc `pass_hash` và `salt`, rồi tính PBKDF2.
   - `ADMIN_LOG`: đọc đuôi file log.
6. **Ngoại lệ là `REGISTER`:** phải có `id` do DB cấp thì mới thêm được vào RAM. Vì vậy:
   1. Luồng chính kiểm tra tên đăng nhập chưa có trong RAM, rồi **giữ chỗ** tên đó (đánh dấu `pending`) để 2 người không đăng ký trùng cùng lúc.
   2. Pool hash mật khẩu, rồi đẩy lệnh `INSERT` sang DB writer.
   3. DB writer báo `id` về. Luồng chính thêm user vào RAM và trả `1000`.

### 2.2. Vì sao chọn cách này

| Phương án | Ưu | Nhược |
|---|---|---|
| **RAM trước, DB sau, 1 luồng ghi** (đã chọn) | Luồng chính không bao giờ chờ DB. Không phải "giữ chỗ" cho đội hay slot. Thứ tự ghi luôn đúng | Server sập thì có thể mất vài thay đổi cuối cùng chưa kịp ghi (cỡ mili giây) |
| DB trước, RAM sau | Không bao giờ mất thay đổi đã báo thành công | Trong lúc chờ DB, phải khóa đội lại để không ai sửa song song. Phức tạp và chậm hơn nhiều |
| Không cache, luôn đọc DB | Đơn giản về ý tưởng | Mọi lệnh đều phải chờ pool, làm mất lợi thế của luồng chính epoll |

**Dung lượng RAM:** mỗi user trong cache khoảng 64 byte. 100.000 user tốn khoảng 6 MB, không đáng kể.

### 2.3. Khi ghi DB bị lỗi

Mọi ràng buộc đều đã được kiểm tra trên RAM trước khi ghi, nên lỗi ghi chỉ có thể đến từ ổ đĩa (đầy, hỏng, mất quyền ghi). Khi đó:

1. DB writer ghi log mức `ERROR`, kèm câu lệnh bị lỗi.
2. DB writer **thử lại 3 lần**, mỗi lần cách nhau 100 ms.
3. Vẫn lỗi: server chuyển sang **chế độ chỉ đọc**:
   - Mọi lệnh làm thay đổi dữ liệu lâu dài (đăng ký, đội, admin) bị từ chối với mã `9001`.
   - Các trận đang chạy vẫn chơi tiếp. Kết quả trận chỉ được ghi vào log.
   - Ghi log mức `FATAL` để admin biết mà xử lý.

### 2.4. Tắt server an toàn

Khi nhận `SIGINT` hoặc `SIGTERM`:

1. Ngừng nhận kết nối mới.
2. Gửi `FORCE_LOGOUT` (`reason: "SHUTDOWN"`) cho mọi client.
3. Chờ hàng đợi ghi chạy hết, tối đa 10 giây.
4. Đóng DB và file log, rồi thoát.

Các trận đang dở bị hủy và **không** được ghi vào bảng `matches`.

---

## 3. Lớp `storage`

Mọi câu SQL chỉ nằm trong `storage.c`. Các hàm ở đây **chạy trên luồng của pool hoặc DB writer**, không bao giờ chạy trên luồng chính. Mỗi luồng giữ một kết nối SQLite riêng (`sqlite3 *`).

```c
/* Mở DB, bật pragma, tạo hoặc nâng cấp lược đồ. Gọi 1 lần cho mỗi luồng. */
int  storage_open(const char *path, sqlite3 **out);
void storage_close(sqlite3 *db);

/* Nạp toàn bộ dữ liệu lâu dài vào cache lúc khởi động. */
int storage_load_all(sqlite3 *db, Cache *cache);

/* Tài khoản */
int storage_user_create(sqlite3 *db, const char *username, const uint8_t *hash,
                        const uint8_t *salt, int iterations, const char *role, int64_t *out_id);
int storage_user_get_credentials(sqlite3 *db, const char *username, Credentials *out);
int storage_user_set_password(sqlite3 *db, int64_t user_id, const uint8_t *hash,
                              const uint8_t *salt, int iterations);
int storage_user_set_locked(sqlite3 *db, int64_t user_id, int locked);
int storage_user_touch_login(sqlite3 *db, int64_t user_id, int64_t when);

/* Đội */
int storage_team_create(sqlite3 *db, const char *name, int64_t captain_id, int64_t *out_id);
int storage_team_delete(sqlite3 *db, int64_t team_id);
int storage_team_set_captain(sqlite3 *db, int64_t team_id, int64_t user_id);
int storage_member_add(sqlite3 *db, int64_t team_id, int64_t user_id, int slot);
int storage_member_remove(sqlite3 *db, int64_t team_id, int64_t user_id);

/* Câu hỏi */
int storage_question_add(sqlite3 *db, const Question *q, int64_t *out_id);
int storage_question_deactivate(sqlite3 *db, int64_t question_id);
int storage_question_import_file(sqlite3 *db, const char *path, int *out_count);

/* Lịch sử trận */
int storage_match_save(sqlite3 *db, const MatchRecord *m);
```

- Mọi hàm trả về `STORAGE_OK`, `STORAGE_CONSTRAINT` (vi phạm `UNIQUE` hoặc `CHECK`), `STORAGE_NOT_FOUND`, hoặc `STORAGE_ERROR`.
- Mọi câu lệnh đều dùng **prepared statement với tham số `?`**. Không bao giờ ghép chuỗi SQL bằng dữ liệu người dùng, để chống SQL injection.
- Thao tác nhiều bước (tạo đội = `INSERT teams` + `INSERT team_members`) nằm trong 1 transaction `BEGIN IMMEDIATE ... COMMIT`.

---

## 4. Tạo admin đầu tiên

Protocol không có lệnh nào tự nâng quyền lên admin, nên admin đầu tiên phải được tạo **ngay trên máy chạy server**:

```
./server --create-admin <username>
```

1. Server hỏi mật khẩu 2 lần qua terminal, **không hiện ký tự** (tắt echo bằng `termios`).
2. Kiểm tra định dạng username và mật khẩu như lệnh `REGISTER`.
3. Nếu username chưa tồn tại: tạo tài khoản mới với `role = 'ADMIN'`.
4. Nếu username đã tồn tại: hỏi xác nhận rồi nâng tài khoản đó lên `ADMIN` (không đổi mật khẩu).
5. In kết quả rồi thoát, **không** khởi động server.

Ai chạy được lệnh này thì phải có quyền truy cập file DB, nên cách này không mở thêm lỗ hổng nào qua mạng.

---

## 5. Câu hỏi soạn sẵn

### 5.1. Định dạng file `data/questions.txt`

Mỗi câu hỏi là một khối, các khối cách nhau bởi một dòng trống. Dòng bắt đầu bằng `#` là chú thích.

```
# Câu hỏi cho rương kho báu - mã hóa UTF-8

[BRONZE]
Q: Giao thức nào đảm bảo dữ liệu đến đúng thứ tự?
A: UDP
B: TCP
C: ICMP
D: ARP
ANSWER: B

[GOLD]
Q: Hàm nào trả về các socket sẵn sàng mà không phải quét toàn bộ danh sách?
A: select()
B: poll()
C: epoll_wait()
D: accept()
ANSWER: C
```

Quy định:
- Thứ tự dòng trong khối là cố định: `[TIER]`, `Q:`, `A:`, `B:`, `C:`, `D:`, `ANSWER:`.
- Mỗi trường nằm trên 1 dòng, tối đa 500 ký tự. Tổng 1 khối không vượt quá giới hạn message 8192 byte khi gửi đi.
- Khối sai định dạng: bỏ qua khối đó, ghi log `WARN` kèm số dòng, rồi đọc tiếp các khối sau.

### 5.2. Khi nào nạp

- Lúc khởi động, nếu bảng `questions` **trống** thì server tự nạp từ `question_seed` trong file cấu hình.
- Nạp thêm bằng tay: `./server --import-questions <file>`. Lệnh này thêm vào các câu đã có, không xóa câu cũ.
- Phải có **ít nhất 1 câu hỏi đang active cho mỗi tier** thì server mới khởi động. Thiếu tier nào thì báo lỗi và thoát, vì rương tier đó sẽ không có câu hỏi.

### 5.3. Chọn câu hỏi cho rương

- Chọn ngẫu nhiên trong các câu cùng tier đang `active`.
- **Không lặp lại câu hỏi trong cùng một trận** chừng nào tier đó còn câu chưa dùng. Dùng hết thì cho lặp lại từ đầu.
