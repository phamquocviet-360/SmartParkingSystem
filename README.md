# iPark System - Hệ Thống Quản Lý Bãi Đỗ Xe Thông Minh

##  Tổng Quan Dự Án
**iPark** là một hệ thống quản lý bãi đỗ xe tự động, sử dụng công nghệ IoT kết hợp với cơ sở dữ liệu đám mây. Hệ thống tự động kiểm soát cổng vào/ra, nhận diện xe, quản lý chỗ đỗ và ghi nhận dữ liệu chi tiết.

##  Tính Năng Chính
- **Kiểm soát cổng tự động**: Mở/đóng cổng thông qua thẻ RFID hoặc nút nhấn thủ công
- **Nhận diện xe**: Sử dụng công nghệ RFID để xác định danh tính xe
- **Quản lý chỗ đỗ**: Giám sát 2 slot bãi đỗ qua cảm biến hồng ngoại
- **Cảnh báo an toàn**: Phát hiện cháy tự động + điều khiển còi báo động
- **Giám sát môi trường**: Theo dõi nhiệt độ, độ ẩm, cảm biến ánh sáng
- **Ghi nhật ký chi tiết**: Lưu trữ mọi hoạt động lên Firebase
- **Hiển thị trạng thái**: Màn hình OLED hiển thị thông tin hệ thống real-time

## ⚙️ Thành Phần Kỹ Thuật

### Hardware
- **Bộ vi điều khiển**: ESP32 (WiFi + Bluetooth)
- **RFID Reader**: MFRC522 (đọc thẻ từ)
- **Cảm biến**: 
  - Cảm biến ngflame (phát hiện lửa)
  - Cảm biến hồng ngoại IR (phát hiện xe)
  - Cảm biến ánh sáng LDR
  - Cảm biến DHT11 (nhiệt độ & độ ẩm)
- **Điều khiển**: Servo motor (cơ chế cổng), LED, Buzzer
- **Display**: OLED 128x64

### Software & Cloud
- **Framework**: Arduino IDE (C++)
- **Kết nối**: WiFi + Firebase Realtime Database
- **Đồng bộ thời gian**: NTP Server
- **Nhận diện biển số**: OpenCV + Haar Cascade (Python)

## 📊 Luồng Hoạt Động
1. **Xe vào**: Quét RFID → Mở cổng → Ghi nhật ký vào Firebase
2. **Xe đỗ**: Cảm biến IR phát hiện → Cập nhật trạng thái chỗ đỗ
3. **Xe ra**: Quét RFID → Xác nhận biển số → Thanh toán → Mở cổng ra
4. **Cảnh báo**: Phát hiện lửa → Kích hoạt còi/LED → Ghi log
5. **Giám sát**: Gửi heartbeat & dữ liệu môi trường mỗi 5 giây

##  Tính Năng Bảo Mật
- Xác thực RFID cho từng xe
- Ghi lại đầy đủ thời gian, UID và hành động
- Giám sát kết nối WiFi & Firebase
- Cảnh báo an toàn trong trường hợp khẩn cấp

##  Dữ Liệu Lưu Trữ
- Nhật ký hành động (Logs)
- Trạng thái cổng (Gate)
- Tình trạng bãi đỗ (Parking)
- Cảnh báo hệ thống (Warnings)
- Dữ liệu môi trường (Environment)

##  Giao Diện
- **OLED Display**: Hiển thị trạng thái cổng, chỗ đỗ, cảnh báo
- **Firebase Dashboard**: Quản lý từ xa qua web/mobile app

## 🚀 Trạng Thái Dự Án
-  Hệ thống cốt lõi hoạt động
-  Tích hợp Firebase
-  Cảm biến đầy đủ
-  Nhận diện biển số (Python OpenCV)
-  Web Dashboard (phát triển)


