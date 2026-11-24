# Việc chờ quyết định / chờ sửa

Ghi lại các điểm đã bàn nhưng chưa sửa. Đọc xong, chốt điểm nào thì sửa điểm đó rồi xóa khỏi file này.

## Bảo mật

3. **Admin có thể gian lận:** admin vào trận rồi gọi `ADMIN_QUESTION_LIST` để xem đáp án rương. Chọn một trong hai:
   - Tài khoản admin không được tham gia đội/trận (tạo đội, xin vào, nhận mời trả `4001`). Đơn giản nhất.
   - Admin đang trong trận thì không gọi được `ADMIN_QUESTION_*`.

4. **Không giới hạn số lần đăng nhập sai.** Thử mật khẩu liên tục được, và spam `LOGIN` làm đầy hàng đợi pool (người khác nhận `9002`).
   Đề xuất: sai 5 lần trên 1 kết nối thì đóng kết nối; sai 10 lần/phút từ cùng IP thì từ chối 30 giây. Áp dụng cả `CHANGE_PASSWORD`.

5. **Mật khẩu gửi dạng gốc qua TCP không mã hóa.** Băm ở client không giúp được (pass-the-hash).
   Đề xuất: giữ nguyên, thêm đoạn "Hạn chế bảo mật" vào report, hướng mở rộng là TLS.

6. Dò được tên tồn tại / ai online qua `REGISTER` (`3005`), `PLAYER_LIST` (ai đang online và rảnh), `TEAM_LIST`, `TEAM_MEMBERS`. Đã đánh giá là chấp nhận được với game; có thể ghi vào report là đã cân nhắc.

## Sơ đồ (`docs/design/diagrams/`)

8. **Ảnh sơ đồ chưa render lại.** File nguồn `.md` trong `docs/design/diagrams/` đã cập nhật (bỏ `id`, `user_id`, `MATCH_UPDATE`, DB writer, client Qt, `index`/`indexes`), nhưng ảnh trong `images/` vẫn là bản cũ. Máy chưa có `mmdc` (mermaid-cli) để render.

## Code

9. Bước 2 (đồng hồ min-heap, idle timeout 60 giây) chưa làm.
10. Cấu trúc lưu phiên (Connection / User, bảng băm tự viết trong `cache.c`) chưa chốt; đổi được nếu cần.
