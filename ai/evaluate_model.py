
from ultralytics import YOLO

# Load the trained pothole model
model = YOLO("ai/models/best.pt")

# Evaluate on the test dataset
metrics = model.val(
    data="ai/dataset/BharatPotHole/data.yaml",
    split="test",
    imgsz=640,
    conf=0.001
)

# Display evaluation results
print("\n===== POTHOLE MODEL EVALUATION =====")

print(f"Precision: {metrics.box.mp:.4f}")
print(f"Recall: {metrics.box.mr:.4f}")
print(f"mAP50: {metrics.box.map50:.4f}")
print(f"mAP50-95: {metrics.box.map:.4f}")

print("====================================")
