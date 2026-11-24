# Design: Đặc tả các luồng client–server

Tài liệu này mô tả **từng luồng**: luồng bắt đầu khi nào, cần điều kiện gì, diễn ra theo các bước nào và kết thúc ra sao. Mỗi luồng có một file sơ đồ riêng trong [diagrams/](diagrams/).

| Tài liệu | Nội dung |
|---|---|
| [SPEC.md](SPEC.md) | Luật chơi |
| [PROTOCOL.md](PROTOCOL.md) | Kiến trúc và lý do chọn, định dạng message, mã lỗi, lệnh, event, quy tắc và ràng buộc |
| **DESIGN.md** (file này) | Đặc tả các luồng |
| [DATABASE.md](DATABASE.md) | Lược đồ SQLite, đồng bộ RAM và DB, tạo admin, câu hỏi soạn sẵn |
| [SERVER.md](SERVER.md) | Module, struct, cấu hình, log, build |
| [CLIENT.md](CLIENT.md) | Module, tầng mạng, màn hình |
| [diagrams/](diagrams/) | Sơ đồ kiến trúc, sequence diagram, state diagram, ER |

Quy ước:
- **Điều kiện** là những gì phải đúng thì luồng mới chạy. Không đạt thì server trả mã lỗi ghi ở cột tương ứng.
- **Kết quả** là trạng thái sau khi luồng kết thúc.
- Tên lệnh, event và mã lỗi đều theo PROTOCOL.md.

---

## 1. Các thành phần tham gia

Sơ đồ: [architecture.md](diagrams/architecture.md)

- **Client** (C++ + Qt/QML, socket bằng C, 1 luồng): Giao diện QML, Net module, Client state.
- **Server, luồng chính** (epoll): Event loop, Connection, Dispatcher, Session manager, Team manager, Challenge và Lobby, Match engine, Timer.
- **Server, thread pool**: các Worker làm việc chậm (PBKDF2, đọc SQLite, đọc file log).
- **Server, DB writer**: 1 luồng ghi SQLite và ghi log theo FIFO.
- **Lưu trữ**: SQLite và file log.

Trách nhiệm chi tiết của từng thành phần xem PROTOCOL.md mục 1.7.

---

## 2. Luồng thành công

### 2.1. Đăng nhập

Sơ đồ: [sequence/01-login.md](diagrams/sequence/01-login.md)

| | |
|---|---|
| **Kích hoạt** | Client gửi `LOGIN` |
| **Điều kiện** | Kết nối chưa đăng nhập (`3002`) |
| **Kết quả** | Kết nối gắn với tài khoản. Tài khoản được đánh dấu online |

1. Luồng chính kiểm tra định dạng và trạng thái kết nối.
2. Luồng chính đẩy job `VERIFY_LOGIN` sang thread pool, rồi **quay lại phục vụ các kết nối khác ngay**. Kết nối này chuyển sang `AUTH_PENDING`. Các request sau của nó phải chờ trong buffer.
3. Worker đọc hash, salt, role và trạng thái khóa từ SQLite, sau đó chạy PBKDF2 (khoảng 50–100 ms).
4. Worker đẩy kết quả vào hàng đợi kết quả và đánh thức luồng chính qua `eventfd`.
5. Luồng chính xử lý tiếp:
   - Nếu tài khoản đang có phiên ở kết nối khác thì thay thế phiên đó (xem 5.2).
   - Tạo phiên mới, đánh dấu tài khoản online.
6. Server trả response `1000` với `data` gồm `user_id`, `username`, `role`, `team_id`.
7. Nếu tài khoản đang có trận dở thì server gửi thêm event `MATCH_RESUME` ngay sau response (xem 5.1). Client dựa vào event này để vào thẳng màn trận, không cần trường `state` trong response.
8. Server đẩy dòng log vào hàng đợi của DB writer.

### 2.2. Xin vào đội và được duyệt

Sơ đồ: [sequence/02-join-team.md](diagrams/sequence/02-join-team.md)

| | |
|---|---|
| **Kích hoạt** | Người chơi X gửi `TEAM_JOIN_REQUEST` |
| **Điều kiện** | X chưa có đội (`5004`). Đội tồn tại (`5001`), chưa đủ 3 người (`5003`), đang `IDLE` (`5006`). X chưa gửi lời xin vào đội này trước đó (`5010`) |
| **Kết quả** | X vào đội, nhận slot trống có số nhỏ nhất |

1. Server lưu lời xin vào trong RAM, trả `1000` cho X.
2. Server gửi `JOIN_REQUEST` cho đội trưởng.
3. Đội trưởng gửi `TEAM_JOIN_REPLY` với `accept: true`. Server kiểm tra lại toàn bộ điều kiện, vì đội có thể đã đổi trạng thái trong lúc chờ.
4. Server gán X vào slot trống có số nhỏ nhất, rồi hủy:
   - mọi lời xin vào và lời mời khác của X,
   - mọi lời treo của đội nếu đội vừa đủ 3 người.
5. Server cập nhật đội trong RAM và trả `1000` cho đội trưởng. Đội là party chỉ nằm trong RAM nên không ghi DB.
6. Server gửi `JOIN_RESULT` cho X, rồi gửi `TEAM_UPDATE` cho cả đội (kể cả X).

Luồng **mời vào đội** đi theo các bước tương tự nhưng đổi vai: đội trưởng gọi `PLAYER_LIST` để lấy danh sách người đang online và chưa có đội, chọn một người rồi gửi `TEAM_INVITE` với `user_id`; người được mời nhận event `INVITE` và trả lời bằng `TEAM_INVITE_REPLY`.

### 2.3. Thách đấu, sảnh chờ, bắt đầu trận

Sơ đồ: [sequence/03-challenge-lobby-start.md](diagrams/sequence/03-challenge-lobby-start.md)

| | |
|---|---|
| **Kích hoạt** | Đội trưởng đội chủ (H) gửi `CHALLENGE_SEND` |
| **Điều kiện** | H là đội trưởng (`4002`). Không tự thách đấu đội mình (`6003`). 2 đội cùng số người (`6001`). Cả 2 đội đang `IDLE` (`5006`) |
| **Kết quả** | Trận bắt đầu. Cả 2 đội ở `IN_MATCH` |

1. Server gửi `CHALLENGE` cho đội trưởng đội khách (G).
2. G gửi `CHALLENGE_REPLY` với `accept: true`. Server tạo sảnh chờ với setting mặc định, chuyển 2 đội sang `LOBBY`, hủy mọi lời thách đấu khác của 2 đội.
3. Server gửi `CHALLENGE_RESULT` cho H, và `LOBBY_UPDATE` cho **mọi thành viên** của 2 đội.
4. H có thể đổi setting bằng `LOBBY_SETTINGS`. Mỗi lần đổi, trạng thái sẵn sàng của đội khách bị bỏ, và server gửi `LOBBY_UPDATE`.
5. G gửi `LOBBY_READY` với `ready: true`. Server gửi `LOBBY_UPDATE` với `guest_ready: true`.
6. H gửi `LOBBY_START`. Nếu đội khách chưa sẵn sàng thì server trả `6005`.
7. Server tạo trận: mỗi tàu nhận HP 1000, 1 pháo tự động với băng 50 viên, và coin theo setting. Hai đội chuyển sang `IN_MATCH`.
8. Server gửi `MATCH_START` cho mọi người trong trận, rồi bắt đầu lượt đầu tiên của A0 (xem 2.4).

### 2.4. Một lượt

Sơ đồ: [sequence/04-full-turn.md](diagrams/sequence/04-full-turn.md)

| | |
|---|---|
| **Kích hoạt** | Lượt trước kết thúc, hoặc trận vừa bắt đầu |
| **Điều kiện** | Người chơi được chọn còn sống |
| **Kết quả** | Chuyển sang người chơi tiếp theo, hoặc trận kết thúc |

1. **Chọn người chơi:** server chọn người tiếp theo theo thứ tự A0 → B0 → A1 → B1 → A2 → B2, bỏ qua tàu đã chết. Nếu người đó đang offline thì bỏ qua lượt luôn (xem 4.4).
2. **Bắt đầu lượt:** server gửi `TURN_START` kèm `deadline_ms`, rồi đặt đồng hồ lượt vào Timer.
3. **Thả rương:** server quay ngẫu nhiên loại rương (đồng 60%, bạc 30%, vàng 10%) và gửi `CHEST_DROP` cho mọi người trong trận.
4. **Hành động tự do:** người chơi làm theo thứ tự bất kỳ, bao nhiêu lần cũng được:
   - `BUY`, `REPAIR`: server trả response, rồi gửi `MATCH_UPDATE` (`action` là `PURCHASE` / `REPAIR`) cho cả trận. Đội mình nhận snapshot đầy đủ, đối phương nhận snapshot đã giấu coin và đạn.
   - `RELOAD`: tương tự. Lệnh này làm được cả ngoài lượt.
5. **Mở rương** (không bắt buộc):
   1. Người chơi gửi `CHEST_OPEN`.
   2. Server **dừng đồng hồ lượt**, ghi lại thời gian còn lại, rồi đặt đồng hồ rương.
   3. Server gửi `CHEST_QUESTION` cho người đang tới lượt (`can_answer: true`) và đồng đội (`can_answer: false`). Đối phương không nhận được câu hỏi.
   4. Từ lúc này mọi lệnh khác của người chơi đều bị từ chối với mã `7012`, kể cả `RELOAD`.
   5. Người chơi gửi `CHEST_ANSWER`. Đúng thì cộng coin, sai thì mất rương. Chỉ được trả lời 1 lần.
   6. Server gửi `CHEST_RESULT`:
      - đội mình nhận bản đầy đủ, có đáp án đúng nếu trả lời sai;
      - đối phương nhận bản rút gọn, chỉ có `correct` và `coin`.
   7. Server **chạy lại đồng hồ lượt** với thời gian còn lại.
6. **Tấn công:**
   1. Người chơi gửi `ATTACK`.
   2. Server kiểm tra: mục tiêu là đối phương còn sống, không trộn 2 loại pháo, các chỗ được chọn đã lắp pháo và còn đạn, số tên lửa không vượt quá số đang có.
   3. Server tính tổng Dam, trừ Amor từ lớp giáp ngoài cùng vào trong, phần dư trừ vào HP.
   4. Server trả `1000`, rồi gửi `MATCH_UPDATE` (`action: ATTACK`) cho cả trận.
   5. Lượt kết thúc. Nếu có tàu bị hạ thì kiểm tra kết thúc trận (xem 2.5), nếu không thì quay lại bước 1.
7. Rương chưa mở mà lượt đã kết thúc thì rương biến mất.

### 2.5. Kết thúc trận

Sơ đồ: [sequence/05-match-end.md](diagrams/sequence/05-match-end.md)

| | |
|---|---|
| **Kích hoạt** | Có tàu bị hạ (do tấn công hoặc do bị khóa tài khoản), hoặc một đội bỏ cuộc (xem 4.4) |
| **Điều kiện** | Một đội không còn tàu nào sống, hoặc toàn đội offline quá 120 giây |
| **Kết quả** | Trận bị hủy. Cả 2 đội quay về room (`IDLE`) và được mở khóa thao tác |

1. Server gửi `MATCH_UPDATE` (`action: SHIP_DESTROYED`) cho tàu vừa bị hạ.
2. Server gửi `MATCH_END` với `reason` là `ELIMINATED` (bị hạ hết tàu) hoặc `FORFEIT` (bỏ cuộc).
3. Server hủy trận và mọi đồng hồ của trận đó, rồi chuyển 2 đội về room (`IDLE`). Thành viên vẫn còn offline bị tự rời đội, đồng đội nhận `TEAM_UPDATE`.
4. Server đẩy dòng log kết quả trận vào hàng đợi của DB writer. Kết quả trận không lưu vào DB.

---

## 3. Luồng lỗi

Nguyên tắc chung cho mọi luồng lỗi:
- Lệnh bị từ chối **không làm thay đổi gì**: không trừ tài nguyên, không tốn lượt, không dừng đồng hồ.
- Server kiểm tra theo thứ tự cố định (PROTOCOL.md mục 9.3) và trả lỗi ở bước đầu tiên không đạt.

### 3.1. Lỗi đăng nhập

Sơ đồ: [sequence/10-error-login.md](diagrams/sequence/10-error-login.md)

| Tình huống | Mã | Kết nối |
|---|---|---|
| Sai username hoặc sai mật khẩu | `3003` | Giữ nguyên, được thử lại |
| Mật khẩu đúng nhưng tài khoản bị khóa | `3004` | Giữ nguyên |

- Sai username và sai mật khẩu trả về **cùng một mã**, để không lộ username nào có tồn tại.
- Mã `3004` chỉ trả về khi mật khẩu đúng, cũng vì lý do trên.

### 3.2. Lỗi định dạng và lỗi chưa đăng nhập

Sơ đồ: [sequence/11-error-format.md](diagrams/sequence/11-error-format.md)

| Tình huống | Mã | Kết nối |
|---|---|---|
| Dòng nhận được không phải JSON hợp lệ | `2001` | Giữ nguyên |
| Lệnh không tồn tại | `2002` | Giữ nguyên |
| Chưa đăng nhập mà gửi lệnh cần đăng nhập | `3001` | Giữ nguyên |
| Message dài quá 8192 byte | `2004` | **Đóng**, vì không còn xác định được message tiếp theo bắt đầu ở đâu |

Kiểm tra đăng nhập đứng trước kiểm tra tham số. Vì vậy gửi `TEAM_CREATE` thiếu `name` khi chưa đăng nhập thì nhận `3001`, không phải `2003`.

### 3.3. Lỗi luật chơi trong trận

Sơ đồ: [sequence/12-error-game-rules.md](diagrams/sequence/12-error-game-rules.md)

| Tình huống | Mã |
|---|---|
| Hành động khi chưa tới lượt (trừ `RELOAD`) | `7002` |
| Trộn pháo tự động và laze trong một lần bắn | `7007` |
| Bắn đồng đội, tàu đã chết, hoặc người không có trong trận | `7006` |
| Không đủ coin | `7004` |
| Hết chỗ lắp pháo hoặc đã đủ tên lửa | `7005` |
| Reload hoặc bắn bằng chỗ chưa lắp pháo | `7014` |
| Làm việc khác khi đang mở rương | `7012` |

### 3.4. Lỗi phân quyền

Sơ đồ: [sequence/13-error-permission.md](diagrams/sequence/13-error-permission.md)

| Tình huống | Mã |
|---|---|
| Thành viên thường dùng lệnh của đội trưởng | `4002` |
| Người không phải admin dùng lệnh admin | `4003` |
| Đội trưởng xóa đội khác | `4001` |
| Admin khóa chính mình hoặc admin khác | `4001` |
| Bất kỳ ai (**kể cả admin**) xóa đội hoặc kick người khi đội đang ở sảnh chờ hoặc trong trận | `5006` |

---

## 4. Luồng timeout

Mọi đồng hồ đều nằm trong Timer của luồng chính. Timer lấy mốc hết hạn gần nhất làm timeout cho `epoll_wait`, nên không cần `sleep` hay luồng riêng.

### 4.1. Hết giờ lượt

Sơ đồ: [sequence/20-timeout-turn.md](diagrams/sequence/20-timeout-turn.md)

1. `epoll_wait` trả về vì hết timeout. Timer báo lượt hiện tại đã hết hạn.
2. Server gửi `MATCH_UPDATE` (`action: TURN_SKIPPED`, `reason: TIMEOUT`) cho cả trận. Rương chưa mở thì biến mất.
3. Server sang lượt tiếp theo (xem 2.4).
4. Request đến server **sau** mốc hết hạn thì bị coi là ngoài lượt (`7002`), kể cả khi người dùng đã bấm nút trước mốc đó. Server chỉ căn theo đồng hồ của chính nó.

### 4.2. Hết giờ rương

Sơ đồ: [sequence/21-timeout-chest.md](diagrams/sequence/21-timeout-chest.md)

1. Hết giờ rương mà chưa có `CHEST_ANSWER`.
2. Server coi như trả lời sai và gửi `CHEST_RESULT` với `correct: false`, `coin: 0` và đáp án đúng.
3. Server chạy lại đồng hồ lượt với thời gian còn lại lúc mở rương.
4. Người chơi vẫn được mua, sửa tàu và tấn công trong thời gian còn lại đó.

### 4.3. Kết nối im lặng quá lâu

Sơ đồ: [sequence/22-timeout-idle.md](diagrams/sequence/22-timeout-idle.md)

1. Client gửi `PING` mỗi 20 giây.
2. Mỗi khi nhận được bất kỳ byte nào, server cập nhật thời điểm nhận gần nhất của kết nối đó.
3. Kết nối nào 60 giây không có byte nào thì server đóng, rồi xử lý như mất kết nối (xem 5.1 hoặc 5.3).

Luồng này phát hiện được các trường hợp mạng chết mà không có gói FIN hay RST, ví dụ rút dây mạng.

### 4.4. Cả đội offline quá lâu (bỏ cuộc)

Sơ đồ: [sequence/23-timeout-forfeit.md](diagrams/sequence/23-timeout-forfeit.md)

1. Người cuối cùng còn online của một đội mất kết nối. Server đặt đồng hồ bỏ cuộc 120 giây cho đội đó.
2. Trong thời gian này, tới lượt của thành viên đội đó thì server bỏ qua ngay (`MATCH_UPDATE` với `action: TURN_SKIPPED`, `reason: DISCONNECTED`).
3. Nếu có thành viên đăng nhập lại thì server hủy đồng hồ bỏ cuộc.
4. Hết 120 giây mà vẫn không có ai online thì đội đó thua: server gửi `MATCH_END` với `reason: FORFEIT` (xem 2.5).

---

## 5. Luồng mất kết nối và kết nối lại

### 5.1. Mất kết nối giữa trận rồi vào lại

Sơ đồ: [sequence/30-reconnect-in-match.md](diagrams/sequence/30-reconnect-in-match.md)

**Khi mất kết nối:**
1. Server hủy phiên. Tàu vẫn ở trong trận, đánh dấu `online: false`.
2. Server gửi `MATCH_UPDATE` (`action: DISCONNECTED`) cho những người còn lại trong trận.
3. Tới lượt người đó thì server bỏ qua ngay, không chờ hết giờ lượt.

**Phía client:**
4. Client chuyển sang `RECONNECT_WAIT` và thử kết nối lại với backoff 1, 2, 4, 8, 16, 30 giây, sau đó cứ 30 giây thử lại một lần.
5. Kết nối được thì client tự gửi `LOGIN`, dùng mật khẩu đang giữ trong bộ nhớ (không ghi ra đĩa).

**Khi vào lại:**
6. Server trả `1000`, rồi gửi `MATCH_RESUME` kèm snapshot.
7. Server gửi `MATCH_UPDATE` (`action: RECONNECTED`) cho những người còn lại.
8. Client vẽ lại màn hình trận từ snapshot.

Người dùng tự bấm đăng xuất, hoặc bị `FORCE_LOGOUT`, thì client **không** tự kết nối lại.

### 5.2. Phiên cũ chưa kịp đóng (thay thế phiên)

Sơ đồ: [sequence/31-session-replace.md](diagrams/sequence/31-session-replace.md)

| | |
|---|---|
| **Kích hoạt** | `LOGIN` đúng mật khẩu cho tài khoản đang có phiên ở kết nối khác |
| **Kết quả** | Phiên chuyển sang kết nối mới. Kết nối cũ bị đóng |

1. Server gửi `FORCE_LOGOUT` với `reason: REPLACED` cho kết nối cũ. Event này có thể không tới được nơi nếu kết nối cũ đã chết.
2. Server đóng kết nối cũ và chuyển phiên sang kết nối mới.
3. Server trả `1000` cho kết nối mới. Nếu tài khoản đang trong trận thì gửi thêm `MATCH_RESUME`.
4. Những người khác **không** nhận `MATCH_UPDATE` (`DISCONNECTED`), vì phiên không bị gián đoạn.

**Vì sao thay thế phiên cũ thay vì từ chối đăng nhập mới:** khi mạng rớt, server có thể chưa phát hiện được ngay, mất tới 60 giây. Nếu từ chối thì người chơi bị kẹt ngoài trận suốt khoảng thời gian đó. Muốn chiếm phiên thì vẫn phải có đúng mật khẩu, nên cách này không làm giảm an toàn.

### 5.3. Mất kết nối ở những lúc khác

Sơ đồ: [sequence/32-disconnect-other.md](diagrams/sequence/32-disconnect-other.md)

| Đang ở đâu | Server xử lý |
|---|---|
| Bất kỳ thành viên nào, đang ở sảnh chờ | Người đó rời đội. Đóng sảnh chờ, gửi `LOBBY_CLOSED` (`reason: DISCONNECTED`), 2 đội về `IDLE` |
| Đang tới lượt và đang mở rương | Mất rương: gửi `CHEST_RESULT` (`correct: false`) cho đội đó, rồi `MATCH_UPDATE` (`action: TURN_SKIPPED`, `reason: DISCONNECTED`) |
| Không ở sảnh chờ, không trong trận | Hủy phiên và tự rời đội (chuyển đội trưởng nếu cần, đội trống thì bị xóa). Hủy lời mời và lời xin vào của người đó. Đồng đội nhận `TEAM_UPDATE` |

---

## 6. Trạng thái

### 6.1. Phía server

| Đối tượng | Sơ đồ | Các trạng thái |
|---|---|---|
| Kết nối và phiên | [state/server-connection.md](diagrams/state/server-connection.md) | `CONNECTED` → `AUTH_PENDING` → `LOGGED_IN` → `CLOSED` |
| Người chơi | [state/server-player.md](diagrams/state/server-player.md) | `NO_TEAM` ↔ `IN_TEAM` ↔ `LOBBY` → `IN_MATCH` → `IN_TEAM` |
| Sảnh chờ | [state/server-lobby.md](diagrams/state/server-lobby.md) | `WAIT_READY` ↔ `GUEST_READY` → `STARTED` hoặc `CLOSED` |
| Trận đấu và lượt | [state/server-match.md](diagrams/state/server-match.md) | `NEXT_TURN` → `ACTING` ↔ `CHEST` → `RESOLVE` hoặc `SKIPPED` → `CHECK_END` → `ENDED` |

Ghi chú:
- **Kết nối và phiên:** trong lúc ở `AUTH_PENDING`, request tiếp theo của kết nối đó phải chờ. `REGISTER` cũng đi qua pool để hash mật khẩu nhưng không đăng nhập luôn, nên kết nối vẫn ở `CONNECTED`.
- **Người chơi:** ngoài trận, mất kết nối là rời đội (về `NO_TEAM`). Riêng trong trận, người mất kết nối vẫn ở `IN_MATCH` và đăng nhập lại thì quay về đúng trận đó; hết trận mà vẫn offline thì rời đội. Khi ở `LOBBY` hoặc `IN_MATCH` thì mọi thao tác đội đều bị khóa, kể cả với admin.
- **Sảnh chờ:** đội chủ đổi setting thì sảnh chờ luôn quay về `WAIT_READY`.
- **Trận đấu:** admin khóa tài khoản người đang trong trận thì tàu bị hạ ngay, và trận cũng đi qua bước `CHECK_END`.

### 6.2. Phía client

| Đối tượng | Sơ đồ | Các trạng thái |
|---|---|---|
| Kết nối | [state/client-connection.md](diagrams/state/client-connection.md) | `DISCONNECTED` → `CONNECTING` → `CONNECTED` → `LOGGING_IN` → `ONLINE`, và `RECONNECT_WAIT` khi mất kết nối |
| Màn hình | [state/client-screen.md](diagrams/state/client-screen.md) | `LOGIN_SCREEN` → `HOME` → `LOBBY_SCREEN` → `GAME` → `RESULT` → `HOME` |
| Trong trận | [state/client-in-game.md](diagrams/state/client-in-game.md) | `WAITING` ↔ `MY_TURN` ↔ `ANSWERING`, `WATCH_CHEST`, `DEAD`, `ENDED` |

Nút nào được bật ở mỗi trạng thái trong trận:

| Trạng thái | Mua, sửa tàu | Mở rương | Tấn công | Reload | Chọn đáp án |
|---|---|---|---|---|---|
| `WAITING` | ❌ | ❌ | ❌ | ✅ | ❌ |
| `WATCH_CHEST` | ❌ | ❌ | ❌ | ✅ | ❌ (chỉ xem) |
| `MY_TURN` | ✅ | ✅ (nếu còn rương) | ✅ | ✅ | ❌ |
| `ANSWERING` | ❌ | ❌ | ❌ | ❌ | ✅ |
| `DEAD` | ❌ | ❌ | ❌ | ❌ | ❌ |

Bảng này chỉ để giao diện thân thiện hơn. **Server vẫn kiểm tra lại mọi lệnh** và không tin vào client.
