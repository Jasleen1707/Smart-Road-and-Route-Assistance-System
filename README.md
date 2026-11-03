#  Smart Driving Assistance System

##  Project Overview

The Smart Driving Assistance System is a road-safety project designed to help drivers identify potholes, access road-hazard information, find nearby facilities, and receive information about road diversions.

The project aims to integrate AI-based pothole detection, GPS-based location information, a MySQL database, and a FastAPI backend to provide useful road information to drivers.

##  Objectives

- Detect and record pothole information.
- Store pothole reports and detection history in a structured database.
- Provide information about nearby facilities such as hospitals, petrol pumps, puncture shops, and restaurants.
- Maintain records of road diversions and maintenance activities.
- Use GPS coordinates to support future location-based searches.
- Provide a backend API for retrieving information from the database.
- Improve road awareness and support safer driving.

##  Key Features

### 1. Pothole Information Management
- Stores pothole reports and road names.
- Maintains report status and source information.
- Supports storing severity and GPS coordinates when available.

### 2. Pothole Detection History
- Stores individual detection records.
- Maintains detection timestamps and confidence scores.
- Links detection records to pothole records using a foreign key.

### 3. Nearby Facilities
Stores information about facilities that may be useful to drivers, including:
- Hospitals
- Petrol pumps
- Puncture shops
- Restaurants

GPS coordinates can support nearby-facility searches once verified location data is available.

### 4. Road Diversion Management
- Stores road names affected by diversions.
- Records reasons for road maintenance or diversion.
- Maintains planned start times, end times, and statuses.

### 5. REST API
The FastAPI backend provides endpoints to retrieve records from the MySQL database.

##  Technology Stack

- **Database:** MySQL
- **Backend Framework:** FastAPI
- **ASGI Server:** Uvicorn
- **Database Connector:** MySQL Connector/Python
- **Programming Language:** Python
- **AI and Computer Vision:** OpenCV and YOLO, depending on the integrated detection module
- **API Testing:** FastAPI Swagger UI
- **Development Environment:** Visual Studio Code

##  System Architecture

The intended system architecture is:

1. A camera and AI module identify potential potholes.
2. GPS provides location coordinates when available.
3. The FastAPI backend communicates with the MySQL database.
4. MySQL stores pothole reports, detection history, facility information, and road-diversion records.
5. API endpoints provide stored information to a frontend or dashboard.

```text
Camera / AI Detection Module
             |
             v
       GPS Coordinates
             |
             v
       FastAPI Backend
             |
             v
       MySQL Database
             |
     ---------------------
     |         |         |
  Potholes  Facilities  Road Diversions
     |
     v
Detection History
