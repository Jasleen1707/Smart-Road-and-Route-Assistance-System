
from ultralytics import YOLO
from pathlib import Path
import sys

# Load the final fine-tuned model
model = YOLO("ai/models/pothole_final.pt")

# Get image path from command line
if len(sys.argv) < 2:
    print("Usage: python ai/detect_image.py path_to_image")
    sys.exit()

image_path = Path(sys.argv[1])

if not image_path.exists():
    print("Image not found:", image_path)
    sys.exit()

print(f"Testing image: {image_path}")

results = model.predict(
    source=str(image_path),
    save=True,
    conf=0.25,
    classes=[2]
)

print("Image detection completed!")
print("Results saved in the runs/detect folder.")
