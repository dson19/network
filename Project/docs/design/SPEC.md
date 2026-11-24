# Đặc tả: Trò chơi "Phi đội không gian"

## 1. Hệ thống
- Giao tiếp qua TCP. Môn học bắt buộc mọi phần đụng tới socket Linux phải viết bằng C.
- Server viết bằng C (C11).
- Client viết bằng C++17 + Qt 5.12 Quick (QML); riêng tầng socket của client (`net.c`) viết bằng C11.
- Server chạy nhiều trận cùng lúc, theo mô hình 1 luồng chính dùng epoll + thread pool cho việc chậm. Message dạng JSON, mỗi message 1 dòng. Chi tiết và lý do chọn: xem [PROTOCOL.md](PROTOCOL.md).
- Dùng SQLite ngay từ đầu. Toàn bộ SQL nằm trong lớp `storage` (`storage.h`/`storage.c`); phần còn lại của server chỉ gọi hàm như `storage_user_create()`, không bao giờ viết SQL trực tiếp.
- Phân loại dữ liệu:

| Dữ liệu | Để ở đâu | Lý do |
|---|---|---|
| Tài khoản, mật khẩu, role, trạng thái khóa | SQLite | Phải còn sau khi server khởi động lại |
| Câu hỏi trắc nghiệm | SQLite (nạp từ file soạn sẵn ở lần chạy đầu) | Admin thêm/xóa được |
| Phiên đăng nhập, ai đang online | RAM | Gắn với kết nối TCP, mất kết nối là hết phiên |
| Đội (party) và thành viên | RAM | Đội là party tạm thời như khi ghép trận, chỉ gồm người đang online |
| Lời mời, lời xin vào đội, lời thách đấu | RAM | Dữ liệu tạm |
| Sảnh chờ, trận đấu, đồng hồ lượt/rương | RAM | Mỗi trận reset; thay đổi liên tục; server sập thì trận cũng hỏng, lưu lại vô ích |
| Log hoạt động, kết quả trận | File | Chỉ ghi nối thêm, không cần truy vấn. Không lưu lịch sử trận vào DB |

- "RAM" nghĩa là dữ liệu chỉ nằm trong biến/struct C của tiến trình server (ví dụ mảng `Match`), mất khi server tắt.

## 2. Role và phân quyền
Các mức: Chưa đăng nhập, Thành viên, Đội trưởng, Admin.

| Lệnh | Admin | Đội trưởng | Thành viên | Chưa đăng nhập |
|---|---|---|---|---|
| Xóa đội | Mọi đội | Đội mình | ❌ | ❌ |
| Kick thành viên | Mọi đội | Đội mình | ❌ | ❌ |
| Khóa tài khoản | ✅ | ❌ | ❌ | ❌ |
| Xem log | ✅ | ❌ | ❌ | ❌ |
| Quản lý câu hỏi | ✅ | ❌ | ❌ | ❌ |

- **Admin không can thiệp được vào đội đang ở sảnh chờ hoặc trong trận.** Khi đó xóa đội và kick bị khóa với tất cả mọi người, kể cả admin.
- Khóa tài khoản của người đang trong trận: người đó bị đăng xuất ngay và tàu của họ tính là bị hạ (HP = 0). Nếu đó là tàu cuối cùng còn sống của đội thì đội đó thua.

## 3. Đội
- Đội hoạt động như **party khi ghép trận**: chỉ tồn tại trong RAM, server khởi động lại thì mọi đội mất.
- Đội chỉ gồm người **đang online**:
  - Thành viên đăng xuất hoặc mất kết nối khi đội **không ở trong trận** thì tự rời đội.
  - Mất kết nối **trong trận** thì vẫn ở trong đội tới hết trận để còn vào lại được. Hết trận mà vẫn offline thì tự rời đội.
  - Chỉ mời được người đang online. Người chơi offline thì mọi lời xin vào và lời mời của họ bị hủy.
- Hết trận, đội **quay về room** với các thành viên còn online, giữ nguyên đội trưởng, và thách đấu tiếp được ngay.
- Mỗi đội tối đa 3 người, có 3 slot cố định là 0, 1, 2. Mỗi người chỉ ở 1 đội.
- Có người rời đội thì các slot **không dồn xuống**. Người mới vào đội nhận slot trống có số nhỏ nhất.
- Người tạo đội là đội trưởng. Đội trưởng chuyển quyền được, chuyển xong thì mất quyền. Lúc nào đội cũng có đúng 1 đội trưởng.
- Đội trưởng rời đội thì quyền chuyển cho thành viên có số slot nhỏ nhất. Người cuối cùng rời đội thì đội bị xóa.
- Vào đội có 2 cách, cả hai đều phải được duyệt:
  - Người chơi xin vào đội, đội trưởng duyệt.
  - Đội trưởng mời, người được mời chấp nhận.
- Mỗi đội chỉ ở 1 trận tại một thời điểm. Khi đội đang ở sảnh chờ hoặc trong trận thì khóa các thao tác: rời đội, mời, xin vào, kick, xóa đội (kể cả với admin).

## 4. Thách đấu và sảnh chờ
- Đội trưởng gửi lời thách đấu, đội trưởng đội kia chấp nhận. Chỉ thách đấu được giữa 2 đội có số người bằng nhau.
- Lời thách đấu được chấp nhận thì 2 đội vào sảnh chờ.
- Thành viên của 1 trong 2 đội mất kết nối khi đang ở sảnh chờ thì người đó rời đội và sảnh chờ bị đóng (2 đội không còn cùng số người).
- Trong sảnh chờ, đội trưởng đội chủ (đội gửi thách đấu) chỉnh được:
  - Thời gian lượt: mặc định 30 giây.
  - Coin ban đầu: mặc định 0.
  - Thời gian trả lời rương: mặc định 30 giây.
- Đội khách bấm "Sẵn sàng" xong thì đội chủ mới bấm "Bắt đầu" được.

## 5. Tàu
- Tàu khởi đầu: HP 1000, Amor 0, 1 pháo tự động với `50 viên / 0 thùng`.
- Coin, trang bị, HP và đạn đều reset khi bắt đầu trận mới. Coin tính riêng cho từng người.

## 6. Vũ khí

| Loại | Giá | Dam | Ghi chú |
|---|---|---|---|
| Pháo tự động | Có sẵn | 10/viên | Chỉ có 1 khẩu, không bỏ được, không mua thêm được. Thùng đạn 100 coin/50 viên |
| Pháo laze | 1000 coin/khẩu | 100/phát | Tối đa 3 khẩu (pháo tự động chiếm 1 trong 4 slot pháo). Bộ nguồn 100 coin/10 phát |
| Tên lửa | 2000 coin/quả | 800 | Dùng 1 lần. Không chiếm slot pháo, mang tối đa 4 quả |

- Mua đạn không giới hạn số lượng, chỉ cần đủ coin.
- Mỗi khẩu có băng đạn riêng. Kho thùng đạn hoặc bộ nguồn dùng chung cho cùng một loại vũ khí.
- Hiển thị: `37 viên / 3 thùng`; `Laze 1: 7 phát / 2 bộ`; `Tên lửa: 2/4`.
- Reload:
  - Làm được bất cứ lúc nào, kể cả trong lượt của đối phương.
  - Mỗi lần lấy 1 thùng hoặc 1 bộ từ kho, nạp đầy băng của khẩu đó.
  - Số đạn còn lại trong băng cũ mất luôn.
  - Băng hết thì khẩu đó tự reload nếu kho còn.
- Tấn công: chọn 1 tàu mục tiêu, sau đó chọn phần pháo, phần tên lửa, hoặc cả hai. Phải chọn ít nhất 1 trong 2 phần.
  - Phần pháo: chỉ dùng **1 loại pháo**, hoặc pháo tự động hoặc pháo laze, không trộn 2 loại.
    - Pháo tự động: chỉ có 1 khẩu nên bắn 1 viên.
    - Pháo laze: chọn số khẩu bắn, từ 1 đến số khẩu còn đạn. Mỗi khẩu bắn 1 phát và băng của khẩu đó mất 1 phát.
  - Phần tên lửa: chọn số quả bắn, từ 1 đến số quả đang mang.
  - Ví dụ hợp lệ: 2 laze + 1 tên lửa; 1 tự động + 2 tên lửa; chỉ 3 tên lửa.
  - Ví dụ không hợp lệ: 1 laze + 1 tự động; 1 laze + 1 tự động + 1 tên lửa.

## 7. Giáp

| Loại | Giá | Amor |
|---|---|---|
| Giáp cơ bản | 1000 coin | 500 |
| Giáp tăng cường | 2000 coin | 1500 |

- Có tối đa 2 lớp (loại bất kỳ), Amor cộng dồn. Lớp mua sau bị trừ trước.
- Lớp nào về 0 Amor thì bị gỡ ra.
- Đang đủ 2 lớp mà mua thêm thì lớp còn ít Amor hơn bị bỏ đi. Giáp bị bỏ thì mất luôn, không hoàn tiền.

## 8. Sát thương và sửa tàu
- Mỗi 1 Dam trừ 1 Amor (bắt đầu từ lớp ngoài). Phần dư trừ tiếp vào HP.
- HP về 0 thì tàu chết, không hồi sinh.
- Sửa tàu: 1 coin cho mỗi 1 HP, sửa tối đa đến 1000 HP. Tàu đã chết thì không sửa được.

## 9. Lượt
- Thứ tự lượt: A0 → B0 → A1 → B1 → A2 → B2 (theo slot), bỏ qua tàu đã chết.
- Trong lượt của mình, người chơi làm được các việc sau, theo thứ tự bất kỳ:
  - Mua đồ
  - Sửa tàu
  - Mở rương
  - Reload
- Bấm tấn công là hết lượt.
- Ngoài lượt của mình thì chỉ được reload.
- Khi đang mở rương thì đồng hồ lượt tạm dừng (xem mục 10).
- Hết giờ lượt hoặc bị disconnect thì bị bỏ qua lượt.

## 10. Rương kho báu
- Mỗi lượt server thả 1 rương cho người đang tới lượt.

| Loại | Tỉ lệ | Coin |
|---|---|---|
| Đồng | 60% | 100 |
| Bạc | 30% | 500 |
| Vàng | 10% | 2000 |

- Câu hỏi của rương vàng khó hơn.
- Mở rương là không bắt buộc. Không mở thì hết lượt rương biến mất.
- Mở rương thì đồng hồ lượt tạm dừng và rương có đồng hồ riêng (30 giây, chỉnh được trong sảnh chờ). Trả lời xong hoặc hết giờ rương thì đồng hồ lượt chạy tiếp.
- Trong lúc mở rương, người chơi chỉ được trả lời câu hỏi. Không được bắn, mua, sửa tàu hay reload.
- Câu hỏi trắc nghiệm, chỉ được trả lời 1 lần. Đúng thì nhận coin, sai thì mất rương.
- Đồng đội chỉ xem được câu hỏi, không có chat. Đội đối phương không thấy câu hỏi.

## 11. Kết thúc trận
- Đội nào bị hạ hết tàu trước thì thua.
- Cả đội offline liên tục 120 giây trong trận thì đội đó thua do bỏ cuộc.
- Kết quả trận được ghi vào log.

## 12. Barem điểm

| Chức năng | Điểm |
|---|---|
| Xử lý truyền dòng | 1 |
| Cài đặt cơ chế vào/ra socket trên server | 2 |
| Đăng ký và quản lý tài khoản | 2 |
| Đăng nhập và quản lý phiên | 2 |
| Kiểm soát quyền truy cập | 2 |
| Tạo đội chơi | 1 |
| Xóa đội chơi | 1 |
| Liệt kê danh sách đội chơi | 1 |
| Liệt kê danh sách thành viên trong đội | 1 |
| Yêu cầu tham gia một đội và phê duyệt | 2 |
| Mời tham gia vào đội và phê duyệt | 2 |
| Rời đội | 1 |
| Xóa thành viên khỏi đội | 1 |
| Gửi/chấp nhận lời thách đấu | 2 |
| Bắt đầu cuộc chơi | 1 |
| Thả rương kho báu | 2 |
| Mở rương kho báu | 2 |
| Mua giáp trang bị | 2 |
| Mua vũ khí trang bị | 4 |
| Tấn công | 4 |
| Sửa chữa tàu | 1 |
| Xác định kết quả cuộc chơi | 1 |
| Ghi log hoạt động | 1 |
| **Tổng** | **39** |
