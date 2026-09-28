# Nhập tọa độ chính xác — F6

Chọn công cụ vẽ (Line, Circle, Polyline, Arc, Rectangle, Dimension...) rồi nhấn
**F6**, hoặc chọn **Draw → Enter Coordinates**. Mỗi lần nhập tương đương một điểm
của công cụ đang dùng, tính bằng millimeter.

| Dạng | Ví dụ | Ý nghĩa |
|---|---|---|
| Tuyệt đối | `100.125,200.0625` | Điểm X=100.125 mm, Y=200.0625 mm |
| Tương đối | `@300,-40` | Dịch từ điểm tham chiếu hiện tại +300 mm theo X, -40 mm theo Y |
| Cực tương đối | `@500<90` | Cách điểm tham chiếu 500 mm, góc 90° ngược chiều kim đồng hồ từ +X |

Dùng dấu chấm cho phần thập phân, dấu phẩy ngăn cách X/Y. Có thể dùng số mũ như
`1e3,2e3`; không thêm hậu tố đơn vị như mm/m. Góc tính bằng độ.

Điểm tham chiếu là điểm vừa nhập trong thao tác hiện tại: điểm cuối polyline,
điểm thứ hai của thao tác ba điểm, hoặc điểm đầu đã chọn. Khi bắt đầu công cụ mới
hoặc thao tác đã hoàn tất, nhập một điểm tuyệt đối trước khi dùng `@`.

## Ví dụ

- Line: chọn L → F6 `100,200` → F6 `@300,0` tạo đoạn dài 300 mm.
- Circle: chọn C → F6 `0,0` → F6 `@25<0` tạo đường tròn bán kính 25 mm.
- Rectangle: chọn B → F6 `0,0` → F6 `@6000,4000` tạo hình 6000×4000 mm.
- Polyline: chọn P → lần lượt nhập điểm bằng F6 → Enter để kết thúc;
  Shift+Enter để đóng.
- Move/Copy/Mirror/Scale/Rotate/Offset: chọn đối tượng có thể sửa trước,
  chọn công cụ, sau đó nhập các điểm công cụ yêu cầu. Rotate vẫn dùng hướng từ
  điểm gốc tới điểm thứ hai; đây chưa phải một trường nhập góc độc lập.

Tọa độ được truyền trực tiếp vào core, không đổi sang pixel rồi đổi ngược lại;
Object Snap không kéo lệch điểm nhập. Có thể nhập điểm nằm ngoài vùng nhìn hiện tại;
dùng Zoom Extents để nhìn toàn bộ bản vẽ. Dữ liệu hình học dùng double, không hứa
biểu diễn chính xác tuyệt đối mọi số thập phân ngoài khả năng số dấu phẩy động.

Cancel không thêm điểm hoặc sửa bản vẽ. Dữ liệu sai/NaN/Inf/tràn số bị từ chối
trước khi gọi công cụ; hộp thoại cho phép sửa lại. Thao tác tạo/sửa hoàn tất vẫn
dùng History và persistence hiện có. Nhập tọa độ hiện chỉ dành cho công cụ 2D
nhận điểm; Select/Trim/Extend/Hatch và viewport 3D vẫn dùng luồng tương tác riêng.

## Bằng chứng nghiệm thu

- `coordinate_input_tests.cpp`: Cartesian, relative, polar, whitespace/scientific
  notation, input lỗi, thiếu anchor, overflow, độ chính xác khi save/open, undo/redo.
- `gui_coordinate_input_smoke.ps1`: F6/menu trên ứng dụng Windows thật; Line tọa độ
  phân số ngoài canvas, Circle cực tương đối khi Snap bật, Undo/Redo và Cancel.
- Windows CI chạy bài GUI này cùng các bài tương tác cũ để phát hiện lỗi do tách
  đường nhập chuột và tọa độ. Kết quả phải được đọc từ đúng commit CI.

Đây là bước cải thiện độ chính xác thao tác. Chưa phải command line đầy đủ,
dynamic input cạnh con trỏ, hoặc bằng chứng toàn bộ sản phẩm đạt 8/10.
