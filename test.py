import cv2
import datetime
import firebase_admin
from firebase_admin import credentials, db
from pytesseract import pytesseract
from PIL import Image
import re
import time

# === Firebase setup ===
cred = credentials.Certificate(r"C:\Users\Viet\Downloads\biensoxe-e00fa-firebase-adminsdk-fbsvc-511fcd4b4e.json")
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://biensoxe-e00fa-default-rtdb.firebaseio.com/"
})

# === OCR setup ===
pytesseract.tesseract_cmd = r"C:\Program Files\Tesseract-OCR\tesseract.exe"

def log_to_firebase(uid, message):
    ref = db.reference("Logs")
    log_entry = {
        "timestamp": int(time.time() * 1000),
        "uid": uid,
        "message": message
    }
    ref.push(log_entry)

def clean_number_plate(text):
    text = ''.join(text.split())
    match = re.search(r"([0-9]{2}[A-Z]-[0-9]{3}\.[0-9]{2})", text)
    return match.group(0) if match else None

def get_waiting_uid():
    ref = db.reference("ParkingSystem/RFID")
    data = ref.get()
    if data:
        for uid, value in data.items():
            if value.get("waiting_plate") == True:
                return uid
    return None

def get_confirm_uid():
    ref = db.reference("ParkingSystem/RFID")
    data = ref.get()
    if data:
        for uid, value in data.items():
            if value.get("confirm_plate") == True and value.get("number_plate"):
                return uid, value["number_plate"]
    return None, None

def read_number_plate_from_image(image_path="images/numberplate.jpg"):
    image = Image.open(image_path)
    text = pytesseract.image_to_string(image, lang="eng").strip()
    return clean_number_plate(text)

def get_fee_config():
    """Đọc cấu hình giá từ Firebase"""
    ref = db.reference("/Config/parking_fee")
    config = ref.get()
    if config:
        return config
    # Giá trị mặc định
    return {
        "rate_per_hour": 10000,
        "rate_per_minute": 0,
        "rounding": "ceil",
        "min_fee": 10000
    }

def calculate_fee(date_in_str):
    """Tính phí dựa trên cấu hình"""
    config = get_fee_config()
    try:
        date_in = datetime.datetime.strptime(date_in_str, "%Y/%m/%d %H:%M:%S")
    except:
        try:
            date_in = datetime.datetime.strptime(date_in_str, "%Y-%m-%d %H:%M:%S")
        except:
            return 0

    now = datetime.datetime.now()
    delta = now - date_in
    total_seconds = delta.total_seconds()
    hours = total_seconds / 3600
    minutes = total_seconds / 60

    # Tính phí theo cấu hình
    if config.get("rate_per_minute", 0) > 0:
        # Tính theo phút
        fee = minutes * config["rate_per_minute"]
    else:
        # Tính theo giờ với làm tròn
        if config["rounding"] == "ceil":
            hours_rounded = int(hours) + (1 if hours - int(hours) > 0 else 0)
        elif config["rounding"] == "floor":
            hours_rounded = int(hours)
        else:  # exact
            hours_rounded = hours
        fee = hours_rounded * config["rate_per_hour"]

    min_fee = config.get("min_fee", 0)
    return max(int(fee), min_fee)

def handle_plate_entry(detected_plate):
    uid = get_waiting_uid()
    if uid:
        ref = db.reference(f"ParkingSystem/RFID/{uid}")
        ref.update({
            "number_plate": detected_plate,
            "waiting_plate": False,
            "status": 0,
            "date_in": datetime.datetime.now().strftime("%Y/%m/%d %H:%M:%S")
        })
        print(f"✅ Gán biển số {detected_plate} cho UID {uid} (Xe vào)")
        log_to_firebase(uid, f"✅ Gán biển số {detected_plate} cho UID {uid} (Xe vào)")
    else:
        print("⚠️ Không có UID nào đang chờ vào!")
        log_to_firebase("unknown", "⚠️ Không có UID nào đang chờ vào!")

def handle_plate_exit(detected_plate):
    uid, stored_plate = get_confirm_uid()
    if uid:
        print(f"🆚 So sánh biển số: {detected_plate} với {stored_plate}")
        log_to_firebase(uid, f"🆚 So sánh biển số: {detected_plate} với {stored_plate}")

        ref = db.reference(f"ParkingSystem/RFID/{uid}")
        data = ref.get()
        if not data:
            return

        if detected_plate == stored_plate:
            # Lấy thời gian vào và tính phí
            date_in = data.get("date_in")
            if not date_in:
                print("❌ Không có thời gian vào!")
                log_to_firebase(uid, "❌ Không có thời gian vào!")
                return

            fee = calculate_fee(date_in)
            balance = data.get("balance", 0)

            print(f"💰 Phí: {fee} VND, Số dư: {balance} VND")

            if balance >= fee:
                # Đủ tiền: trừ và mở cổng
                new_balance = balance - fee
                ref.update({
                    "status": 1,
                    "confirm_plate": False,
                    "date_out": datetime.datetime.now().strftime("%Y/%m/%d %H:%M:%S"),
                    "balance": new_balance,
                    "waiting_payment": False,
                    "fee_due": 0
                })

                # Gửi tín hiệu mở cổng
                gate_ref = db.reference("Gate")
                gate_ref.update({
                    "exit_open": True,
                    "exit_uid": uid,
                    "insufficient_balance": False
                })

                print(f"✅ Thanh toán thành công! Số dư còn: {new_balance} VND")
                log_to_firebase(uid, f"✅ Thanh toán {fee} VND thành công, mở cổng")
            else:
                # Thiếu tiền: set cờ chờ nạp
                ref.update({
                    "waiting_payment": True,
                    "fee_due": fee,
                    "confirm_plate": False
                })
                gate_ref = db.reference("Gate")
                gate_ref.update({
                    "insufficient_balance": True
                })

                print(f"❌ Số dư không đủ! Cần {fee} VND, hiện có {balance} VND")
                log_to_firebase(uid, f"❌ Số dư không đủ! Cần {fee} VND, hiện có {balance} VND")
        else:
            print("❌ Biển số không khớp! Không cho mở cổng.")
            log_to_firebase(uid, "❌ Biển số không khớp! Không cho mở cổng.")
    else:
        print("⚠️ Không có UID nào đang chờ xác nhận ra!")
        log_to_firebase("unknown", "⚠️ Không có UID nào đang chờ xác nhận ra!")

# === Mở camera và xử lý biển số ===
cam = cv2.VideoCapture(0)
plate_cascade = cv2.CascadeClassifier("haarcascade_russian_plate_number.xml")

while True:
    ret, frame = cam.read()
    frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    plates = plate_cascade.detectMultiScale(frame_gray, scaleFactor=1.05, minNeighbors=3)

    for (x, y, w, h) in plates:
        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 255), 2)
        plate_img = frame[y:y + h, x:x + w]
        gray_plate = cv2.cvtColor(plate_img, cv2.COLOR_BGR2GRAY)

        cv2.imshow("Biển số xe", gray_plate)

        if cv2.waitKey(1) == ord('s'):
            cv2.imwrite("images/numberplate.jpg", gray_plate)
            print("📷 Đã chụp ảnh biển số. Đang xử lý OCR...")
            log_to_firebase("unknown", "📷 Đã chụp ảnh biển số. Đang xử lý OCR...")

            plate = read_number_plate_from_image()
            if plate:
                print("🔍 Biển số phát hiện:", plate)
                log_to_firebase("unknown", f"🔍 Biển số phát hiện: {plate}")

                uid, stored_plate = get_confirm_uid()
                if uid:
                    handle_plate_exit(plate)
                else:
                    handle_plate_entry(plate)

            else:
                print("❌ Không nhận diện được biển số!")
                log_to_firebase("unknown", "❌ Không nhận diện được biển số!")

    if cv2.waitKey(1) == ord('q'):
        break

    cv2.imshow("Camera", frame)

cam.release()
cv2.destroyAllWindows()