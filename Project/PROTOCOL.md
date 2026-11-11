# Protocol & kiến trúc: "Phi đội không gian"

Tài liệu này đi kèm [SPEC.md](SPEC.md). SPEC mô tả luật chơi; tài liệu này mô tả kiến trúc và giao thức client–server.

---

## 1. Các quyết định kiến trúc và lý do chọn

### 1.1. Server: 1 luồng chính dùng epoll + thread pool

**Mô hình:**
- **Luồng chính** chạy vòng lặp `epoll_wait()`. Luồng này nhận và gửi trên mọi socket, và là luồng **duy nhất** được đọc/sửa state: phiên đăng nhập, đội, lời mời, sảnh chờ, trận đấu, đồng hồ.
- **Thread pool** gồm N luồng, chỉ làm các việc chậm hoặc có thể bị chặn:
  - hash và kiểm tra mật khẩu,
  - đọc/ghi SQLite,
  - ghi log ra file.
- Luồng chính đẩy việc vào hàng đợi việc (có mutex). Pool làm xong thì đẩy kết quả vào hàng đợi kết quả, rồi đánh thức luồng chính qua một `eventfd` đã được đăng ký vào epoll.
- Đồng hồ lượt và đồng hồ rương **không dùng `sleep` hay luồng riêng**. Luồng chính giữ danh sách các mốc hết hạn và lấy mốc gần nhất làm timeout của `epoll_wait()`.
- Mọi socket để ở chế độ non-blocking. Mỗi kết nối có buffer nhận và buffer gửi riêng. Socket chưa gửi được hết thì đăng ký thêm `EPOLLOUT` để gửi nốt phần còn lại.

**Lý do chọn:**

| Tiêu chí | epoll + thread pool | Mỗi client 1 luồng (như HW6) | 1 luồng + poll() |
|---|---|---|---|
| Số kết nối | Hàng chục nghìn: epoll chỉ trả về các socket đang có dữ liệu | Mỗi luồng tốn stack và context switch, vài nghìn luồng là nặng | poll() quét toàn bộ danh sách mỗi lần gọi, O(n) |
| Race condition trên state game | Không có, vì chỉ luồng chính đụng vào state | Phải khóa mutex khắp nơi, dễ deadlock, bug khó tái hiện | Không có |
| Việc chậm (hash mật khẩu ~50–100ms, ghi DB) | Đẩy sang pool, luồng chính không bị chặn | Mỗi client tự chịu | Chặn toàn bộ server |
| Đồng hồ lượt/rương | Lấy làm timeout của epoll, không cần luồng thêm | Cần thêm luồng chạy timer, lại phải đồng bộ | Lấy làm timeout của poll |

- Game theo lượt nên mỗi người chơi chỉ gửi vài lệnh mỗi phút, và mỗi lệnh xử lý mất vài micro giây. Một luồng chính dư sức phục vụ rất nhiều trận chạy cùng lúc. Các trận chạy **xen kẽ** nhau: xử lý trọn vẹn một lệnh rồi mới tới lệnh tiếp theo.
- Mutex chỉ còn ở đúng một chỗ là 2 hàng đợi giữa luồng chính và pool, nên dễ kiểm soát.
- **Đường mở rộng sau này:** các trận độc lập với nhau, nên có thể chia trận ra nhiều luồng, mỗi luồng có epoll riêng. Để chuẩn bị cho việc này, game logic được viết dạng hàm thuần, chỉ nhận vào `Match *` và không dùng biến toàn cục.

### 1.2. Định dạng message: JSON (thư viện cJSON)

**Lý do chọn:**
- Trạng thái trận có cấu trúc lồng nhau (2 đội × 3 tàu × vũ khí, giáp, đạn). Với JSON thì biểu diễn trực tiếp được. Với text tách bằng `|` thì phải tự quy ước thứ tự trường, rất dễ lệch giữa client và server.
- Thêm trường mới không làm hỏng code cũ: bên nhận chỉ cần bỏ qua những trường nó không biết.
- Chuỗi chứa ký tự đặc biệt (dấu cách, `|`, `"`, tiếng Việt UTF-8 trong câu hỏi) được JSON tự escape, nên không phải tự viết quy tắc escape.
- cJSON chỉ có 1 file `.c` và 1 file `.h`, giấy phép MIT, nhúng thẳng vào project, không phải cài thư viện.
- Đánh đổi: test bằng `nc` phải gõ JSON dài. Khắc phục bằng cách lưu sẵn kịch bản test thành file, hoặc dùng tool test nhỏ.

### 1.3. Truyền dòng (framing): mỗi message 1 dòng, kết thúc bằng `\r\n`

**Lý do chọn:**
- TCP là luồng byte: một lần `recv()` có thể nhận được nửa message, hoặc nhiều message dính vào nhau. Vì vậy cần một dấu hiệu để biết message kết thúc ở đâu.
- JSON được in ra bằng `cJSON_PrintUnformatted()` nên luôn nằm trên 1 dòng. Nếu dữ liệu chứa ký tự xuống dòng thì JSON tự escape thành `\n`. Do đó ký tự `\n` thật **chỉ xuất hiện ở cuối message**.
- Dùng lại được cơ chế buffer nhận của `stream.c` (HW6), chỉ cần sửa lại cho phù hợp với socket non-blocking.
- Phương án đã cân nhắc nhưng không chọn: thêm 4 byte độ dài ở đầu message. Cách này cũng đúng, nhưng khó debug hơn (không đọc được bằng mắt) mà không đem lại lợi ích gì thêm khi payload là JSON 1 dòng.

### 1.4. Đồng bộ trạng thái: sự kiện kèm toàn bộ trạng thái

**Lý do chọn:**
- Sự kiện (ví dụ "A1 bắn B2, gây 900 sát thương") cho client biết **chuyện gì vừa xảy ra**, để hiện thông báo và hiệu ứng.
- Toàn bộ trạng thái (snapshot) cho client biết **trạng thái hiện tại đúng là gì**, để vẽ lại giao diện. Client không bao giờ tự tính toán HP hay đạn, nên không thể bị lệch dữ liệu so với server.
- Một trận có tối đa 6 tàu, snapshot chỉ khoảng 1–2 KB, gửi lại mỗi khi có thay đổi vẫn rất rẻ.

### 1.5. Client: gắn socket vào main loop của GTK

**Lý do chọn:**
- Dùng `g_io_add_watch()` để GTK gọi callback mỗi khi socket có dữ liệu. Toàn bộ client chạy trên **1 luồng**.
- GTK không an toàn khi gọi từ nhiều luồng. Có luồng mạng riêng thì phải chuyển mọi cập nhật giao diện về luồng UI qua `g_idle_add`, dễ sinh lỗi.

### 1.6. Lưu trữ: SQLite, qua một lớp `storage`

**Lý do chọn:**
- Chỉ server truy cập DB, client không bao giờ đụng vào. Vì vậy không cần một DB server riêng cho nhiều máy kết nối tới.
- Vẫn là SQL đầy đủ: bảng, JOIN, transaction, foreign key.
- Cả DB chỉ là 1 file, chỉ cần `libsqlite3-dev`, không phải cài và chạy server DB lúc nộp hay chấm bài.
- Mở ở chế độ WAL để việc đọc không bị chặn bởi việc ghi. Mọi truy vấn chạy trên thread pool, không chạy trên luồng chính.
- Code gọi qua lớp `storage.h`. Ban đầu cài lớp này bằng RAM để làm phần mạng và gameplay trước, sau đó thay bằng SQLite mà không phải sửa code ở chỗ khác.

---

## 2. Cấu trúc message

### 2.1. Framing
- Mỗi message là **1 object JSON trên 1 dòng**, kết thúc bằng `\r\n`. Bên nhận chấp nhận cả `\n` đơn.
- Độ dài tối đa: **8192 byte**, tính cả `\r\n`.
  - Message dài quá mức này: server trả lỗi `2004` rồi đóng kết nối.
  - Dòng không phải JSON hợp lệ: server trả lỗi `2001`, giữ kết nối.
- Chuỗi mã hóa UTF-8.

### 2.2. Ba loại message

**Request** (client → server):
```json
{"id": 17, "cmd": "TEAM_CREATE", "name": "Rong Bay"}
```
- `id`: số nguyên do client tự tăng. Server trả lại đúng `id` này trong response, để client biết response nào ứng với request nào.
- `cmd`: tên lệnh. Các tham số của lệnh nằm cùng cấp với `id` và `cmd`.

**Response** (server → client, trả lời đúng 1 request):
```json
{"type": "res", "id": 17, "code": 1000, "msg": "OK", "data": {"team_id": 5}}
```
- `code`: mã kết quả (xem mục 3). `1000` là thành công.
- `msg`: mô tả ngắn để hiển thị hoặc debug.
- `data`: không bắt buộc, chứa kết quả trả về.

**Event** (server → client, server tự gửi, không gắn với request nào):
```json
{"type": "evt", "evt": "INVITE", "data": {"team_id": 5, "team_name": "Rong Bay", "from": "son"}}
```

### 2.3. Kiểm tra quyền
Với mỗi request, server kiểm tra theo đúng thứ tự sau và trả lỗi ngay ở bước đầu tiên không đạt:

1. Định dạng message hợp lệ, lệnh tồn tại.
2. Đã đăng nhập chưa (trừ `REGISTER`, `LOGIN`, `PING`).
3. Role có đủ quyền không (admin, đội trưởng, thành viên).
4. Trạng thái có cho phép không (đang trong trận, đang mở rương, chưa tới lượt, ...).
5. Tham số có hợp lệ không.

---

## 3. Mã kết quả

| Mã | Tên | Ý nghĩa |
|---|---|---|
| **1000** | OK | Thành công |
| **2001** | BAD_FORMAT | Không phải JSON hợp lệ |
| 2002 | UNKNOWN_CMD | Lệnh không tồn tại |
| 2003 | BAD_PARAM | Thiếu tham số hoặc sai kiểu |
| 2004 | MSG_TOO_LONG | Message dài quá 8192 byte |
| **3001** | NOT_LOGGED_IN | Chưa đăng nhập |
| 3002 | ALREADY_LOGGED_IN | Kết nối này đã đăng nhập rồi |
| 3003 | WRONG_CREDENTIALS | Sai tài khoản hoặc mật khẩu |
| 3004 | ACCOUNT_LOCKED | Tài khoản bị khóa |
| 3005 | USERNAME_TAKEN | Tên đăng nhập đã tồn tại |
| 3006 | INVALID_USERNAME | Tên đăng nhập không hợp lệ |
| 3007 | INVALID_PASSWORD | Mật khẩu không đạt yêu cầu |
| 3008 | ONLINE_ELSEWHERE | Tài khoản đang đăng nhập ở nơi khác |
| **4001** | FORBIDDEN | Không đủ quyền |
| 4002 | NOT_CAPTAIN | Chỉ đội trưởng được làm |
| 4003 | NOT_ADMIN | Chỉ admin được làm |
| **5001** | TEAM_NOT_FOUND | Không tìm thấy đội |
| 5002 | TEAM_NAME_TAKEN | Tên đội đã tồn tại |
| 5003 | TEAM_FULL | Đội đã đủ 3 người |
| 5004 | ALREADY_IN_TEAM | Người chơi đã ở trong một đội |
| 5005 | NOT_IN_TEAM | Người chơi chưa ở đội nào |
| 5006 | TEAM_IN_MATCH | Đội đang trong sảnh chờ hoặc trong trận, thao tác bị khóa |
| 5007 | USER_NOT_FOUND | Không tìm thấy người chơi |
| 5008 | NOT_A_MEMBER | Người này không ở trong đội |
| 5009 | REQUEST_NOT_FOUND | Không có lời xin vào hoặc lời mời tương ứng |
| 5010 | ALREADY_PENDING | Đã gửi lời xin vào hoặc lời mời trước đó rồi |
| **6001** | TEAM_SIZE_MISMATCH | 2 đội không cùng số người |
| 6002 | CHALLENGE_NOT_FOUND | Không có lời thách đấu tương ứng |
| 6003 | SAME_TEAM | Không thể thách đấu chính đội mình |
| 6004 | NOT_IN_LOBBY | Không ở trong sảnh chờ |
| 6005 | OPPONENT_NOT_READY | Đội khách chưa sẵn sàng |
| 6006 | INVALID_SETTING | Giá trị setting ngoài khoảng cho phép |
| 6007 | ONLY_HOST | Chỉ đội chủ được làm |
| 6008 | ONLY_GUEST | Chỉ đội khách được làm |
| **7001** | NOT_IN_MATCH | Không ở trong trận |
| 7002 | NOT_YOUR_TURN | Chưa tới lượt |
| 7003 | SHIP_DESTROYED | Tàu của mình đã bị hạ |
| 7004 | NOT_ENOUGH_COIN | Không đủ coin |
| 7005 | SLOT_FULL | Hết slot (pháo, tên lửa) |
| 7006 | INVALID_TARGET | Mục tiêu không hợp lệ (đồng đội, đã chết, không tồn tại) |
| 7007 | INVALID_ATTACK | Trộn 2 loại pháo, số lượng sai, hoặc không chọn vũ khí nào |
| 7008 | NO_AMMO | Không đủ đạn để bắn với số lượng đã chọn |
| 7009 | NO_RESERVE | Hết thùng đạn hoặc bộ nguồn để reload |
| 7010 | HP_FULL | HP đã đầy |
| 7011 | NO_CHEST | Lượt này không có rương hoặc rương đã mở |
| 7012 | CHEST_ACTIVE | Đang mở rương, chỉ được trả lời câu hỏi |
| 7013 | CHEST_EXPIRED | Hết giờ trả lời rương |
| **9001** | INTERNAL_ERROR | Lỗi phía server |
| 9002 | SERVER_BUSY | Server quá tải, thử lại sau |

---

## 4. Danh sách lệnh (client → server)

Cột **Quyền**:
- `*`: không cần đăng nhập
- `U`: đã đăng nhập
- `C`: đội trưởng
- `A`: admin
- `P`: đang trong trận, và đúng lượt mình (trừ khi có ghi chú khác)

### 4.1. Chung
| Lệnh | Quyền | Tham số | `data` trả về |
|---|---|---|---|
| `PING` | * | | `{"time": <unix ms>}` |

### 4.2. Tài khoản và phiên
| Lệnh | Quyền | Tham số | `data` trả về |
|---|---|---|---|
| `REGISTER` | * | `username`, `password` | |
| `LOGIN` | * | `username`, `password` | `{"username", "role", "team_id"}` |
| `LOGOUT` | U | | |
| `WHOAMI` | U | | `{"username", "role", "team_id", "is_captain", "state"}` |
| `CHANGE_PASSWORD` | U | `old_password`, `new_password` | |

Quy định:
- **Tên đăng nhập:** 3–20 ký tự, chỉ gồm `a-z`, `A-Z`, `0-9`, `_`.
- **Mật khẩu:** 6–64 ký tự. Lưu dạng **PBKDF2-SHA256 + salt riêng cho từng tài khoản**, không lưu mật khẩu gốc.
- **Mỗi tài khoản chỉ có 1 phiên.** Đăng nhập lại khi đang online ở nơi khác thì bị từ chối với mã `3008`.
- `state` có các giá trị: `IDLE`, `LOBBY`, `IN_MATCH`.
- `LOGOUT` khi đang trong trận: tàu của người đó tính là bị disconnect, nên bị bỏ qua lượt (xem mục 6).

### 4.3. Đội
| Lệnh | Quyền | Tham số | `data` trả về |
|---|---|---|---|
| `TEAM_CREATE` | U | `name` | `{"team_id"}` |
| `TEAM_DELETE` | C, A | `team_id` | |
| `TEAM_LIST` | U | | `{"teams": [{"team_id", "name", "captain", "size", "state"}]}` |
| `TEAM_MEMBERS` | U | `team_id` | `{"members": [{"slot", "username", "is_captain", "online"}]}` |
| `TEAM_JOIN_REQUEST` | U | `team_id` | |
| `TEAM_JOIN_LIST` | C | | `{"requests": ["username", ...]}` |
| `TEAM_JOIN_REPLY` | C | `username`, `accept` (bool) | |
| `TEAM_INVITE` | C | `username` | |
| `TEAM_INVITE_LIST` | U | | `{"invites": [{"team_id", "team_name", "from"}]}` |
| `TEAM_INVITE_REPLY` | U | `team_id`, `accept` (bool) | |
| `TEAM_LEAVE` | U | | |
| `TEAM_KICK` | C, A | `username` | |
| `TEAM_TRANSFER` | C | `username` | |

Quy định:
- **Tên đội:** 1–30 ký tự UTF-8.
- **`TEAM_DELETE` và `TEAM_KICK`:** đội trưởng chỉ làm được với đội mình, admin làm được với mọi đội. Đội ở trạng thái `LOBBY` hoặc `IN_MATCH` thì bị từ chối với mã `5006`, **kể cả admin**.
- Khi một người vào được đội (qua lời xin vào hoặc lời mời):
  - Mọi lời xin vào đội khác và mọi lời mời khác của người đó bị hủy.
  - Nếu đội vừa đủ 3 người thì mọi lời xin vào và lời mời còn treo của đội đó cũng bị hủy.
- Người chơi ngắt kết nối thì lời mời và lời xin vào của họ **vẫn giữ nguyên**. Các lời này lưu trong RAM và mất khi server khởi động lại.

### 4.4. Thách đấu và sảnh chờ
| Lệnh | Quyền | Tham số | `data` trả về |
|---|---|---|---|
| `CHALLENGE_SEND` | C | `team_id` | |
| `CHALLENGE_LIST` | C | | `{"incoming": [...], "outgoing": [...]}` |
| `CHALLENGE_REPLY` | C | `team_id`, `accept` (bool) | `{"lobby_id"}` nếu chấp nhận |
| `CHALLENGE_CANCEL` | C | `team_id` | |
| `LOBBY_SETTINGS` | C, đội chủ | `turn_time`, `start_coin`, `chest_time` | |
| `LOBBY_READY` | C, đội khách | `ready` (bool) | |
| `LOBBY_START` | C, đội chủ | | |
| `LOBBY_LEAVE` | C | | |

Quy định:
- **Khoảng giá trị setting:**
  - `turn_time`: 10–120 giây (mặc định 30).
  - `chest_time`: 10–120 giây (mặc định 30).
  - `start_coin`: 0–100000 (mặc định 0).
- Đội chủ đổi setting thì trạng thái sẵn sàng của đội khách tự bị bỏ, để đội khách phải đồng ý lại với setting mới.
- `LOBBY_LEAVE`: hủy sảnh chờ, cả 2 đội về trạng thái `IDLE`.
- Khi 2 đội vào sảnh chờ, mọi lời thách đấu khác của 2 đội đó bị hủy.
- Thành viên trong đội nhận được event của sảnh chờ nhưng không gửi được lệnh sảnh chờ.

### 4.5. Trong trận
| Lệnh | Quyền | Tham số |
|---|---|---|
| `GAME_STATE` | trong trận, bất kỳ lúc nào | (trả về snapshot mới nhất trong `data`) |
| `BUY` | P | `item`, `qty` |
| `REPAIR` | P | `hp` |
| `RELOAD` | trong trận, **bất kỳ lúc nào** | `weapon` (`"AUTO"` / `"LASER"`), `index` (số thứ tự khẩu laze, bắt đầu từ 0) |
| `CHEST_OPEN` | P | |
| `CHEST_ANSWER` | P, đang mở rương | `choice` (`"A"` / `"B"` / `"C"` / `"D"`) |
| `ATTACK` | P | `target` (username), `gun`, `missiles` |

**Các giá trị `item` của `BUY`:**

| `item` | Giá | Hiệu ứng |
|---|---|---|
| `AMMO_BOX` | 100 | +1 thùng đạn (50 viên) vào kho của pháo tự động |
| `LASER` | 1000 | +1 pháo laze, băng rỗng (0/10). Tối đa 3 khẩu |
| `POWER_PACK` | 100 | +1 bộ nguồn (10 phát) vào kho chung của laze |
| `MISSILE` | 2000 | +1 tên lửa. Tối đa 4 quả |
| `ARMOR_BASIC` | 1000 | +1 lớp giáp cơ bản, 500 Amor |
| `ARMOR_ADVANCED` | 2000 | +1 lớp giáp tăng cường, 1500 Amor |

- Mua thì kiểm tra theo kiểu **tất cả hoặc không**: không đủ coin hoặc không đủ slot cho toàn bộ `qty` thì từ chối cả lệnh.
- `ARMOR_*` chỉ được mua với `qty = 1`.

**`REPAIR`:** `hp` là số HP muốn hồi. Nếu `hp` vượt quá phần HP còn thiếu so với 1000 thì chỉ hồi tới 1000 và chỉ trừ coin cho phần thực sự hồi.

**`ATTACK`:**
```json
{"id": 40, "cmd": "ATTACK", "target": "minh",
 "gun": {"type": "LASER", "count": 2}, "missiles": 1}
```
- `gun` không bắt buộc. Nếu có thì `type` là `"AUTO"` hoặc `"LASER"`, và `count` là số khẩu bắn (với `AUTO` thì `count` luôn bằng 1).
- `missiles` không bắt buộc, mặc định 0.
- Phải có ít nhất 1 trong 2 phần `gun` và `missiles`.
- **Chọn khẩu laze nào bắn:** server tự chọn `count` khẩu đang còn đạn, ưu tiên khẩu có số thứ tự nhỏ hơn.
- Bắn thành công thì lượt kết thúc ngay.

### 4.6. Admin
| Lệnh | Quyền | Tham số | `data` trả về |
|---|---|---|---|
| `ADMIN_USER_LIST` | A | | `{"users": [{"username", "role", "locked", "online"}]}` |
| `ADMIN_LOCK` | A | `username`, `locked` (bool) | |
| `ADMIN_LOG` | A | `limit` (mặc định 100) | `{"lines": [...]}` |
| `ADMIN_QUESTION_LIST` | A | | `{"questions": [...]}` |
| `ADMIN_QUESTION_ADD` | A | `tier`, `question`, `choices` (4 chuỗi), `answer` | `{"question_id"}` |
| `ADMIN_QUESTION_DELETE` | A | `question_id` | |

- `tier`: `"BRONZE"`, `"SILVER"` hoặc `"GOLD"`.
- `ADMIN_LOCK` với người đang online: server gửi event `FORCE_LOGOUT` rồi ngắt phiên. Nếu người đó đang trong trận thì tàu tính là bị hạ (HP = 0).
- Admin không khóa được chính mình hoặc admin khác.

---

## 5. Danh sách event (server → client)

### 5.1. Phiên
| Event | Người nhận | `data` |
|---|---|---|
| `FORCE_LOGOUT` | Người bị khóa | `{"reason"}` |

### 5.2. Đội
| Event | Người nhận | `data` |
|---|---|---|
| `JOIN_REQUEST` | Đội trưởng | `{"username"}` |
| `JOIN_RESULT` | Người xin vào | `{"team_id", "accepted"}` |
| `INVITE` | Người được mời | `{"team_id", "team_name", "from"}` |
| `INVITE_RESULT` | Đội trưởng | `{"username", "accepted"}` |
| `TEAM_UPDATE` | Mọi thành viên trong đội | `{"team_id", "members": [...]}` (gửi khi có người vào, rời, bị kick, hoặc đổi đội trưởng) |
| `KICKED` | Người bị kick | `{"team_id", "by"}` |
| `TEAM_DELETED` | Mọi thành viên trong đội | `{"team_id", "by"}` |

### 5.3. Thách đấu và sảnh chờ
| Event | Người nhận | `data` |
|---|---|---|
| `CHALLENGE` | Đội trưởng đội bị thách đấu | `{"team_id", "team_name"}` |
| `CHALLENGE_RESULT` | Đội trưởng đội gửi thách đấu | `{"team_id", "accepted"}` |
| `CHALLENGE_CANCELLED` | Đội trưởng đội bị thách đấu | `{"team_id"}` |
| `LOBBY_UPDATE` | Mọi người trong sảnh chờ | `{"lobby_id", "host", "guest", "settings", "guest_ready"}` |
| `LOBBY_CLOSED` | Mọi người trong sảnh chờ | `{"reason"}` |

### 5.4. Trong trận
Mọi event trong trận đều kèm trường `state`, là snapshot mới nhất (xem mục 7), **trừ** `CHEST_QUESTION` và `CHEST_DROP`.

| Event | Người nhận | `data` |
|---|---|---|
| `MATCH_START` | Mọi người trong trận | `{"match_id", "settings", "state"}` |
| `TURN_START` | Mọi người trong trận | `{"player", "deadline_ms", "state"}` |
| `CHEST_DROP` | Mọi người trong trận | `{"player", "tier"}` |
| `CHEST_QUESTION` | Người đang tới lượt và **đồng đội** | `{"tier", "question", "choices", "deadline_ms", "can_answer"}` |
| `CHEST_RESULT` | Đội của người đang tới lượt | `{"correct", "answer", "coin", "state"}` (khi trả lời sai hoặc hết giờ thì `answer` là đáp án đúng) |
| `PURCHASE` | Mọi người trong trận | `{"player", "item", "qty", "state"}` |
| `REPAIR` | Mọi người trong trận | `{"player", "hp", "state"}` |
| `RELOAD` | Mọi người trong trận | `{"player", "weapon", "index", "state"}` |
| `ATTACK` | Mọi người trong trận | `{"attacker", "target", "gun", "missiles", "damage", "armor_lost", "hp_lost", "destroyed", "state"}` |
| `TURN_SKIPPED` | Mọi người trong trận | `{"player", "reason"}` (`reason` là `"TIMEOUT"` hoặc `"DISCONNECTED"`) |
| `PLAYER_DISCONNECTED` | Mọi người trong trận | `{"player"}` |
| `PLAYER_RECONNECTED` | Mọi người trong trận | `{"player"}` |
| `SHIP_DESTROYED` | Mọi người trong trận | `{"player", "reason"}` (`reason` là `"ATTACK"` hoặc `"LOCKED"`) |
| `MATCH_END` | Mọi người trong trận | `{"winner_team_id", "loser_team_id", "state"}` |

- **`can_answer`:** `true` với người đang tới lượt, `false` với đồng đội (chỉ được xem).
- **`deadline_ms`:** thời điểm hết hạn, tính theo đồng hồ server (unix ms). Client tự tính thời gian còn lại bằng cách so với `time` nhận được từ `PING`.
- **Đội đối phương** chỉ nhận `CHEST_DROP` (biết là có rương, rương loại gì) và `CHEST_RESULT` dạng rút gọn: chỉ có `correct` và `coin`, không có câu hỏi, không có đáp án.

---

## 6. Vòng đời của một lượt

1. Server chọn người tiếp theo theo thứ tự A0 → B0 → A1 → B1 → A2 → B2 (theo slot), bỏ qua tàu đã chết.
2. Gửi `TURN_START`. Đồng hồ lượt bắt đầu chạy.
3. Thả rương: gửi `CHEST_DROP`. Tỉ lệ: đồng 60%, bạc 30%, vàng 10%.
4. Người chơi làm theo thứ tự bất kỳ: `BUY`, `REPAIR`, `RELOAD`, `CHEST_OPEN`.
5. **Nếu mở rương:**
   - Đồng hồ lượt **tạm dừng**, đồng hồ rương bắt đầu chạy.
   - Server gửi `CHEST_QUESTION` cho đội của người đang tới lượt.
   - Mọi lệnh khác của người đó đều bị từ chối với mã `7012`, **kể cả `RELOAD`**.
   - Khi có `CHEST_ANSWER` hoặc hết giờ rương: gửi `CHEST_RESULT`, đồng hồ lượt chạy tiếp.
6. **`ATTACK` thành công:** gửi event `ATTACK`, kiểm tra kết thúc trận, rồi sang lượt tiếp theo.
7. **Hết giờ lượt:** gửi `TURN_SKIPPED` (`TIMEOUT`), sang lượt tiếp theo.
8. **Người đang tới lượt bị disconnect:** bị bỏ qua lượt ngay (`TURN_SKIPPED` với `DISCONNECTED`).
   - Ở các lượt sau, nếu người đó vẫn chưa kết nối lại thì cũng bị bỏ qua ngay, không phải chờ hết giờ.
   - Người đó **đăng nhập lại được trong lúc trận còn diễn ra**. Đăng nhập lại thì được đưa về trận (nhận `GAME_STATE`), và mọi người nhận `PLAYER_RECONNECTED`.
9. Rương chưa mở mà hết lượt thì rương biến mất.

**Kết thúc trận:** một đội không còn tàu nào sống thì gửi `MATCH_END`. Cả 2 đội về trạng thái `IDLE`, các đội được mở khóa thao tác. Kết quả trận được ghi vào log.

---

## 7. Snapshot trạng thái trận

Mỗi người nhận một snapshot **riêng**:
- Với tàu của mình và tàu của đồng đội: thấy đầy đủ mọi thông tin.
- Với tàu đối phương: chỉ thấy HP, giáp, số khẩu laze và số tên lửa. **Coin và đạn được giấu đi.**

```json
{
  "match_id": 3,
  "turn": {"player": "son", "deadline_ms": 1762862400000, "paused": false},
  "chest": {"tier": "GOLD", "opened": false},
  "teams": [
    {
      "team_id": 5, "name": "Rong Bay",
      "ships": [
        {
          "username": "son", "slot": 0, "online": true, "alive": true,
          "hp": 820,
          "armor": [
            {"type": "BASIC", "amor": 500},
            {"type": "ADVANCED", "amor": 1200}
          ],
          "coin": 1500,
          "auto": {"mag": 37, "boxes": 3},
          "lasers": [{"mag": 7}, {"mag": 10}],
          "power_packs": 2,
          "missiles": 2
        }
      ]
    },
    {
      "team_id": 8, "name": "Sao Bang",
      "ships": [
        {
          "username": "minh", "slot": 0, "online": true, "alive": true,
          "hp": 1000,
          "armor": [{"type": "BASIC", "amor": 500}],
          "laser_count": 1, "missiles": 0
        }
      ]
    }
  ]
}
```

- **`armor`:** thứ tự từ lớp trong ra lớp ngoài. Phần tử cuối là lớp mua sau cùng, cũng là lớp bị trừ trước khi trúng đạn.
- **`chest`:** bằng `null` nếu lượt này không còn rương.

---

## 8. Ví dụ một phiên

```
C: {"id":1,"cmd":"LOGIN","username":"son","password":"123456"}
S: {"type":"res","id":1,"code":1000,"msg":"OK","data":{"username":"son","role":"PLAYER","team_id":null}}
C: {"id":2,"cmd":"TEAM_CREATE","name":"Rong Bay"}
S: {"type":"res","id":2,"code":1000,"msg":"OK","data":{"team_id":5}}
C: {"id":3,"cmd":"TEAM_INVITE","username":"lan"}
S: {"type":"res","id":3,"code":1000,"msg":"OK"}
        ... (lan chấp nhận lời mời) ...
S: {"type":"evt","evt":"INVITE_RESULT","data":{"username":"lan","accepted":true}}
S: {"type":"evt","evt":"TEAM_UPDATE","data":{"team_id":5,"members":[...]}}
C: {"id":4,"cmd":"CHALLENGE_SEND","team_id":8}
S: {"type":"res","id":4,"code":1000,"msg":"OK"}
        ... (đội 8 chấp nhận và bấm sẵn sàng) ...
C: {"id":5,"cmd":"LOBBY_START"}
S: {"type":"res","id":5,"code":1000,"msg":"OK"}
S: {"type":"evt","evt":"MATCH_START","data":{...}}
S: {"type":"evt","evt":"TURN_START","data":{"player":"son",...}}
S: {"type":"evt","evt":"CHEST_DROP","data":{"player":"son","tier":"SILVER"}}
C: {"id":6,"cmd":"CHEST_OPEN"}
S: {"type":"res","id":6,"code":1000,"msg":"OK"}
S: {"type":"evt","evt":"CHEST_QUESTION","data":{"tier":"SILVER","question":"...","choices":["...","...","...","..."],"can_answer":true,...}}
C: {"id":7,"cmd":"CHEST_ANSWER","choice":"B"}
S: {"type":"res","id":7,"code":1000,"msg":"OK"}
S: {"type":"evt","evt":"CHEST_RESULT","data":{"correct":true,"coin":500,...}}
C: {"id":8,"cmd":"ATTACK","target":"minh","gun":{"type":"AUTO","count":1}}
S: {"type":"res","id":8,"code":1000,"msg":"OK"}
S: {"type":"evt","evt":"ATTACK","data":{"attacker":"son","target":"minh","damage":10,...}}
S: {"type":"evt","evt":"TURN_START","data":{"player":"minh",...}}
```
