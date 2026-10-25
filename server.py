
from http.server import HTTPServer, SimpleHTTPRequestHandler
import json

class GPSHandler(SimpleHTTPRequestHandler):

    def do_POST(self):
        if self.path != "/save-location":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", 0))
        data = json.loads(self.rfile.read(length))

        latitude = float(data["latitude"])
        longitude = float(data["longitude"])

        if not (-90 <= latitude <= 90 and
                -180 <= longitude <= 180):
            self.send_error(400, "Invalid coordinates")
            return

        with open("location.txt", "w") as file:
            file.write(f"{latitude} {longitude}\n")

        response = json.dumps({
            "message": "Coordinates saved successfully",
            "latitude": latitude,
            "longitude": longitude
        }).encode()

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.wfile.write(response)

        print("Saved:", latitude, longitude)

server = HTTPServer(("127.0.0.1", 8000), GPSHandler)
print("GPS server running at http://127.0.0.1:8000")
server.serve_forever()
