# Auto CAD Pro — đánh giá sản phẩm và kế hoạch đạt tối thiểu 8/10

Ngày đánh giá: 2026-09-25. Baseline main: `394dacc2b7bf9e826b838db5eb8e2da2a8ab1016`.

## Kết luận

Chưa ngang AutoCAD/QCAD Professional về CAD 2D phục vụ hồ sơ xây dựng, chưa ngang
FreeCAD về mô hình tham số. Hiện là nền tảng CAD đang phát triển, có executable
Windows và nhiều bài kiểm thử tự động, nhưng chưa đủ để thay thế phần mềm chuyên
nghiệp trong toàn bộ quy trình. Không nâng điểm chỉ vì build xanh hoặc có icon.

Mục tiêu người dùng: **tất cả tiêu chí từ 8/10**, không lấy điểm trung bình bù cho
tiêu chí dưới chuẩn. Đạt chuẩn 2D không được gọi là đã đạt chuẩn 3D/BIM.

## Phương pháp và giới hạn

Đọc code core/GUI/persistence, workflow CI và tests; xem ảnh chụp giao diện thật
`AutoCADPro-used-ui.png` của Windows run 36108083860; đối chiếu tài liệu chính thức
của các sản phẩm tham chiếu. Không trực tiếp chạy AutoCAD/QCAD/FreeCAD trong phiên này.
Điểm dưới đây là **ước lượng kỹ thuật theo bằng chứng hiện có**, không phải kết quả
khảo sát người dùng hoặc benchmark độc lập. Chưa đo FPS/độ trễ trên máy đích của người dùng.

Thang điểm: 0 = chưa có; 2 = nền tảng rời rạc; 4 = làm được tác vụ đơn giản nhưng thiếu
quy trình thiết yếu; 6 = quy trình cơ bản có test, còn thiếu/rủi ro đáng kể;
8 = đáp ứng đầy đủ bộ nghiệm thu bên dưới trên Windows và bản vẽ thực tế;
10 = vượt chuẩn tham chiếu bằng đo lường. Không tự cấp 8 khi còn thiếu bằng chứng.

## Điểm và điều kiện đạt 8

| Tiêu chí | Điểm tạm thời | Bằng chứng/khoảng thiếu | Điều kiện tối thiểu 8/10 |
|---|---:|---|---|
| Độ chính xác và an toàn dữ liệu | 5/10 | Có Undo/Redo, atomic save; vừa phát hiện Mirror block, tràn ID và autosave lỗi | Không còn lỗi P0/P1 trong bộ nghiệm thu; hình học tham chiếu, import/export, 100 vòng undo/redo và save/open giữ nguyên dữ liệu được hỗ trợ |
| Năng suất vẽ/chỉnh sửa 2D | 4/10 | Selected chỉ là optional một EntityId; Trim/Extend cho line; thiếu nhiều công cụ phổ biến | Multi-select window/crossing, nhập tọa độ tuyệt đối/tương đối/polar, Ortho/tracking, Fillet/Chamfer/Array/Stretch/Join/Explode; đủ tác vụ hồ sơ mẫu |
| UI/UX và khả năng đọc | 4/10 | Toolbar gọn, panel ẩn/ghim/kéo được; ảnh thật có icon nhỏ và nhãn bị cắt | Font UI rõ, tooltip đầy đủ, nhập lệnh/giá trị rõ ràng; DPI 100/150/200%, 1366×768 và 1920×1080 không cắt điều khiển; hoàn thành ≥90% tác vụ không cần hướng dẫn |
| Trao đổi dữ liệu CAD | 2/10 | DXF chỉ tập con; Block/Dimension/Hatch bị bỏ khi export có cảnh báo; chưa có DWG/IFC đầy đủ | Ma trận định dạng công bố rõ; đối tượng được hỗ trợ round-trip đúng; mọi mất mát có báo cáo; DWG/IFC cần adapter và bộ file đối chiếu riêng |
| Hồ sơ, kích thước và in | 4/10 | Có PDF Unicode, A4–A0 và scale; chỉ linear dimension, chưa có hệ sheets/viewports hoàn chỉnh | Dimstyle, radial/angular/ordinate dimensions, nhiều sheet/viewport, khung tên, print preview; 1:50/100/200 kiểm tra kích thước đầu ra |
| Hiệu năng và độ bền | 3/10 | Test 5.120 đối tượng với ngân sách 10 giây cho toàn workflow; chưa là benchmark tương tác | Trên máy Windows công bố cấu hình: fixture 10k/100k, chọn/snap p95 ≤100 ms, pan/zoom ≥30 FPS; mở/lưu trong ngân sách đã chốt; phiên thao tác dài không crash |
| 3D/BIM | 3/10 | Có mesh, kiến trúc, BRep adapter và ProjectModel; GUI giữ Scene thay vì ProjectModel làm nguồn dữ liệu chính | Model tham số được lưu/mở/undo; pick/sửa phần tử; level/type/material/host hợp lệ; section/elevation/schedule liên kết; IFC đối chiếu |
| Mở rộng và tự động hóa | 1/10 | Chưa có registry/plugin SDK/scripting hoàn chỉnh | Command/Tool/Importer registry, API có version, script mẫu chạy thật và kiểm soát lỗi plugin |
| Build, phân phối, vận hành | 7/10 | CI Windows, portable, NSIS và GUI test có bằng chứng | Clean install/update/uninstall, hồ sơ phát hành, crash report có kiểm soát, hướng dẫn sử dụng, ký số nếu phát hành yêu cầu; mọi gate đúng commit xanh |

Các điểm này giữ nguyên sau bản sửa tập trung: sửa vài lỗi không đồng nghĩa đã có
đủ bằng chứng để nâng toàn bộ nhóm lên 8. Ngưỡng hiệu năng ở trên là mục tiêu nghiệm
thu đề xuất, chưa phải số đo đã đạt.

## Đối chiếu đúng nhóm sản phẩm

| Sản phẩm | Điểm tham chiếu | Khoảng cách của Auto CAD Pro |
|---|---|---|
| AutoCAD | Dynamic blocks; model/paper space và viewport theo layout | Chưa có block động, nhiều layout/viewport và quy trình tham số tương ứng |
| QCAD Professional | Bộ công cụ 2D rộng, multi-select, block, kích thước, DXF/DWG, in | Chưa đủ công cụ/định dạng và tốc độ thao tác để thay thế cho công việc 2D phổ thông |
| FreeCAD | Parametric model, Sketcher constraint solver, TechDraw/BIM | ProjectModel còn là nền tảng; chưa có toàn bộ quy trình authoring, tài liệu liên kết và ràng buộc |

Không gán tính năng thương mại QCAD Professional cho Community Edition. Không coi
OpenCASCADE là toàn bộ FreeCAD. `docs/UPSTREAM_COMPONENTS.md` ghi rõ nền tảng này
chưa sao chép mã FreeCAD; không gọi sản phẩm là một bản FreeCAD đầy đủ đổi giao diện.

Nguồn chính thức tra ngày 2026-09-25:
- https://www.qcad.org/en/documentation/features
- https://help.autodesk.com/cloudhelp/2026/HUN/AutoCAD-DidYouKnow/files/GUID-2A3D92B8-20E2-47B3-92CE-FB3EB03888C3.htm
- https://help.autodesk.com/view/ACD/2026/ENU/?caas=caas%2Fdocumentation%2FCIV3D%2F2014%2FENU%2FfilesACD%2FGUID-990538B6-DDA1-4190-BCC0-BB5BA94C9879-htm.html
- https://www.freecad.org/features.php
- https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/TechDraw_Workbench.md

## Lỗi đã tái hiện và thay đổi trong đợt này

| Mã | Nguyên nhân | Cách sửa | Hồi quy |
|---|---|---|---|
| GEO-001 | Mirror block đổi rotation/insertion nhưng thiếu handedness | Thêm cờ mirrored; phản chiếu local Y trước rotate; đổi chiều và góc arc; lưu bằng BLOCKREF_MIRRORED | So từng primitive của block bất đối xứng với phép phản chiếu độc lập trên 3 trục; mirror 2 lần; save/open; history; GUI mirror/undo/redo |
| DATA-005 | Counter unsigned có thể về 0 sau import ID cực đại | Bộ cấp ID dùng chung bỏ qua 0, tránh ID đã dùng, tìm slot trống sau wrap | Entity/Layer/Block max ID; không sinh ID 0; thử không gian ID nhỏ đầy và có lỗ trống |
| BIM-001 | set_host chỉ cấm tự host, không cấm chu trình gián tiếp | Duyệt chuỗi tổ tiên trước mutation, có giới hạn duyệt | Chu trình 2/3 phần tử bị từ chối và giữ host cũ |
| FEAT-001 | Offset GUI chỉ áp dụng line | Thêm circle/arc offset và through-point mode dùng chung; giữ style, dùng active layer | Bán kính trong/ngoài, collapse, NaN/Inf, sweep, history, save/open; GUI circle offset |

Trước sửa: bộ product_integrity có 19 assertion thất bại, xác nhận 3 nhóm lỗi mới.
Các sửa DATA-001..004 từ đợt trước được giữ lại trên nhánh này để không đánh mất
phần bảo toàn dữ liệu. Xem `PERSISTENCE_AUDIT_2026-09-25.md`.

Tương thích: file cũ vẫn đọc được; block không mirror giữ token BLOCKREF cũ.
File có BLOCKREF_MIRRORED cần bản mới; bản cũ sẽ từ chối token thay vì mở sai hình.
DXF export block vẫn là hạn chế đã công bố, không được coi là đã giải quyết.

## Kế hoạch triển khai theo phụ thuộc

| Đợt | Việc thực hiện | Kết quả phải nghiệm thu |
|---|---|---|
| A — bảo vệ nền tảng | Hoàn thành các sửa ở trên; validator hình học đầu vào; rà ID, host/type/material, sửa mất thuộc tính khi biến đổi | Không P0/P1 trong fixture; negative tests bắt lỗi; Windows/OCCT xanh đúng commit |
| B — thao tác 2D | SelectionSet + transaction nhóm; window/crossing; nhập số và tọa độ; Ortho/polar; nâng Offset/Trim/Extend, Fillet/Chamfer/Array | 10 tác vụ vẽ cơ bản và 10 tác vụ chỉnh sửa thực tế, undo toàn tác vụ một lần |
| C — workspace | Tách command/controller khỏi main_win.cpp; toolbar theo nhóm, font Segoe UI, tooltip, property editor có bố cục co giãn; giữ cấu hình panel | Screenshot/interaction tại DPI và kích thước màn hình đã định, không label bị che; keyboard hoàn chỉnh |
| D — hồ sơ 2D | Dimstyle, ký hiệu, block nhiều đối tượng, layouts/viewports, khung tên, PDF preview/batch | 3 hồ sơ mẫu: mặt bằng, chi tiết kết cấu, chi tiết cơ khí; in đúng tỷ lệ và Unicode |
| E — trao đổi file | DXF BLOCK/INSERT/DIMENSION/HATCH + fixture ngoài repo; DWG adapter sau đánh giá dependency/license | Mỗi format có bảng hỗ trợ và round-trip; không bỏ dữ liệu âm thầm |
| F — hiệu năng | Spatial index, cache render, phân đoạn redraw; đo p50/p95 và RAM trên fixture 10k/100k | Đạt ngân sách hiệu năng đã định, kèm cấu hình máy và raw measurements |
| G — BIM | ProjectModel làm nguồn chính; persistence/version migration; BIM history; authoring/property/picking; constraints/IFC/tài liệu liên kết | Mô hình nhà nhiều tầng sửa được, lưu/mở không mất semantic data, bản vẽ liên kết cập nhật đúng |
| H — hệ sinh thái/phát hành | Registry/SDK, script, hướng dẫn Việt/Nhật, bộ mẫu, release/update và crash diagnostics | Cài/chạy độc lập; plugin mẫu; review sử dụng thực tế; tất cả tiêu chí ≥8 |

Không đưa hạn hoàn tất hoặc phần trăm hoàn thành giả khi chưa lượng hóa backlog.
Mỗi đợt là phần phát triển đáng kể, không phải một lần đổi giao diện. Các dependency
thương mại (nếu chọn) và ký số cần được giải quyết riêng trước khi hứa phát hành.

## Bộ nghiệm thu 8/10

1. Chuẩn bị 3 hồ sơ mẫu tự tạo (mặt bằng nhà, chi tiết kết cấu, cơ khí), cùng bộ
   file trao đổi được phép sử dụng từ ít nhất hai công cụ tham chiếu.
2. Mỗi tác vụ có đầu vào, thao tác, hình học/kích thước mong đợi, thời gian mục tiêu,
   Undo/Redo và save/open. Không dùng việc file “khác trước” làm bằng chứng duy nhất.
3. Mọi nhóm chức năng bắt buộc đạt ≥90% tác vụ; 100% tác vụ an toàn dữ liệu đạt;
   không có lỗi P0/P1 mở. Các tính năng thiếu không được chấm bằng điểm giả định.
4. Ít nhất hai người dùng có kinh nghiệm CAD thực hiện tác vụ; đo độ hoàn thành,
   thời gian và lỗi. Đây là bằng chứng còn cần thu thập, không giả nhận đã có.
5. Chỉ công bố đạt 8/10 khi từng hàng trong bảng có hồ sơ nghiệm thu. Nếu chỉ đạt
   phạm vi 2D, công bố rõ 2D; phần BIM/plugin chưa đạt tiếp tục đánh dấu chưa đạt.
