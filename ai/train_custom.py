from ultralytics import YOLO

# Load the existing pothole model
model = YOLO("ai/models/best.pt")

# Train on our custom dataset
model.train(
    data="ai/custom_dataset/data.yaml",
    epochs=30,
    imgsz=640,
    batch=4,
    patience=10,
    workers=0,
    project="ai/custom_dataset/runs",
    name="pothole_custom_v1"
)

print("Custom pothole training completed!")