# Kiểm tra ổn định dữ liệu — 2026-09-25

## Mục đích và phạm vi

Bảo vệ bản vẽ và bản khôi phục trước khi mở rộng tính năng. Baseline:
`394dacc2b7bf9e826b838db5eb8e2da2a8ab1016` trên main. Đây là đợt kiểm tra
lõi lưu/đọc/khôi phục và chạy lại các bộ kiểm thử hiện có; không phải chứng nhận
toàn bộ phần mềm đã hoàn thiện hoặc không còn lỗi.

## Các lỗi tái hiện được

| Mã | Hiện tượng và cách tái hiện | Nguyên nhân gốc | Sửa và kiểm tra chống tái phát |
|---|---|---|---|
| DATA-001 | Ghi snapshot tốt, sau đó autosave tài liệu tham chiếu block không tồn tại hoặc tỷ lệ in âm: báo thành công nhưng snapshot mới không đọc được | Recovery tự ghi/thay file, bỏ qua xác thực mà Save đã có | Dùng chung `save_project_atomic`; kiểm tra trả về thất bại, byte cũ không đổi và bản cũ vẫn mở được |
| DATA-002 | POLY/HATCH/B khai báo số phần tử SIZE_MAX hoặc -1 gây ngoại lệ `vector::reserve` | Cấp phát theo số lượng trong file trước khi đọc dữ liệu | Chỉ tăng bộ nhớ theo phần tử đọc thành công; kiểm tra cả polyline trong block, hatch và block |
| DATA-003 | Có đối tượng sau END nhưng bộ đọc vẫn báo thành công và bỏ đối tượng | Trả kết quả ngay ở END mà không kiểm tra phần dư | Chỉ cho phép khoảng trắng sau END; kiểm tra từ chối phần dư và tương thích khoảng trắng/CRLF |
| DATA-004 | Trên POSIX, lưu vào đường dẫn trỏ đến thư mục rỗng có thể xóa thư mục rồi tạo file ở đó | Khi rename thất bại, fallback xóa đích trước khi thử lại | Bỏ fallback phá hủy; kiểm tra Save và Recovery thất bại, giữ nguyên thư mục |

Ngoài ra, việc ghi file kiểm tra cả lỗi lúc đóng stream trước khi xác thực và thay đích.
DATA-004 là lỗi nhánh POSIX; không gán lỗi này cho API thay file Windows.

## Quá trình và bằng chứng

1. Tải main hiện tại, đọc chính sách ổn định và nhánh; tạo nhánh mới từ main.
2. Build 33 tệp nguồn lõi bằng GCC 13.3.0.
3. Chạy bộ hồi quy mới với lõi chưa sửa: 15 assertion thất bại, bao phủ 4 nguyên nhân trên.
4. Bộ kiểm thử cũ: 21/21 đạt. Điều này cho thấy kiểm thử cũ chưa bao phủ các lỗi mới.
5. Sửa tại lớp persistence/recovery, bỏ cơ chế autosave trùng lặp.
6. Đăng ký `acp_persistence_safety_tests` trong CTest để Windows CI và OpenCASCADE CI tự chạy.
7. Build Release sau sửa: 22/22 bộ đạt; lặp 5 lần: 110 lượt đạt.
8. `git diff --check` đạt.

Kiểm thử cục bộ chạy trên Linux, `ACP_BUILD_APP=OFF`, backend BRep tương thích.
Font Noto Sans JP đúng URL được ghim trong CMake được dùng cho kiểm thử PDF.
`CMAKE_RC_COMPILER=/bin/true` chỉ dùng cho build lõi cục bộ vì project khai báo RC
nhưng Linux không build executable/resource Windows; không coi đây là build Windows.

## Giới hạn và cổng hoàn tất

- Bản sửa cần Windows CI xanh (MSVC, CTest lặp, GUI, package và installer),
  OpenCASCADE CI và review trước khi tích hợp main theo chính sách repo.
- Kết quả chính xác của CI gắn với commit/PR, không suy từ kết quả của main cũ.
- Các bộ hiện có kiểm tra hình học 2D, chỉnh sửa, thuộc tính/layer, Undo/Redo,
  lưu/đọc, PDF, bản vẽ lớn, 3D/Blender, kiến trúc và cache. Chúng không bao phủ
  mọi thao tác GUI, mọi file ngoài thực tế hay mọi trường hợp thiếu tài nguyên.
- Chưa xác minh mất điện đột ngột, truy cập đồng thời cùng đường dẫn tạm, ổ đĩa mạng,
  lỗi phần cứng hoặc giới hạn bộ nhớ với file thực sự rất lớn.
- Quy trình chống tái phát: mỗi lỗi có fixture tái hiện, sửa tại lớp chịu trách nhiệm,
  CTest bắt buộc, cổng Windows và đối chiếu main trước merge. Không cam kết tuyệt đối
  rằng phần mềm sẽ không bao giờ phát sinh lỗi mới.

## Phần phát triển tiếp theo

Tiếp tục kiểm tra bất biến hình học, định danh đối tượng và thao tác GUI trên bản vẽ
thực tế; chỉ mở rộng tính năng khi các cổng liên quan đạt. Các hạng mục roadmap như
constraints, IFC/DWG đầy đủ, GPU picking và hệ plugin vẫn cần triển khai/xác minh
riêng; không coi việc các bài kiểm thử lõi đạt là đã hoàn thành các hạng mục đó.
