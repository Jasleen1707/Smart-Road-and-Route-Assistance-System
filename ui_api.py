"""Backend add-on for the dashboard. At the bottom of main.py:
        from ui_api import install
        install(app)
Needs: pip install python-multipart pillow ultralytics"""
import io, os, sys, time, threading, subprocess
from fastapi import APIRouter, File, UploadFile, HTTPException
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

BASE = os.path.dirname(os.path.abspath(__file__))
router = APIRouter()
_model = None

# ---------------------------------------------------------------------------
# OS CORE: runs in the background from server start (works on Windows too).
# Mirrors os_module.c: frame packets carry GPS, a 16-slot alert queue guarded by
# two counting semaphores (sem_empty / sem_full) + a mutex, and a consumer thread
# that always dispatches the highest priority first.   Priorities: 4 SAFETY, 3 DIVERSION, 2 POTHOLE, 1 SERVICE
# ---------------------------------------------------------------------------
class OsCore:
    CAP = 16
    def __init__(self):
        self.lock = threading.Lock()
        self.empty = threading.Semaphore(self.CAP)      # sem_empty: free slots
        self.full = threading.Semaphore(0)              # sem_full : queued alerts
        self.q, self.hist, self.cur = [], [], None
        self.frames_in = self.frames_done = self.dispatched = self.dropped = self.wait_sum = self.seq = 0
        self.gps = {"lat": 0.0, "lon": 0.0, "updates": 0}
        self.t0 = time.monotonic()
        threading.Thread(target=self._alert_loop, daemon=True, name="alert_thread").start()

    def set_gps(self, lat, lon):                        # gps_thread: mutex-protected register
        with self.lock: self.gps = {"lat": lat, "lon": lon, "updates": self.gps["updates"] + 1}
    def frame_in(self):                                 # camera_thread: one frame packet enters
        with self.lock: self.frames_in += 1
    def frame_done(self):                               # detector: inference finished
        with self.lock: self.frames_done += 1

    def submit(self, prio, label, conf=0.0, lat=0.0, lon=0.0):
        if not self.empty.acquire(blocking=False):      # queue full -> drop
            with self.lock: self.dropped += 1
            return False
        with self.lock:
            self.seq += 1
            self.q.append({"id": self.seq, "prio": prio, "label": label, "t": time.monotonic()})
        self.full.release()
        return True

    def _alert_loop(self):
        while True:
            self.full.acquire()
            with self.lock:                             # priority scheduling: highest prio, oldest first
                i = max(range(len(self.q)), key=lambda k: (self.q[k]["prio"], -self.q[k]["t"]))
                a = self.q.pop(i)
                a["wait_ms"] = int((time.monotonic() - a["t"]) * 1000); self.cur = a
            self.empty.release()
            time.sleep(0.4)                             # time taken to show/speak the alert
            with self.lock:
                self.cur = None; self.dispatched += 1; self.wait_sum += a["wait_ms"]
                self.hist.insert(0, a); del self.hist[8:]

    def state(self):
        with self.lock:
            now = time.monotonic()
            v = lambda a: {"id": a["id"], "prio": a["prio"], "label": a["label"], "wait_ms": a.get("wait_ms", int((now - a["t"]) * 1000))}
            exe = os.path.exists(os.path.join(BASE, "os_module")) and not sys.platform.startswith("win")
            return {"mode": "Python port of os_module.c" + (" (C binary also available)" if exe else ""),
                    "uptime": int(now - self.t0), "cap": self.CAP, "sem_full": len(self.q), "sem_empty": self.CAP - len(self.q),
                    "frames_in": self.frames_in, "frames_done": self.frames_done, "gps": dict(self.gps),
                    "dispatched": self.dispatched, "dropped": self.dropped,
                    "avg_wait_ms": int(self.wait_sum / self.dispatched) if self.dispatched else 0,
                    "queue": [v(a) for a in sorted(self.q, key=lambda a: (-a["prio"], a["t"]))],
                    "current": v(self.cur) if self.cur else None, "history": [v(a) for a in self.hist]}

core = OsCore()

class AlertIn(BaseModel):
    prio: int
    label: str
    conf: float = 0.0
    lat: float = 0.0
    lon: float = 0.0

@router.get("/os/live")
def os_live():
    return core.state()

@router.post("/os/submit")
def os_submit(a: AlertIn):
    return {"queued": core.submit(a.prio, a.label, a.conf, a.lat, a.lon)}

@router.get("/os/run")                # run the compiled C module (Linux/WSL) or Python OS concurrency core
def os_run():
    exe = os.path.join(BASE, "os_module")
    if os.path.exists(exe) and not sys.platform.startswith("win"):
        p = subprocess.run([exe], cwd=BASE, capture_output=True, text=True, timeout=30)
        return {"output": p.stdout + p.stderr}
    script = os.path.join(BASE, "run_os.py")
    if os.path.exists(script):
        p = subprocess.run([sys.executable, script], cwd=BASE, capture_output=True, text=True, timeout=30)
        return {"output": p.stdout + p.stderr}
    raise HTTPException(404, "OS runner not found")

# ---------------------------------------------------------------------------
def _yolo():
    global _model
    if _model is None:
        from ultralytics import YOLO
        _model = YOLO(os.path.join(BASE, "ai", "models", "pothole_final.pt"))
    return _model

@router.post("/ai/detect")            # AI: one camera frame in -> boxes, confidence, severity out
async def ai_detect(file: UploadFile = File(...)):
    from PIL import Image
    core.frame_in()
    img = Image.open(io.BytesIO(await file.read())).convert("RGB")
    try:
        model = _yolo()
    except Exception as e:
        raise HTTPException(503, f"YOLO model unavailable: {e}")
    keep = [i for i, n in model.names.items() if "pothole" in str(n).lower()] or None
    r = model.predict(img, conf=0.25, classes=keep, verbose=False)[0]
    w, h = img.size
    dets = []
    for b in r.boxes:
        x1, y1, x2, y2 = [float(v) for v in b.xyxy[0]]
        area = (x2 - x1) * (y2 - y1) / (w * h)
        dets.append({"confidence": float(b.conf[0]), "box": [x1, y1, x2, y2], "area": area,
                     "severity": "High" if area >= 0.10 else "Medium" if area >= 0.03 else "Low"})
    core.frame_done()
    return {"width": w, "height": h, "detections": dets}

@router.post("/gps/file")             # GPS: location.txt as "lat lon" for gps_module.c / os_module.c + OS core
def gps_file(latitude: float, longitude: float):
    core.set_gps(latitude, longitude)
    with open(os.path.join(BASE, "location.txt"), "w") as f:
        f.write(f"{latitude:.6f} {longitude:.6f}\n")
    return {"ok": True}

def install(app):
    app.include_router(router)
    app.mount("/ui", StaticFiles(directory=os.path.join(BASE, "ui"), html=True), name="ui")
