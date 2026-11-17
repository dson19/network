# Thiết kế client

Client là ứng dụng GTK3 chạy trên **1 luồng**. Socket được gắn vào main loop của GTK (lý do: [PROTOCOL.md](PROTOCOL.md) mục 1.5).

- Trạng thái client: [DESIGN.md](DESIGN.md) mục 6.2.
- Server: [SERVER.md](SERVER.md).

---

## 1. Các module

```
client/
├── main.c          khởi tạo GTK, đọc tham số, mở cửa sổ chính
├── net.c/.h        kết nối, tách dòng, gửi request, ghép response, PING, kết nối lại
├── state.c/.h      dữ liệu client: tài khoản, đội, sảnh chờ, snapshot trận
├── clock.c/.h      độ lệch đồng hồ với server, đếm ngược
├── errors.c/.h     đổi mã lỗi thành câu tiếng Việt cho người dùng
├── ui_login.c      màn đăng nhập / đăng ký
├── ui_home.c       màn chính: đội, lời mời, thách đấu
├── ui_admin.c      các tab chỉ admin thấy
├── ui_lobby.c      sảnh chờ
├── ui_game.c       màn trận đấu
├── ui_chest.c      hộp thoại câu hỏi rương
└── ui_result.c     màn kết quả
```

**Một chiều dữ liệu:** `net` nhận message → `state` cập nhật → `ui_*` vẽ lại theo `state`. Các màn hình không bao giờ tự sửa `state` và không tự tính luật chơi.

---

## 2. Tầng mạng (`net.c`)

### 2.1. Nhận dữ liệu
- Mở socket rồi đăng ký vào main loop bằng `g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR, ...)`.
- Socket để non-blocking. Callback đọc tới khi `EAGAIN`, rồi dùng `common/linebuf` (chung với server) để tách dòng.
- Kết nối tới server (`connect`) cũng non-blocking, để giao diện không bị đơ khi server chậm phản hồi.

### 2.2. Gửi request và ghép response
```c
typedef void (*ResponseCb)(int code, const cJSON *data, void *user);
uint32_t net_request(const char *cmd, cJSON *params, ResponseCb cb, void *user);
```
- `net_request` gán `id` tăng dần, lưu cặp `id → (cb, user, thời điểm gửi)` vào bảng chờ, rồi gửi đi.
- Response về thì tra `id` trong bảng chờ và gọi `cb`.
- Request quá **15 giây** mà chưa có response: gọi `cb` với mã lỗi nội bộ của client `CLIENT_TIMEOUT`, rồi coi như kết nối có vấn đề và chuyển sang kết nối lại.
- Event về thì chuyển sang `state_on_event(evt, data)`.

### 2.3. Giữ kết nối và kết nối lại
- Gửi `PING` mỗi 20 giây. Response của `PING` dùng để cập nhật độ lệch đồng hồ (`clock.c`).
- Mất kết nối khi đã đăng nhập: chuyển sang `RECONNECT_WAIT`, thử lại với backoff 1, 2, 4, 8, 16, 30 giây, sau đó 30 giây một lần. Kết nối được thì tự gửi `LOGIN`.
- Mật khẩu chỉ giữ trong RAM để tự đăng nhập lại, **không ghi ra đĩa**. Đăng xuất hoặc thoát app thì xóa (ghi đè bằng 0).
- Mọi request đang chờ khi mất kết nối đều được gọi `cb` với `CLIENT_DISCONNECTED`, để giao diện mở khóa các nút.

### 2.4. Đồng hồ (`clock.c`)
- Mỗi response `PING` có `time` (giờ server). Client tính `offset = time - (lúc gửi + lúc nhận) / 2`.
- Thời gian còn lại của lượt = `deadline_ms - (giờ máy + offset)`.
- Đếm ngược chạy bằng `g_timeout_add(250, ...)` và chỉ để hiển thị. Hết giờ thì client **không tự làm gì**, mà chờ event `TURN_SKIPPED` của server.

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
Để có số liệu sát thương cho màn này, `MATCH_END` cần thêm trường `stats` (xem PROTOCOL.md mục 5.4).

---

## 4. Thông báo lỗi

`errors.c` đổi mã lỗi thành câu cho người dùng. Ví dụ:

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
