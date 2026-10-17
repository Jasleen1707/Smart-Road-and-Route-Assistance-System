from ultralytics import YOLO
from pathlib import Path

# Load the existing pretrained model
model = YOLO("ai/models/best.pt")

# Find a test image
image_folder = Path("ai/dataset/BharatPotHole/test/images")
image_path = next(
    (p for p in image_folder.iterdir()
     if p.suffix.lower() in [".jpg", ".jpeg", ".png"]),
    None
)

if image_path is None:
    print("No test images found.")
else:
    print(f"Testing image: {image_path}")

    results = model.predict(
        source=str(image_path),
        save=True,
        conf=0.25
    )

    print("Image test completed.")
    print("Pothole detection test completed using best.pt.")