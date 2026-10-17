from ultralytics import YOLO

# Load the pretrained YOLO model
model = YOLO("ai/models/yolo11n.pt")

# Train the model on the pothole dataset
model.train(
    data="ai/dataset/BharatPotHole/data.yaml",
    epochs=10,
    imgsz=640,
    batch=8,
    project="ai/models",
    name="pothole_detector"
)