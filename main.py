
from fastapi import FastAPI
import mysql.connector

app = FastAPI(title="Smart Driving Assistance System")


# Connect to the existing MySQL database
def get_db_connection():
    return mysql.connector.connect(
        host="localhost",
        user="root",
        password="jasK2006##",
        database="smart_road_assistance"
    )


# Test whether the API is running
@app.get("/")
def home():
    return {
        "message": "Smart Driving Assistance System API is running!"
    }


# Retrieve pothole records from MySQL
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

