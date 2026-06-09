 iPark System - Hệ Thống Bãi Đỗ Xe Thông Minh
Giới thiệu

iPark System là dự án IoT quản lý bãi đỗ xe thông minh sử dụng ESP32, RFID, nhận diện biển số xe và Firebase. Hệ thống tự động kiểm soát xe ra/vào, giám sát vị trí đỗ xe, tính phí gửi xe và đồng bộ dữ liệu theo thời gian thực lên nền tảng đám mây.

Chức năng chính
Xác thực phương tiện bằng thẻ RFID.
Nhận diện biển số xe bằng OpenCV và Tesseract OCR.
Điều khiển cổng tự động bằng Servo.
Giám sát trạng thái chỗ đỗ xe bằng cảm biến IR.
Tính phí gửi xe và quản lý lịch sử ra/vào.
Đồng bộ dữ liệu thời gian thực với Firebase.
Cảnh báo cháy bằng cảm biến lửa.
Hiển thị thông tin hệ thống trên màn hình OLED.
Công nghệ sử dụng

Firmware & Embedded

ESP32
Arduino Framework
SPI, I2C
RFID RC522
Firebase Realtime Database

Computer Vision

Python
OpenCV
Tesseract OCR
Kết quả đạt được
Xây dựng thành công hệ thống bãi đỗ xe tự động hoàn chỉnh.
Kết nối và đồng bộ dữ liệu giữa ESP32, Firebase và ứng dụng Python.
Nhận diện biển số xe và xác thực phương tiện trước khi cho phép ra/vào bãi đỗ.
