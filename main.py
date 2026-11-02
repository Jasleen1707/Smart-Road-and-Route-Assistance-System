
from fastapi import FastAPI,Query
import mysql.connector
from pydantic import BaseModel, Field
from fastapi import HTTPException
from math import radians, sin, cos, sqrt, atan2
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="Smart Driving Assistance System")
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://127.0.0.1:5500",
        "http://localhost:5500",
        "http://127.0.0.1:8000",
        "http://localhost:8000"
    ],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Connect to the existing MySQL database
def get_db_connection():
    return mysql.connector.connect(
        host="localhost",
        user="root",
        password="jasK2006##",
        database="smart_road_assistance"
    )
class PotholeDetection(BaseModel):
    latitude: float = Field(ge=-90, le=90)
    longitude: float = Field(ge=-180, le=180)
    road_name: str
    severity: str = "Medium"
    confidence: float = Field(default=0.90, ge=0, le=1)

class GPSLocation(BaseModel):
    latitude: float = Field(ge=-90, le=90)
    longitude: float = Field(ge=-180, le=180)


def calculate_distance(lat1, lon1, lat2, lon2):
    earth_radius = 6371000  # metres

    lat1, lon1, lat2, lon2 = map(
        radians, [lat1, lon1, lat2, lon2]
    )

    dlat = lat2 - lat1
    dlon = lon2 - lon1

    a = (
        sin(dlat / 2) ** 2
        + cos(lat1) * cos(lat2) * sin(dlon / 2) ** 2
    )

    return earth_radius * 2 * atan2(
        sqrt(a), sqrt(1 - a)
    )

@app.get("/")
def home():
    return {
        "message": "Smart Driving Assistance System API is running!"
    }


# Retrieve pothole records
@app.get("/potholes")
def get_potholes():
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        cursor.execute("""
            SELECT
                POTHOLE_ID,
                ROAD_NAME,
                SEVERITY,
                STATUS,
                DATA_SOURCE
            FROM POTHOLES
        """)

        potholes = cursor.fetchall()
        return {"potholes": potholes}

    finally:
        cursor.close()
        connection.close()
        
# Retrieve detection history
@app.get("/detections")
def get_detections():
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        cursor.execute("""
            SELECT
                DETECTION_ID,
                POTHOLE_ID,
                CONFIDENCE,
                DETECTED_AT
            FROM DETECTION
            ORDER BY DETECTED_AT DESC
        """)
        return {"detections": cursor.fetchall()}

    finally:
        cursor.close()
        connection.close()


# Retrieve nearby facility records
@app.get("/facilities")
def get_facilities():
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        cursor.execute("""
            SELECT
                FACILITY_ID,
                NAME,
                TYPE,
                LATITUDE,
                LONGITUDE,
                ADDRESS
            FROM FACILITIES
        """)
        return {"facilities": cursor.fetchall()}

    finally:
        cursor.close()
        connection.close()


# Retrieve road diversion records
@app.get("/diversions")
def get_diversions():
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        cursor.execute("""
            SELECT *
            FROM ROAD_DIVERSIONS
        """)
        return {"diversions": cursor.fetchall()}

    finally:
        cursor.close()
        connection.close()
@app.post("/detect-pothole")
def detect_pothole(data: PotholeDetection):
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        connection.start_transaction()

        # Find existing potholes with known GPS coordinates
        cursor.execute("""
            SELECT POTHOLE_ID, LATITUDE, LONGITUDE
            FROM POTHOLES
            WHERE LATITUDE IS NOT NULL
              AND LONGITUDE IS NOT NULL
            FOR UPDATE
        """)

        existing_potholes = cursor.fetchall()
        matched_pothole_id = None

        # Check whether this is a repeat detection
        for pothole in existing_potholes:
            distance = calculate_distance(
                data.latitude,
                data.longitude,
                float(pothole["LATITUDE"]),
                float(pothole["LONGITUDE"])
            )

            if distance <= 10:
                matched_pothole_id = pothole["POTHOLE_ID"]
                break

        if matched_pothole_id is not None:
            # Update the existing pothole
            cursor.execute("""
                UPDATE POTHOLES
                SET LAST_DETECTED = NOW()
                WHERE POTHOLE_ID = %s
            """, (matched_pothole_id,))

            action = "Existing pothole updated"

        else:
            # Create a new pothole record
            cursor.execute("""
                INSERT INTO POTHOLES (
                    LATITUDE,
                    LONGITUDE,
                    SEVERITY,
                    STATUS,
                    ROAD_NAME,
                    FIRST_DETECTED,
                    LAST_DETECTED
                )
                VALUES (%s, %s, %s, 'Reported', %s, NOW(), NOW())
            """, (
                data.latitude,
                data.longitude,
                data.severity,
                data.road_name
            ))

            matched_pothole_id = cursor.lastrowid
            action = "New pothole created"

        # Record every observation in detection history
        cursor.execute("""
            INSERT INTO DETECTION (
                POTHOLE_ID,
                CONFIDENCE,
                DETECTED_AT,
                LATITUDE,
                LONGITUDE
            )
            VALUES (%s, %s, NOW(), %s, %s)
        """, (
            matched_pothole_id,
            data.confidence,
            data.latitude,
            data.longitude
        ))

        connection.commit()

        return {
            "message": action,
            "pothole_id": matched_pothole_id,
            "detection_recorded": True
        }

    except Exception:
        connection.rollback()
        raise HTTPException(
            status_code=500,
            detail="Failed to process pothole detection"
        )

    finally:
        cursor.close()
        connection.close()

@app.get("/potholes/search")
def search_potholes(
    status: str = None,
    road_name: str = None
):
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        query = """
            SELECT
                POTHOLE_ID,
                ROAD_NAME,
                SEVERITY,
                STATUS,
                LATITUDE,
                LONGITUDE
            FROM POTHOLES
            WHERE 1 = 1
        """

        params = []

        if status:
            query += " AND STATUS = %s"
            params.append(status)

        if road_name:
            query += " AND ROAD_NAME LIKE %s"
            params.append("%" + road_name + "%")

        cursor.execute(query, params)

        return {"potholes": cursor.fetchall()}

    finally:
        cursor.close()
        connection.close()

@app.post("/save-location")
def save_location(data: GPSLocation):
    connection = get_db_connection()
    cursor = connection.cursor()

    try:
        cursor.execute(
            """
            INSERT INTO GPS_LOCATIONS (LATITUDE, LONGITUDE)
            VALUES (%s, %s)
            """,
            (data.latitude, data.longitude)
        )

        connection.commit()

        return {
            "message": "GPS location saved successfully",
            "latitude": data.latitude,
            "longitude": data.longitude
        }

    except Exception:
        connection.rollback()
        raise HTTPException(
            status_code=500,
            detail="Failed to save GPS location"
        )

    finally:
        cursor.close()
        connection.close()


@app.get("/nearby-potholes")
def get_nearby_potholes(
    latitude: float = Query(..., ge=-90, le=90),
    longitude: float = Query(..., ge=-180, le=180),
    radius_m: float = Query(1000, gt=0)
):
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        cursor.execute("""
            SELECT POTHOLE_ID, ROAD_NAME, SEVERITY, STATUS,
                   LATITUDE, LONGITUDE
            FROM POTHOLES
            WHERE LATITUDE IS NOT NULL
              AND LONGITUDE IS NOT NULL
        """)

        potholes = cursor.fetchall()
        nearby = []

        for p in potholes:
            distance = calculate_distance(
                latitude,
                longitude,
                float(p["LATITUDE"]),
                float(p["LONGITUDE"])
            )

            if distance <= radius_m:
                p["LATITUDE"] = float(p["LATITUDE"])
                p["LONGITUDE"] = float(p["LONGITUDE"])
                p["distance_m"] = round(distance, 2)
                nearby.append(p)

        nearby.sort(key=lambda p: p["distance_m"])

        return {
            "count": len(nearby),
            "radius_m": radius_m,
            "potholes": nearby
        }

    except Exception as e:
        raise HTTPException(
            status_code=500,
            detail="Failed to retrieve nearby potholes"
        )
    finally:
        cursor.close()
        connection.close()

@app.get("/nearby-facilities")
def get_nearby_facilities(
    latitude: float = Query(..., ge=-90, le=90),
    longitude: float = Query(..., ge=-180, le=180),
    radius_m: float = Query(10000, gt=0)
):
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        cursor.execute("""
            SELECT FACILITY_ID, NAME, TYPE, LATITUDE, LONGITUDE, ADDRESS
            FROM FACILITIES
            WHERE LATITUDE IS NOT NULL
              AND LONGITUDE IS NOT NULL
        """)

        facilities = cursor.fetchall()
        nearby = []

        for f in facilities:
            distance = calculate_distance(
                latitude,
                longitude,
                float(f["LATITUDE"]),
                float(f["LONGITUDE"])
            )

            if distance <= radius_m:
                f["LATITUDE"] = float(f["LATITUDE"])
                f["LONGITUDE"] = float(f["LONGITUDE"])
                f["distance_m"] = round(distance, 2)
                nearby.append(f)

        nearby.sort(key=lambda f: f["distance_m"])

        return {
            "count": len(nearby),
            "radius_m": radius_m,
            "facilities": nearby
        }

    except Exception:
        raise HTTPException(
            status_code=500,
            detail="Failed to retrieve nearby facilities"
        )
    finally:
        cursor.close()
        connection.close()

@app.get("/current-location")
def get_current_location():
    connection = get_db_connection()
    cursor = connection.cursor(dictionary=True)

    try:
        cursor.execute("""
            SELECT LOCATION_ID, LATITUDE, LONGITUDE, RECORDED_AT
            FROM GPS_LOCATIONS
            ORDER BY RECORDED_AT DESC, LOCATION_ID DESC
            LIMIT 1
        """)

        location = cursor.fetchone()

        if location is None:
            raise HTTPException(
                status_code=404,
                detail="No GPS location has been saved yet"
            )

        location["LATITUDE"] = float(location["LATITUDE"])
        location["LONGITUDE"] = float(location["LONGITUDE"])

        return location

    except HTTPException:
        raise
    except Exception:
        raise HTTPException(
            status_code=500,
            detail="Failed to retrieve current GPS location"
        )
    finally:
        cursor.close()
        connection.close()
