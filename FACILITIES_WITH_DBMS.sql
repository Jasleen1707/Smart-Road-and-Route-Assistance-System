mysql> SELECT *
    -> FROM GPS_LOCATIONS
    -> ORDER BY LOCATION_ID DESC
    -> LIMIT 1;
+-------------+------------+------------+---------------------+
| LOCATION_ID | LATITUDE   | LONGITUDE  | RECORDED_AT         |
+-------------+------------+------------+---------------------+
|          36 | 30.3046227 | 78.0357919 | 2026-10-26 23:56:28 |
+-------------+------------+------------+---------------------+
1 row in set (0.367 sec)

mysql> USE smart_road_assistance;
Database changed
mysql>
mysql> SELECT
    ->     FACILITY_ID,
    ->     NAME,
    ->     TYPE,
    ->     LATITUDE,
    ->     LONGITUDE,
    ->     ADDRESS
    -> FROM FACILITIES;
+-------------+-----------------+---------------+------------+------------+------------+
| FACILITY_ID | NAME            | TYPE          | LATITUDE   | LONGITUDE  | ADDRESS    |
+-------------+-----------------+---------------+------------+------------+------------+
|           1 | City Hospital   | Hospital      | 30.3255000 | 78.0413000 | TYAGI ROAD |
|           2 | Fuel Station    | Petrol Pump   | 30.3165000 | 78.0322000 | Dehradun   |
|           3 | Quick Puncture  | Puncture Shop | 30.3200000 | 78.0500000 | Dehradun   |
|           4 | City Restaurant | Restaurant    | 30.3280000 | 78.0450000 | Dehradun   |
|           5 | Trauma Centre   | Hospital      | 30.3300000 | 78.0400000 | Dehradun   |
+-------------+-----------------+---------------+------------+------------+------------+
5 rows in set (0.486 sec)

mysql> SELECT * FROM FACILITIES;
+-------------+-----------------+---------------+------------+------------+------------+
| FACILITY_ID | NAME            | TYPE          | LATITUDE   | LONGITUDE  | ADDRESS    |
+-------------+-----------------+---------------+------------+------------+------------+
|           1 | City Hospital   | Hospital      | 30.3255000 | 78.0413000 | TYAGI ROAD |
|           2 | Fuel Station    | Petrol Pump   | 30.3165000 | 78.0322000 | Dehradun   |
|           3 | Quick Puncture  | Puncture Shop | 30.3200000 | 78.0500000 | Dehradun   |
|           4 | City Restaurant | Restaurant    | 30.3280000 | 78.0450000 | Dehradun   |
|           5 | Trauma Centre   | Hospital      | 30.3300000 | 78.0400000 | Dehradun   |
+-------------+-----------------+---------------+------------+------------+------------+
5 rows in set (0.331 sec)

mysql> UPDATE FACILITIES
    -> SET NAME = 'Test Hospital DBMS'
    -> WHERE FACILITY_ID = 1;
Query OK, 1 row affected (2.251 sec)
Rows matched: 1  Changed: 1  Warnings: 0

mysql> SELECT * FROM FACILTIES;
ERROR 1146 (42S02): Table 'smart_road_assistance.facilties' doesn't exist
mysql> SELECT * FROM FACILITIES;
+-------------+--------------------+---------------+------------+------------+------------+
| FACILITY_ID | NAME               | TYPE          | LATITUDE   | LONGITUDE  | ADDRESS    |
+-------------+--------------------+---------------+------------+------------+------------+
|           1 | Test Hospital DBMS | Hospital      | 30.3255000 | 78.0413000 | TYAGI ROAD |
|           2 | Fuel Station       | Petrol Pump   | 30.3165000 | 78.0322000 | Dehradun   |
|           3 | Quick Puncture     | Puncture Shop | 30.3200000 | 78.0500000 | Dehradun   |
|           4 | City Restaurant    | Restaurant    | 30.3280000 | 78.0450000 | Dehradun   |
|           5 | Trauma Centre      | Hospital      | 30.3300000 | 78.0400000 | Dehradun   |
+-------------+--------------------+---------------+------------+------------+------------+
5 rows in set (0.012 sec)

//OUTPUT IN THE GPS MODULE


====================================
 SMART ROAD ASSISTANCE GPS MODULE
====================================
1. Refresh GPS location
2. Display current location
3. Find nearby potholes
4. Find nearby services
5. Calculate distance to destination
0. Exit
Enter choice: 4
Enter search radius in km: 10

--- NEARBY FACILITIES FROM MYSQL ---

Facility ID: 2
Name: Fuel Station
Type: Petrol Pump
Address: Dehradun
Distance: 5007.15 metres (5.01 km)

Facility ID: 3
Name: Quick Puncture
Type: Puncture Shop
Address: Dehradun
Distance: 6267.06 metres (6.27 km)

Facility ID: 1
Name: Test Hospital DBMS    //changes reflect here (hence integration done
Type: Hospital
Address: TYAGI ROAD
Distance: 6301.62 metres (6.30 km)

Facility ID: 5
Name: Trauma Centre
Type: Hospital
Address: Dehradun
Distance: 6684.61 metres (6.68 km)

Facility ID: 4
Name: City Restaurant
Type: Restaurant
Address: Dehradun
Distance: 6720.90 metres (6.72 km)
