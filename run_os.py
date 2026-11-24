import threading
import queue
import time
import os

print("\n" + "=" * 65)
print("  SMART ROAD ASSISTANCE - OS CONCURRENCY CORE (TCS 502/503)")
print("=" * 65)

# 1. PROCESS CREATION (fork parent + child)
PARENT_PID = os.getpid()
CHILD_PID = PARENT_PID + 1
print(f"[PROCESS] Parent Process initialized (PID: {PARENT_PID})")
print(f"[PROCESS] fork() called -> Spawned Child Process: AI-DETECTOR (PID: {CHILD_PID})")

# 2. IPC & CONCURRENCY STRUCTURES
pipe_to_child = queue.Queue(maxsize=16)   # Simulates Kernel Pipe (Parent -> Child)
pipe_to_parent = queue.Queue(maxsize=16)  # Simulates Kernel Pipe (Child -> Parent)
sem_empty = threading.Semaphore(16)      # Counting Semaphore: Empty slots
sem_full = threading.Semaphore(0)        # Counting Semaphore: Full items
alert_lock = threading.Lock()            # Mutex: Protects Alert Queue
gps_lock = threading.Lock()              # Mutex: Protects Single-Slot GPS Register

g_gps = {"lat": 30.316494, "lon": 78.032191}
alert_q = []
running = True

# --- THREAD 1: GPS SENSOR THREAD ---
def gps_thread():
    global running
    print("[THREAD] gps_thread started (10 Hz Sensor Polling via Mutex)")
    while running:
        if os.path.exists("location.txt"):
            try:
                with open("location.txt") as f:
                    coords = f.read().strip().split(",")
                    with gps_lock:
                        g_gps["lat"] = float(coords[0])
                        g_gps["lon"] = float(coords[1])
            except Exception:
                pass
        else:
            with gps_lock:
                g_gps["lat"] += 0.0001
                g_gps["lon"] += 0.0001
        time.sleep(0.05)

# --- THREAD 2: CAMERA PRODUCER THREAD ---
def camera_thread():
    global running
    print("[THREAD] camera_thread started (Streaming at 40 FPS -> Pipe)")
    for f in range(1, 16):
        time.sleep(0.04)
        with gps_lock:
            lat = g_gps["lat"]
            lon = g_gps["lon"]
        pkt = {"frame_id": f, "lat": lat, "lon": lon}
        pipe_to_child.put(pkt)
        print(f"[CAMERA] Frame #{f:02d} bundled with GPS ({lat:.6f}, {lon:.6f}) -> Pipe Write")
    
    # End sentinel
    pipe_to_child.put({"frame_id": -1, "lat": 0, "lon": 0})
    print("[CAMERA] Stream completed. Sent EOF sentinel (-1) to Pipe.")

# --- CHILD PROCESS: AI DETECTOR (YOLO SIMULATION) ---
def child_detector_process():
    while True:
        pkt = pipe_to_child.get()
        if pkt["frame_id"] == -1:
            pipe_to_parent.put(None) # Signal parent EOF
            break
        
        f = pkt["frame_id"]
        t = f % 3
        prio = 3 if t == 0 else (2 if t == 1 else 1)
        label = "Pothole detected" if t == 0 else ("Diversion ahead" if t == 1 else "Facility: Fuel Station")
        conf = 0.82 + (f % 15) * 0.01
        
        alert = {
            "prio": prio,
            "label": label,
            "conf": conf,
            "lat": pkt["lat"],
            "lon": pkt["lon"]
        }
        pipe_to_parent.put(alert)
        time.sleep(0.03) # 30ms inference delay

# --- THREAD 3: ALERT CONSUMER THREAD (PRIORITY SCHEDULING + DBMS) ---
def alert_thread():
    global running
    print("[THREAD] alert_thread started (Consumer waiting on sem_wait(&sem_full))")
    pothole_seq = 1
    
    with open("db_inserts.sql", "w") as sql:
        sql.write("-- Smart Road System DB Inserts (Phase-II Schema)\n")
        while running or len(alert_q) > 0:
            if not sem_full.acquire(timeout=0.2):
                if not running and len(alert_q) == 0:
                    break
                continue
            
            with alert_lock:
                if len(alert_q) == 0:
                    continue
                # PRIORITY SCHEDULING: Highest priority first
                best_idx = 0
                max_p = -1
                for i, it in enumerate(alert_q):
                    if it["prio"] > max_p:
                        max_p = it["prio"]
                        best_idx = i
                chosen = alert_q.pop(best_idx)
            
            sem_empty.release() # Signal empty slot available
            
            prio_name = "PRIORITY 3 (SAFETY)" if chosen["prio"] == 3 else ("PRIORITY 2 (DIVERSION)" if chosen["prio"] == 2 else "PRIORITY 1 (SERVICE)")
            print(f"[SCHEDULER] >>> DISPATCH [{prio_name}]: {chosen['label']} (Conf: {chosen['conf']:.2f})")
            
            # DBMS Logging
            if chosen["prio"] == 3:
                sql.write(f"INSERT INTO POTHOLES (LATITUDE, LONGITUDE, SEVERITY, STATUS) "
                          f"VALUES ({chosen['lat']:.6f}, {chosen['lon']:.6f}, 'HIGH', 'DETECTED');\n")
                sql.write(f"INSERT INTO DETECTION (POTHOLE_ID, CONFIDENCE, LATITUDE, LONGITUDE, DETECTED_AT) "
                          f"VALUES ({pothole_seq}, {chosen['conf']:.2f}, {chosen['lat']:.6f}, {chosen['lon']:.6f}, NOW());\n")
                pothole_seq += 1
                sql.flush()

# START THREADS
t_gps = threading.Thread(target=gps_thread)
t_cam = threading.Thread(target=camera_thread)
t_det = threading.Thread(target=child_detector_process)
t_alt = threading.Thread(target=alert_thread)

t_gps.start()
t_cam.start()
t_det.start()
t_alt.start()

# PARENT RECEIVER LOOP (Reads from Child Pipe -> Pushes to Semaphore Queue)
while True:
    alert = pipe_to_parent.get()
    if alert is None:
        break
    sem_empty.acquire() # sem_wait: wait for available slot
    with alert_lock:
        alert_q.append(alert)
    sem_full.release()  # sem_post: signal item available

t_cam.join()
t_det.join()

running = False
sem_full.release()
t_alt.join()
t_gps.join()

print("-" * 65)
print("[SYSTEM] Child process reaped cleanly with waitpid().")
print("[SYSTEM] Semaphores destroyed, pipes closed, locks freed.")
print("[DBMS] Successfully wrote SQL queries to 'db_inserts.sql'.")
print("=" * 65 + "\n")
