from ultralytics import YOLO

model = YOLO(
    "runs/detect/ai/custom_dataset/runs/pothole_custom_v1/weights/best.pt"
)

image = "ai/custom_dataset/annotated/images/WhatsApp Image 2026-11-01 at 4.35.39 PM.jpeg"

results = model.predict(
    source=image,
    conf=0.60,
    save=True
)

print("Custom model test completed!")