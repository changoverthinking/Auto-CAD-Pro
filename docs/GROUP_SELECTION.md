# Chọn nhiều đối tượng và chỉnh sửa nhóm

## Mục đích

Giảm thao tác lặp khi chỉnh sửa bản vẽ 2D; một thao tác nhóm tương ứng một lần Undo/Redo. Không để lỗi ở một thành viên làm bản vẽ bị chỉnh sửa một phần.

## Cách dùng

- Esc chuyển sang Select. Bấm một đối tượng để chọn riêng đối tượng đó.
- Giữ Ctrl và bấm để thêm/bỏ từng đối tượng. Ctrl + bấm vùng trống giữ nguyên lựa chọn; bấm vùng trống không giữ Ctrl bỏ lựa chọn.
- Ctrl+A hoặc Draw → Select All Editable chọn mọi đối tượng đang hiển thị, không bị khóa, kể cả ngoài vùng nhìn. Đây là lựa chọn đối tượng 2D, không phải lựa chọn phần tử BIM.
- M: Move; Y: Copy; R: Rotate; S: Scale; I: Mirror; Delete: xóa. Toàn bộ tập lựa chọn được xử lý cùng một lần.
- Move/Copy: chọn điểm gốc, điểm đích. Rotate: chọn tâm, điểm xác định góc so với trục X. Scale: chọn tâm, điểm tham chiếu, điểm đích. Mirror: chọn hai điểm của trục phản chiếu.
- Ctrl+Z/Ctrl+Y khôi phục/thực hiện lại cả nhóm. Copy chọn các bản sao mới; Undo Copy loại bỏ các ID không còn tồn tại khỏi lựa chọn.
- Mỗi đối tượng có khung chọn. Thanh trạng thái hiển thị ID chính và số lượng; bảng thuộc tính hiển thị đối tượng chính để xem. Các lệnh thuộc tính, tạo block, Trim, Extend, Offset và Hatch hiện cần chọn đúng một đối tượng.

## Cơ chế ngăn lỗi tái phát

1. Tập lựa chọn giữ ID duy nhất, không tạo bản sao hình học riêng dễ lệch dữ liệu.
2. Tính mọi hình học mới trước khi cập nhật. Kết quả có NaN/Infinity bị từ chối.
3. Kiểm tra mọi đối tượng nguồn còn tồn tại và được phép sửa. Nhóm có thành viên khóa/ẩn bị từ chối toàn bộ.
4. Thực thi các lệnh trên bản tài liệu tạm. Chỉ thay tài liệu thật sau khi tất cả thành công; thất bại giữ nguyên dữ liệu và lịch sử Redo.
5. Copy ghi thuộc tính cùng với thao tác thêm, giữ layer/màu/kiểu nét/độ dày và ID qua Undo/Redo.
6. Kiểm thử lỗi ở lệnh con thứ hai, khóa layer, Undo/Redo, lưu/mở cùng các thao tác thực trên Windows được đăng ký trong CI.

## Bằng chứng và giới hạn

- Kiểm thử core: `acp_group_selection_tests`; bộ CTest đầy đủ hiện có 24 suite, đã chạy lặp 5 lần thành công trên Linux (120 lượt).
- Windows: `tests/gui_group_selection_smoke.ps1` kiểm tra Ctrl-click, Ctrl+A/menu, Move/Copy/Rotate/Scale/Mirror/Delete và Undo/Redo bằng snapshot dự án. Kết quả Windows chỉ được xác nhận sau CI.
- Giao dịch nhóm sao chép tài liệu tạm trong lúc thực thi; lịch sử chỉ giữ dữ liệu lệnh con. Chưa có benchmark đủ 10k/100k đối tượng để chấm hiệu năng 8/10.
- Chưa có chọn bằng cửa sổ/crossing, chỉnh thuộc tính hàng loạt, lựa chọn BIM hoặc preview hình học toàn nhóm. Hướng dẫn điểm gốc/đích hiện dùng đường chỉ dẫn sẵn có.
- Đây là một hạng mục trong lộ trình chất lượng, chưa phải bản bàn giao đạt 8/10 ở mọi tiêu chí.
