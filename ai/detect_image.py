from pathlib import Path
from ultralytics import YOLO

# Load our trained pothole detector
model = YOLO("runs/detect/ai/models/pothole_detector/weights/best.pt")

# Automatically choose the first test image
image_path = next(Path("ai/dataset/test/images").glob("*.jpg"))

print(f"Testing image: {image_path}")

# Run pothole detection
results = model.predict(
    source=str(image_path),
    save=True,
    conf=0.25
)

print("Pothole detection completed!")