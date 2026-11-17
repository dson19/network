# Thiết kế server

Tài liệu này mô tả server sẽ được code như thế nào: cấu trúc thư mục, các module, struct chính, file cấu hình, định dạng log, cách build và chạy.

- Kiến trúc và lý do chọn: [PROTOCOL.md](PROTOCOL.md) mục 1.
- Các luồng xử lý: [DESIGN.md](DESIGN.md).
- Database: [DATABASE.md](DATABASE.md).

---

## 1. Cấu trúc thư mục project

```
Project/
├── Makefile
├── server.conf              cấu hình mặc định
├── data/
│   └── questions.txt        câu hỏi soạn sẵn
├── common/                  dùng chung cho server, client và test
│   ├── cJSON.c, cJSON.h     thư viện JSON (MIT), nhúng nguyên file
│   ├── protocol.h           tên lệnh, tên event, mã kết quả, giới hạn
│   ├── linebuf.c/.h         tách dòng từ luồng byte TCP
│   └── msg.c/.h             tạo và đọc request/response/event
├── server/
│   ├── main.c               đọc tham số dòng lệnh, cấu hình, khởi động
│   ├── config.c/.h          đọc server.conf
│   ├── loop.c/.h            vòng lặp epoll, accept, đọc, ghi
│   ├── conn.c/.h            struct Connection, buffer nhận/gửi
│   ├── dispatch.c/.h        bảng lệnh, kiểm tra quyền theo thứ tự cố định
│   ├── session.c/.h         phiên đăng nhập, thay thế phiên, online
│   ├── cache.c/.h           users, teams, câu hỏi trong RAM
│   ├── team.c/.h            đội, slot, đội trưởng, lời mời, lời xin vào
│   ├── lobby.c/.h           thách đấu, sảnh chờ
│   ├── match.c/.h           luật chơi: hàm thuần, không gửi mạng
│   ├── match_net.c/.h       nối match.c với mạng: event, snapshot, đồng hồ
│   ├── snapshot.c/.h        tạo snapshot riêng cho từng người nhận
│   ├── timer.c/.h           min-heap các mốc hết hạn
│   ├── pool.c/.h            thread pool đọc + DB writer + eventfd
│   ├── storage.c/.h         toàn bộ SQL
│   ├── auth.c/.h            PBKDF2, salt, so sánh hash thời gian hằng
│   ├── rng.c/.h             số ngẫu nhiên có seed (để test lặp lại được)
│   └── log.c/.h             ghi log qua pool
├── client/                  xem CLIENT.md
├── tests/                   xem TESTING.md
└── docs/design/
```

`match.c` không biết gì về socket hay JSON. Nó chỉ nhận vào `Match *` và một hành động, rồi trả về kết quả. Nhờ vậy:
- Unit test luật chơi không cần mở mạng.
- Sau này có thể chia các trận ra nhiều luồng mà không phải viết lại luật chơi.

---

## 2. Các struct chính

Đây là phác thảo để thống nhất cách tổ chức dữ liệu, không phải code cuối cùng.

```c
/* ---------- Kết nối ---------- */
typedef enum { CONN_CONNECTED, CONN_AUTH_PENDING, CONN_LOGGED_IN, CONN_CLOSING } ConnState;

typedef struct Connection {
    int        fd;
    uint64_t   id;              /* tăng dần, không tái sử dụng (fd thì bị tái sử dụng) */
    ConnState  state;
    LineBuf    in;              /* buffer nhận, tối đa 8192 byte 1 dòng */
    ByteQueue  out;             /* buffer gửi, tối đa send_buffer_max */
    int64_t    last_rx_ms;      /* lần cuối nhận được byte, cho idle timeout */
    struct Session *session;    /* NULL nếu chưa đăng nhập */
    int        busy;            /* 1 khi đang chờ pool: request sau phải xếp hàng */
    char       peer[48];        /* ip:port, để ghi log */
} Connection;

/* ---------- Phiên và người chơi ---------- */
typedef enum { ROLE_PLAYER, ROLE_ADMIN } Role;
typedef enum { PS_IDLE, PS_LOBBY, PS_IN_MATCH } PlayerState;

typedef struct User {               /* nằm trong cache, tồn tại cả khi offline */
    int64_t     id;
    char        username[21];
    Role        role;
    int         locked;
    struct Team *team;              /* NULL nếu chưa có đội */
    struct Session *session;        /* NULL nếu offline */
    struct Match *match;            /* NULL nếu không trong trận, giữ cả khi offline */
} User;

typedef struct Session {
    User       *user;
    Connection *conn;
} Session;

/* ---------- Đội ---------- */
typedef struct Team {
    int64_t     id;
    char        name[121];          /* 30 ký tự UTF-8, tối đa 4 byte mỗi ký tự */
    User       *slots[3];           /* NULL = slot trống, không dồn */
    User       *captain;
    PlayerState state;              /* IDLE, LOBBY, IN_MATCH */
    struct Lobby *lobby;
    struct Match *match;
    /* lời xin vào và lời mời: danh sách user_id, chỉ trong RAM */
    IdList      join_requests;
    IdList      invites;
} Team;

/* ---------- Trận ---------- */
typedef enum { ARMOR_BASIC, ARMOR_ADVANCED } ArmorType;
typedef struct { ArmorType type; int amor; } ArmorLayer;

typedef struct Ship {
    User      *user;
    int        slot;
    int        alive, online;
    int        hp;                  /* 0..1000 */
    int        coin;
    ArmorLayer armor[2];  int armor_count;   /* armor[armor_count-1] là lớp ngoài */
    int        auto_mag, auto_boxes;         /* pháo tự động: băng, kho thùng */
    int        laser_mag[3]; int laser_count; /* băng từng khẩu laze */
    int        power_packs;                  /* kho bộ nguồn chung */
    int        missiles;                     /* 0..4 */
    int        damage_dealt;                 /* thống kê, ghi vào match_players */
} Ship;

typedef enum { TS_ACTING, TS_CHEST } TurnState;
typedef enum { CHEST_NONE, CHEST_BRONZE, CHEST_SILVER, CHEST_GOLD } ChestTier;

typedef struct Match {
    int64_t    id;
    Team      *teams[2];            /* [0] = đội chủ (A), [1] = đội khách (B) */
    Ship       ships[2][3];
    int        size;                /* số người mỗi đội, 1..3 */
    Settings   settings;            /* turn_time, chest_time, start_coin */
    int        turn_team, turn_slot;
    int        turns_played;
    TurnState  turn_state;
    int64_t    turn_deadline_ms;
    int64_t    turn_remaining_ms;   /* lưu lại khi tạm dừng vì mở rương */
    ChestTier  chest;  int chest_opened;
    const Question *chest_question;
    int64_t    chest_deadline_ms;
    int64_t    forfeit_deadline_ms[2];  /* 0 = không đếm */
    Bitset     used_questions;      /* không lặp câu hỏi trong 1 trận */
    int64_t    started_at;
} Match;

/* ---------- Đồng hồ ---------- */
typedef enum { T_TURN, T_CHEST, T_IDLE, T_FORFEIT } TimerKind;
typedef struct {
    int64_t   at_ms;
    TimerKind kind;
    uint64_t  target_id;            /* conn id hoặc match id */
    uint32_t  generation;           /* để hủy "lười": đồng hồ cũ bị bỏ qua */
} Timer;
```

**Hủy đồng hồ:** khi một lượt kết thúc sớm (người chơi tấn công), server không xóa đồng hồ cũ khỏi heap. Thay vào đó, `Match` tăng `generation` lên. Khi đồng hồ cũ tới hạn mà `generation` không khớp thì bị bỏ qua. Cách này đơn giản và luôn đúng.

---

## 3. Vòng đời một request trên server

1. `epoll_wait` báo socket đọc được. `loop.c` đọc **hết** dữ liệu đang có (socket non-blocking, đọc tới khi `EAGAIN`).
2. `linebuf` tách ra từng dòng hoàn chỉnh. Dòng dài quá 8192 byte thì trả `2004` và đóng kết nối.
3. Nếu kết nối đang `busy` (chờ pool) thì dừng, để các dòng còn lại trong buffer. Khi pool trả kết quả xong mới xử lý tiếp.
4. `dispatch.c` parse JSON, tra bảng lệnh, kiểm tra theo thứ tự cố định (PROTOCOL.md mục 9.3), rồi gọi hàm xử lý.
5. Hàm xử lý sửa state trong RAM, rồi xếp response và event vào buffer gửi của các kết nối liên quan.
6. Thay đổi cần lưu lâu dài được đẩy vào hàng đợi DB writer. Dòng log được đẩy vào hàng đợi log.
7. `loop.c` thử gửi ngay. Gửi chưa hết thì bật `EPOLLOUT` để gửi tiếp lần sau.

---

## 4. File cấu hình `server.conf`

Định dạng `khóa = giá trị`, mỗi dòng một khóa. Dòng bắt đầu bằng `#` là chú thích. Khóa không có trong file thì dùng giá trị mặc định.

```ini
# --- Mạng ---
bind_address      = 0.0.0.0
port              = 5500
max_connections   = 10000
max_message_bytes = 8192
send_buffer_max   = 262144     # 256 KB
idle_timeout_s    = 60

# --- Thread pool ---
read_workers      = 4          # luồng đọc DB + hash mật khẩu
job_queue_max     = 10000      # vượt quá thì trả 9002 SERVER_BUSY

# --- Lưu trữ ---
db_path           = data/game.db
log_path          = logs/server.log
log_level         = INFO       # DEBUG, INFO, WARN, ERROR
question_seed     = data/questions.txt

# --- Bảo mật ---
pbkdf2_iterations = 100000

# --- Luật chơi ---
default_turn_time_s  = 30
default_chest_time_s = 30
default_start_coin   = 0
min_turn_time_s      = 10      # khoảng cho phép trong sảnh chờ
max_turn_time_s      = 120
min_chest_time_s     = 10
max_chest_time_s     = 120
max_start_coin       = 100000
forfeit_timeout_s    = 120
chest_weights        = 60,30,10   # đồng, bạc, vàng (%)

# --- Kiểm thử ---
rng_seed          = 0          # 0 = ngẫu nhiên theo thời gian; khác 0 = lặp lại được
```

- **Đọc cấu hình ở đâu:** mặc định là `server.conf` trong thư mục hiện tại. Đổi bằng `./server -c <file>`.
- **Giá trị không hợp lệ** (sai kiểu, ngoài khoảng, `min > max`): báo lỗi kèm số dòng rồi thoát, **không** tự đoán giá trị.
- **`rng_seed` và các khóa `min_*`:** tồn tại để test chạy nhanh và lặp lại được. Ví dụ file cấu hình cho test đặt `min_turn_time_s = 1` và `rng_seed = 42` (xem TESTING.md).

### Tham số dòng lệnh

| Tham số | Ý nghĩa |
|---|---|
| `-c <file>` | Dùng file cấu hình khác |
| `-p <port>` | Ghi đè `port` (test dùng `-p 0` để hệ điều hành tự chọn port trống) |
| `--create-admin <username>` | Tạo hoặc nâng quyền admin rồi thoát (DATABASE.md mục 4) |
| `--import-questions <file>` | Nạp thêm câu hỏi rồi thoát (DATABASE.md mục 5) |
| `--port-file <file>` | Sau khi `listen` xong, ghi port thực tế ra file. Test đọc file này để biết kết nối vào đâu |

---

## 5. Log hoạt động

### 5.1. Định dạng

Mỗi sự kiện là **1 dòng**, gồm các trường ngăn cách bằng dấu cách:

```
<thời gian ISO-8601 có mili giây và múi giờ> <MỨC> <NHÓM> <SỰ_KIỆN> khóa=giá_trị ...
```

```
2026-11-17T10:22:31.123+07:00 INFO  NET   CONNECT     conn=12 peer=192.168.1.5:51234
2026-11-17T10:22:31.402+07:00 INFO  AUTH  LOGIN_OK    conn=12 user=son
2026-11-17T10:22:35.010+07:00 WARN  AUTH  LOGIN_FAIL  conn=13 user=son code=3003
2026-11-17T10:23:02.551+07:00 INFO  TEAM  CREATE      user=son team=5 name="Rong Bay"
2026-11-17T10:25:40.000+07:00 INFO  MATCH START       match=3 team_a=5 team_b=8 size=2
2026-11-17T10:26:10.777+07:00 INFO  MATCH ATTACK      match=3 from=son to=minh gun=LASER:2 missiles=1 damage=1000 hp_lost=500
2026-11-17T10:26:41.000+07:00 INFO  MATCH TURN_SKIP   match=3 user=lan reason=TIMEOUT
2026-11-17T10:40:12.300+07:00 INFO  MATCH END         match=3 winner=5 reason=ELIMINATED turns=41
2026-11-17T10:41:00.000+07:00 INFO  ADMIN LOCK        by=admin user=hacker locked=1
```

- **Mức:** `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`.
- **Nhóm:** `NET`, `AUTH`, `TEAM`, `LOBBY`, `MATCH`, `CHEST`, `ADMIN`, `DB`, `SYS`.
- Giá trị có dấu cách hoặc dấu `"` thì được bọc trong `"..."` và escape theo kiểu JSON. Nhờ vậy mỗi dòng log tách lại được bằng máy.
- **Không bao giờ ghi mật khẩu**, kể cả mật khẩu sai, và không ghi `pass_hash`.

### 5.2. Những gì phải ghi

| Nhóm | Sự kiện |
|---|---|
| `NET` | Kết nối, ngắt kết nối (kèm lý do), message quá dài, JSON hỏng |
| `AUTH` | Đăng ký, đăng nhập thành công/thất bại, đăng xuất, đổi mật khẩu, thay thế phiên |
| `TEAM` | Tạo, xóa, xin vào, mời, duyệt/từ chối, rời, kick, chuyển đội trưởng |
| `LOBBY` | Gửi/chấp nhận/từ chối/hủy thách đấu, đổi setting, sẵn sàng, bắt đầu, đóng sảnh |
| `MATCH` | Bắt đầu trận, mua, sửa, reload, tấn công, bỏ lượt, tàu bị hạ, mất/có lại kết nối, kết thúc |
| `CHEST` | Thả rương, mở, trả lời (đúng/sai/hết giờ). Ghi `question_id` chứ không ghi đáp án |
| `ADMIN` | Mọi lệnh admin, ghi rõ ai làm và làm với ai |
| `DB` | Lỗi ghi, thử lại, chuyển chế độ chỉ đọc |
| `SYS` | Khởi động, tắt, đọc cấu hình, nạp câu hỏi |

### 5.3. Cách ghi

- Luồng chính chỉ định dạng dòng log rồi đẩy vào hàng đợi log. Việc ghi ra file nằm ở **luồng DB writer**, nên luồng chính không bao giờ chờ ổ đĩa.
- File mở ở chế độ append. Mỗi dòng được ghi bằng 1 lần `write()`.
- `ADMIN_LOG` đọc ngược từ cuối file để lấy `limit` dòng mới nhất. Việc đọc chạy trên pool.

---

## 6. Build và môi trường

### 6.1. Thư viện cần cài

| Thư viện | Gói trên Ubuntu 20.04 | Dùng cho |
|---|---|---|
| SQLite 3.31 | `libsqlite3-dev` | Database |
| OpenSSL 1.1.1 | `libssl-dev` | `PKCS5_PBKDF2_HMAC` và `RAND_bytes` (salt) |
| GTK 3.24 | `libgtk-3-dev` | Client |
| pthread | có sẵn trong glibc | Thread pool |
| cJSON | nhúng sẵn trong `common/` | JSON |

### 6.2. Cờ biên dịch

```make
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -D_GNU_SOURCE
LDLIBS  = -lsqlite3 -lcrypto -lpthread
```

- **`-std=c11` là bắt buộc**, vì bài được chấm bằng GCC 9.4 trên Ubuntu 20.04.
- **`-D_GNU_SOURCE`:** với `-std=c11`, glibc ẩn bớt các hàm POSIX và Linux (`accept4`, `pipe2`, `strdup`, `getline`, ...). `_GNU_SOURCE` bao gồm luôn `_POSIX_C_SOURCE=200809L`.
- **Bản debug và bản test** thêm `-O0 -g -fsanitize=address,undefined`. Kiểm tra race condition giữa luồng chính và pool dùng `-fsanitize=thread`, chạy bằng GCC trên máy (GCC 9 trong môi trường Ubuntu 20.04 không link được TSan).

### 6.3. Make target

| Target | Việc làm |
|---|---|
| `make` | Build `server` và `client` |
| `make server` / `make client` | Build riêng từng cái |
| `make test` | Build bản test rồi chạy unit test + integration test |
| `make test-unit` / `make test-integration` | Chạy riêng từng loại |
| `make test-load` | Chạy load test (không nằm trong `make test` vì lâu) |
| `make tsan` | Build và chạy integration test với ThreadSanitizer |
| `make check-ubuntu20` | Build và chạy `make test` trong môi trường Ubuntu 20.04 |
| `make diagrams` | Render lại ảnh PNG cho các sơ đồ |
| `make clean` | Xóa file build |
