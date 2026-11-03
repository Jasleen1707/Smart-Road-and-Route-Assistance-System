from ultralytics import YOLO
from pathlib import Path

checkpoint = Path(
    "ai/public_training/pothole_finetuned_v1/weights/last.pt"
)

if checkpoint.exists():
    print("Resuming training from checkpoint...")
    model = YOLO(str(checkpoint))
    model.train(resume=True)

else:
    print("Starting fine-tuning...")
    model = YOLO(
        "runs/detect/ai/custom_dataset/runs/pothole_custom_v1/weights/best.pt"
    )

    model.train(
        data="ai/Pothole detection.v1i.yolov11/data.yaml",
        epochs=30,
        imgsz=640,
        batch=4,
        patience=10,
        workers=0,
        project="ai/public_training",
        name="pothole_finetuned_v1",
        exist_ok=False
    )

print("Training process finished!")