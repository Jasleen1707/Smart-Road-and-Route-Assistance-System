
from ultralytics import YOLO
import cv2
import time
from collections import deque

# Load final fine-tuned model
model = YOLO("ai/models/pothole_final.pt")

# Open webcam
camera = cv2.VideoCapture(0)

if not camera.isOpened():
    print("Camera could not be opened!")
    exit()

print("Smart Road Assistance Started")
print("Press Q to close")

fps_history = deque(maxlen=20)
previous_time = time.time()

while True:
    success, frame = camera.read()

    if not success:
        print("Could not read camera")
        break

    # Detect potholes only (class index 2)
    results = model.predict(
        frame,
        conf=0.25,
        classes=[2],
        verbose=False
    )

    output = results[0].plot()

    current_time = time.time()
    fps = 1 / max(current_time - previous_time, 0.001)
    previous_time = current_time

    fps_history.append(fps)
    average_fps = sum(fps_history) / len(fps_history)

    pothole_count = len(results[0].boxes)

    if pothole_count > 0:
        status = "POTHOLE DETECTED"
    else:
        status = "NO POTHOLE DETECTED"

    cv2.putText(
        output,
        f"FPS: {average_fps:.1f}",
        (20, 35),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (0, 255, 0),
        2
    )

    cv2.putText(
        output,
        f"Potholes: {pothole_count}",
        (20, 70),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (0, 255, 0),
        2
    )

    cv2.putText(
        output,
        status,
        (20, 105),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (0, 0, 255) if pothole_count > 0 else (0, 255, 0),
        2
    )

    cv2.imshow("Smart Road Assistance - AI Detection", output)

    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

camera.release()
cv2.destroyAllWindows()

print("Detection stopped.")
