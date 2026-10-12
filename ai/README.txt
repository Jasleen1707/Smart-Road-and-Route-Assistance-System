AI Pothole Detection Module

- Detects potholes in images using a trained YOLO11 model.
- Returns the class, confidence score, and bounding-box coordinates.
- Trained model: models/best.pt
- Main code: detect_image.py
- Training code: train.py

Run from the main project folder:
python AI-Handoff/detect_image.py "path/to/road_image.jpg"

GPS integration:
The AI module does not generate GPS coordinates. The GPS module must associate detections with the image's location.

Severity classification is not implemented yet.