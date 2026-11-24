# Thiết kế client

Client viết bằng **C++17 + Qt 5.12 Quick (QML)**, chạy trên **1 luồng**. Riêng tầng socket viết bằng **C11** (yêu cầu của môn: phần socket Linux phải viết bằng C). Socket được gắn vào vòng lặp sự kiện của Qt bằng `QSocketNotifier` (lý do: [PROTOCOL.md](PROTOCOL.md) mục 1.5).

**Ranh giới C / C++:** mọi lời gọi socket (`socket`, `connect`, `send`, `recv`, `close`) chỉ nằm trong `net.c`. Phần C++ không gọi API socket nào và không dùng `QTcpSocket`; nó chỉ lấy `fd` từ `net.c` để biết khi nào socket đọc/ghi được.

- Trạng thái client: [DESIGN.md](DESIGN.md) mục 6.2.
- Server: [SERVER.md](SERVER.md).

---

## 1. Các module

```
TCP_Client/
├── client.pro          file qmake (Makefile gốc gọi qmake để build client)
├── net.c/.h            [C11] socket non-blocking: connect, send (buffer gửi), recv + tách dòng bằng common/linebuf, close
├── main.cpp            khởi tạo QGuiApplication, đọc tham số, nạp QML, đăng ký NetClient và ClientState
├── NetClient.cpp/.h    [C++] gắn fd của net.c vào QSocketNotifier, parse/tạo JSON (QJsonDocument),
│                       quản lý request đang chờ + hàng đợi cục bộ, PING, kết nối lại (QTimer)
├── ClientState.cpp/.h  [C++] dữ liệu client: tài khoản, đội, sảnh chờ, snapshot trận; đưa sang QML qua Q_PROPERTY/signal
├── Clock.cpp/.h        [C++] độ lệch đồng hồ với server, đếm ngược
├── Errors.cpp/.h       [C++] đổi mã lỗi thành câu tiếng Việt cho người dùng
└── qml/
    ├── Main.qml        cửa sổ chính, chuyển màn theo ClientState
    ├── Login.qml       đăng nhập / đăng ký
    ├── Home.qml        đội, lời mời, thách đấu, tab admin
    ├── Lobby.qml       sảnh chờ
    ├── Game.qml        màn trận đấu
    ├── ChestDialog.qml hộp thoại câu hỏi rương
    ├── Result.qml      màn kết quả
    └── components/     ShipCard, HpBar, Countdown, hiệu ứng bắn/nổ
```

**Một chiều dữ liệu:** `net.c` nhận byte → `NetClient` tách message → `ClientState` cập nhật → QML vẽ lại qua binding. QML gửi lệnh bằng cách gọi `NetClient.request(...)`; QML không bao giờ tự sửa `ClientState` và không tự tính luật chơi.

---

## 2. Tầng mạng (`net.c` + `NetClient`)

### 2.1. Nhận dữ liệu
- `net.c` mở socket non-blocking và trả `fd`. `NetClient` tạo `QSocketNotifier(fd, Read)` để Qt báo khi có dữ liệu, và `QSocketNotifier(fd, Write)` chỉ bật khi buffer gửi của `net.c` còn dữ liệu chưa gửi hết.
- Khi socket đọc được, `NetClient` gọi `net_read()`. Hàm này đọc tới khi `EAGAIN` và dùng `common/linebuf` (chung với server) để tách dòng; mỗi dòng hoàn chỉnh được trả về cho `NetClient` để parse JSON.
- Kết nối tới server (`connect`) cũng non-blocking, để giao diện không bị đơ khi server chậm phản hồi.

```c
/* net.h - phần C, chỉ chứa thao tác socket */
int  net_connect(NetConn *c, const char *host, uint16_t port); /* non-blocking, trả fd */
int  net_send_line(NetConn *c, const char *line);              /* thêm \r\n, đưa vào buffer gửi */
int  net_flush(NetConn *c);                                    /* gửi tiếp buffer khi socket ghi được */
int  net_read(NetConn *c, void (*on_line)(const char *line, void *u), void *u);
void net_close(NetConn *c);
```

### 2.2. Gửi request và ghép response
```cpp
using ResponseCb = std::function<void(int code, const QJsonObject &data)>;
void NetClient::request(const QString &cmd, const QJsonObject &params, ResponseCb cb);
```
- Protocol không có trường `id`: mỗi kết nối chỉ có **1 request đang chờ response** (PROTOCOL.md mục 9.2).
- `request` gửi ngay nếu không có request nào đang chờ, và ghi lại `(cb, thời điểm gửi)` của request đó. Nếu đang có request chờ thì xếp vào hàng đợi cục bộ (FIFO), gửi khi response trước về.
- Message có `type = "res"` luôn thuộc request đang chờ: gọi `cb` của nó, rồi gửi request kế tiếp trong hàng đợi (nếu có).
- Request quá **15 giây** mà chưa có response: gọi `cb` với mã lỗi nội bộ của client `CLIENT_TIMEOUT`, rồi coi như kết nối có vấn đề và chuyển sang kết nối lại.
- Event về thì chuyển sang `ClientState::onEvent(evt, data)`.

### 2.3. Giữ kết nối và kết nối lại
- Gửi `PING` mỗi 20 giây. Response của `PING` dùng để cập nhật độ lệch đồng hồ (`Clock`).
- Mất kết nối khi đã đăng nhập: chuyển sang `RECONNECT_WAIT`, thử lại với backoff 1, 2, 4, 8, 16, 30 giây (đếm bằng `QTimer`), sau đó 30 giây một lần. Mỗi lần thử gọi `net_connect()`. Kết nối được thì tự gửi `LOGIN`.
- Mật khẩu chỉ giữ trong RAM để tự đăng nhập lại, **không ghi ra đĩa**. Đăng xuất hoặc thoát app thì xóa (ghi đè bằng 0).
- Request đang chờ và mọi request trong hàng đợi cục bộ khi mất kết nối đều được gọi `cb` với `CLIENT_DISCONNECTED`, để giao diện mở khóa các nút.

### 2.4. Đồng hồ (`Clock`)
- Mỗi response `PING` có `time` (giờ server). Client tính `offset = time - (lúc gửi + lúc nhận) / 2`.
- Thời gian còn lại của lượt = `deadline_ms - (giờ máy + offset)`.
- Đếm ngược chạy bằng `QTimer` 250 ms và chỉ để hiển thị. Hết giờ thì client **không tự làm gì**, mà chờ event `TURN_SKIPPED` của server.

---

## 3. Màn hình

Phác thảo bố cục, không phải thiết kế đồ họa cuối cùng.

### 3.1. Đăng nhập

```
┌──────────────────────────────────────────────┐
│            PHI ĐỘI KHÔNG GIAN                │
│                                              │
│   Server   [ 127.0.0.1     ] : [ 5500 ]      │
│   Tài khoản [ son                     ]      │
│   Mật khẩu  [ ••••••••                ]      │
│                                              │
│        [ Đăng nhập ]   [ Đăng ký ]           │
│                                              │
│   ⚠ Sai tài khoản hoặc mật khẩu              │
└──────────────────────────────────────────────┘
```
- Bấm Đăng ký thì hiện thêm ô "Nhập lại mật khẩu". Client kiểm tra trước định dạng username và độ dài mật khẩu để báo lỗi ngay, nhưng server vẫn kiểm tra lại.
- Địa chỉ server và tên đăng nhập gần nhất được lưu vào `~/.config/phidoi/client.conf`. Mật khẩu thì không lưu.

### 3.2. Màn chính

```
┌─ son (PLAYER) ─────────────────────────── [Đổi mật khẩu] [Đăng xuất] ─┐
│ [Danh sách đội] [Đội của tôi] [Lời mời (2)] [Thách đấu (1)] [Admin]   │
├───────────────────────────────────────────────────────────────────────┤
│ Danh sách đội                                    [ Tạo đội mới ]      │
│ ┌───────────────┬────────────┬──────┬──────────┬───────────────────┐  │
│ │ Tên đội       │ Đội trưởng │ Số   │ Trạng    │                   │  │
│ ├───────────────┼────────────┼──────┼──────────┼───────────────────┤  │
│ │ Rong Bay      │ son        │ 2/3  │ IDLE     │ [Xem] [Xin vào]   │  │
│ │ Sao Bang      │ minh       │ 2/3  │ IN_MATCH │ [Xem]             │  │
│ │ Hac Long      │ lan        │ 2/3  │ IDLE     │ [Xem] [Thách đấu] │  │
│ └───────────────┴────────────┴──────┴──────────┴───────────────────┘  │
└───────────────────────────────────────────────────────────────────────┘
```
- **Tab "Đội của tôi":** 3 slot cố định. Đội trưởng có thêm các nút Mời, Kick, Chuyển đội trưởng, Xóa đội, và danh sách lời xin vào kèm nút Duyệt/Từ chối. Mọi người có nút Rời đội.
- **Tab "Lời mời"** và **tab "Thách đấu"** có số đếm trên tiêu đề, cập nhật theo event.
- **Tab "Admin"** chỉ hiện khi `role = ADMIN`: danh sách tài khoản (khóa/mở khóa), xem log, quản lý câu hỏi.
- Đội đang ở sảnh chờ hoặc trong trận thì các nút thao tác đội bị **làm mờ**, kèm chú thích "Đội đang trong trận".

### 3.3. Sảnh chờ

```
┌─ Sảnh chờ: Rong Bay (chủ) vs Hac Long (khách) ────────────────┐
│                                                               │
│   Rong Bay                    Hac Long                        │
│   ★ son   ● online            ★ lan   ● online                │
│     hoa   ● online              tuan  ○ offline               │
│                                                               │
│   Thời gian lượt   [ 30 ▾] giây      (chỉ đội chủ sửa được)   │
│   Thời gian rương  [ 30 ▾] giây                               │
│   Coin ban đầu     [ 0     ]                                  │
│                                                               │
│   Đội khách: ✔ Đã sẵn sàng                                    │
│                                                               │
│   [ Rời sảnh ]                              [ Bắt đầu ▶ ]     │
└───────────────────────────────────────────────────────────────┘
```
- Đội trưởng đội chủ thấy các ô setting và nút **Bắt đầu**. Nút này chỉ bấm được khi đội khách đã sẵn sàng.
- Đội trưởng đội khách thấy setting ở dạng chỉ đọc, cùng nút **Sẵn sàng / Bỏ sẵn sàng**.
- Thành viên thường chỉ xem.

### 3.4. Trận đấu

```
┌─ Trận #3 ─ Lượt của: son (bạn) ─────────────────────── ⏱ 00:18 ─┐
│                                                                 │
│  ĐỘI ĐỊCH: Hac Long                                             │
│  ┌──────────────────┐ ┌──────────────────┐                      │
│  │ lan      ● online│ │ tuan    ○ offline│                      │
│  │ HP ████████░ 820 │ │ HP ██████████1000│                      │
│  │ Giáp 500+1200    │ │ Giáp —           │                      │
│  │ Laze 2  Tên lửa 1│ │ Laze 0  Tên lửa 0│                      │
│  └──────────────────┘ └──────────────────┘                      │
│                                                                 │
│  ĐỘI TA: Rong Bay                                               │
│  ┌──────────────────┐ ┌──────────────────┐                      │
│  │ son (bạn)        │ │ hoa              │                      │
│  │ HP █████████ 950 │ │ HP ██████░░ 640  │                      │
│  │ Giáp 1500  Coin  │ │ Giáp —  Coin 300 │                      │
│  │ 2400             │ │                  │                      │
│  └──────────────────┘ └──────────────────┘                      │
├─────────────────────────────────┬───────────────────────────────┤
│ VŨ KHÍ CỦA BẠN                  │ CỬA HÀNG                      │
│ Tự động  37 viên / 3 thùng  [R] │ Thùng đạn  100   [Mua]        │
│ Laze 1    7 phát            [R] │ Laze      1000   [Mua]        │
│ Laze 2   10 phát            [R] │ Bộ nguồn   100   [Mua]        │
│ Bộ nguồn: 2                     │ Tên lửa   2000   [Mua]        │
│ Tên lửa: 2/4                    │ Giáp CB   1000   [Mua]        │
│                                 │ Giáp TC   2000   [Mua]        │
│ ┌─ Tấn công ────────────────┐   │ Sửa tàu [ 50 ] HP [Sửa]       │
│ │ Mục tiêu [ lan        ▾]  │   │                               │
│ │ Pháo ( ) Không            │   │ 🎁 Rương BẠC  [ Mở rương ]    │
│ │      ( ) Tự động          │   ├───────────────────────────────┤
│ │      (•) Laze  số khẩu [2]│   │ NHẬT KÝ TRẬN                  │
│ │ Tên lửa  số quả [1]       │   │ son bắn lan: 1000 dmg         │
│ │ Dự kiến: 1000 Dam         │   │ lan bỏ lượt (hết giờ)         │
│ │           [ BẮN ▶ ]       │   │ hoa mở rương: đúng +500       │
│ └───────────────────────────┘   │                               │
└─────────────────────────────────┴───────────────────────────────┘
```
- **Nút [R]** là reload từng khẩu. Nút này bấm được cả ngoài lượt.
- **Dự kiến Dam** chỉ để tham khảo (cộng Dam của các vũ khí đã chọn). Kết quả thật luôn lấy từ event `ATTACK`.
- **Bật/tắt nút** theo bảng ở DESIGN.md mục 6.2. Đang chờ response thì làm mờ nút đó để tránh bấm 2 lần.
- **Nhật ký trận** sinh từ các event. Các dòng giữ trong bộ nhớ, tối đa 200 dòng.
- **Đối phương:** không hiện coin và đạn, vì server không gửi.

### 3.5. Câu hỏi rương

```
┌─ Rương VÀNG — 2000 coin ─────────────────── ⏱ 00:24 ─┐
│                                                      │
│  Hàm nào trả về các socket sẵn sàng mà không phải    │
│  quét toàn bộ danh sách?                             │
│                                                      │
│   ( ) A. select()                                    │
│   ( ) B. poll()                                      │
│   ( ) C. epoll_wait()                                │
│   ( ) D. accept()                                    │
│                                                      │
│                                   [ Trả lời ]        │
└──────────────────────────────────────────────────────┘
```
- Là hộp thoại **modal**: trong lúc mở, không bấm được gì khác trên màn trận. Điều này khớp với luật "đang mở rương thì chỉ được trả lời".
- **Đồng đội** thấy cùng hộp thoại nhưng các lựa chọn bị làm mờ, kèm dòng "son đang trả lời". Hộp thoại của đồng đội **không** modal, để họ vẫn reload được.
- Có `CHEST_RESULT` thì hộp thoại hiện đúng/sai và đáp án đúng trong 2 giây, rồi tự đóng.

### 3.6. Kết quả

```
┌─ KẾT THÚC TRẬN #3 ─────────────────────────────┐
│                                                │
│              🏆  Rong Bay THẮNG                 │
│           (đối phương bị hạ hết tàu)           │
│                                                │
│   Người chơi   Đội        Sống sót   Sát thương│
│   son          Rong Bay   ✔          3200      │
│   hoa          Rong Bay   ✘          1450      │
│   lan          Hac Long   ✘           900      │
│   tuan         Hac Long   ✘             0      │
│                                                │
│   Số lượt: 41                                  │
│                              [ Về màn chính ]  │
└────────────────────────────────────────────────┘
```
Số liệu của màn này lấy từ trường `turns` và `stats` của event `MATCH_END` (PROTOCOL.md mục 5.4).

### 3.7. Hiệu ứng (QML)

Hiệu ứng chỉ để hiển thị, luôn chạy **sau khi** nhận event từ server, không bao giờ đoán trước kết quả.

| Event | Hiệu ứng |
|---|---|
| `TURN_START` | Thẻ tàu đang tới lượt sáng viền; đồng hồ chuyển vàng khi còn 10 giây, đỏ khi còn 5 giây |
| `CHEST_DROP` | Rương rơi xuống góc màn hình, màu theo tier (đồng/bạc/vàng) |
| `ATTACK` | Tia laze/đạn/tên lửa bay từ tàu bắn sang tàu mục tiêu; tàu mục tiêu rung; thanh giáp rồi thanh HP tụt mượt (`Behavior on value { NumberAnimation }`) |
| `SHIP_DESTROYED` | Tàu nổ, thẻ tàu chuyển xám |
| `CHEST_RESULT` | Rương mở, hiện số coin cộng thêm bay lên |
| `MATCH_END` | Màn kết quả trượt vào |

Nền màn trận là trời sao chuyển động chậm (`ParticleSystem`), theme tối.

---

## 4. Thông báo lỗi

`Errors` đổi mã lỗi thành câu cho người dùng. Ví dụ:

| Mã | Câu hiển thị |
|---|---|
| 3003 | Sai tài khoản hoặc mật khẩu |
| 3004 | Tài khoản đã bị khóa |
| 5003 | Đội đã đủ 3 người |
| 5006 | Đội đang trong trận, không thể thao tác |
| 6001 | Hai đội phải có cùng số người |
| 7002 | Chưa tới lượt của bạn |
| 7004 | Không đủ coin |
| 7007 | Không thể bắn cùng lúc pháo tự động và pháo laze |
| 9002 | Server đang quá tải, thử lại sau |
| `CLIENT_TIMEOUT` | Server không phản hồi |
| `CLIENT_DISCONNECTED` | Mất kết nối, đang thử kết nối lại... |

Mã nào không có trong bảng thì hiện `msg` của server kèm mã số.

Lỗi do người dùng thao tác (ví dụ thiếu coin) hiện dạng **thông báo nhỏ, tự tắt sau 3 giây**. Lỗi kết nối hiện dạng **thanh cảnh báo** cố định ở đầu cửa sổ cho tới khi kết nối lại được.
