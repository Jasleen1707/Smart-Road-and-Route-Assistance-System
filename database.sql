Enter password: **********
Welcome to the MySQL monitor.  Commands end with ; or \g.
Your MySQL connection id is 11
Server version: 26.7.0 MySQL Community Server - GPL

Copyright (c) 2000, 2026, Oracle and/or its affiliates.

Oracle is a registered trademark of Oracle Corporation and/or its
affiliates. Other names may be trademarks of their respective
owners.

Type 'help;' or '\h' for help. Type '\c' to clear the current input statement.

mysql> show databases;
+-----------------------+
| Database              |
+-----------------------+
| exam                  |
| geu_exam_s1           |
| hotel_management      |
| information_schema    |
| mysql                 |
| performance_schema    |
| smart_road_assistance |
| sys                   |
+-----------------------+
8 rows in set (10.012 sec)

mysql> use smar_road_assistance;
ERROR 1049 (42000): Unknown database 'smar_road_assistance'
mysql> USE SMART_ROAD_ASSISTANCE;
Database changed
mysql> SHOW TABLES;
+---------------------------------+
| Tables_in_smart_road_assistance |
+---------------------------------+
| detection                       |
| facilities                      |
| potholes                        |
| road_diversions                 |
+---------------------------------+
4 rows in set (3.247 sec)

mysql> SELECT * FROM DETECTUON;
ERROR 1146 (42S02): Table 'smart_road_assistance.detectuon' doesn't exist
mysql> SELECT * FROM DETECTION;
+--------------+------------+------------+---------------------+----------+-----------+
| DETECTION_ID | POTHOLE_ID | CONFIDENCE | DETECTED_AT         | LATITUDE | LONGITUDE |
+--------------+------------+------------+---------------------+----------+-----------+
|            1 |          1 |     0.9500 | 2026-10-09 10:00:00 |     NULL |      NULL |
|            2 |          2 |     0.8700 | 2026-10-09 10:05:00 |     NULL |      NULL |
|            3 |          3 |     0.9200 | 2026-10-09 10:10:00 |     NULL |      NULL |
|            4 |          4 |     0.8900 | 2026-10-09 10:15:00 |     NULL |      NULL |
|            5 |          5 |     0.9600 | 2026-10-09 10:20:00 |     NULL |      NULL |
+--------------+------------+------------+---------------------+----------+-----------+
5 rows in set (1.855 sec)

mysql> SELECT * FROM FACILITIES;
+-------------+-----------------+---------------+------------+------------+----------+
| FACILITY_ID | NAME            | TYPE          | LATITUDE   | LONGITUDE  | ADDRESS  |
+-------------+-----------------+---------------+------------+------------+----------+
|           1 | City Hospital   | Hospital      | 30.3255000 | 78.0413000 | Dehradun |
|           2 | Fuel Station    | Petrol Pump   | 30.3165000 | 78.0322000 | Dehradun |
|           3 | Quick Puncture  | Puncture Shop | 30.3200000 | 78.0500000 | Dehradun |
|           4 | City Restaurant | Restaurant    | 30.3280000 | 78.0450000 | Dehradun |
|           5 | Trauma Centre   | Hospital      | 30.3300000 | 78.0400000 | Dehradun |
+-------------+-----------------+---------------+------------+------------+----------+
5 rows in set (0.012 sec)

mysql> SELECT * FROM POTHOLES;
+------------+----------+-----------+----------+----------------+---------------+----------+--------------------------------------------------+----------------+-------------+
| POTHOLE_ID | LATITUDE | LONGITUDE | SEVERITY | FIRST_DETECTED | LAST_DETECTED | STATUS   | road_name                                        | data_source    | verified_on |
+------------+----------+-----------+----------+----------------+---------------+----------+--------------------------------------------------+----------------+-------------+
|          1 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | Subhash Road                                     | Times of India | NULL        |
|          2 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | MKP Chowk                                        | Times of India | NULL        |
|          3 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | Approach Road to Government Doon Medical College | Times of India | NULL        |
|          4 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | Canal Road                                       | Times of India | NULL        |
|          5 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | Dharampur                                        | Pioneer Edge   | NULL        |
|          6 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | GMS Road                                         | Pioneer Edge   | NULL        |
|          7 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | Ballupur                                         | Pioneer Edge   | NULL        |
|          8 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported | Premnagar                                        | Pioneer Edge   | NULL        |
+------------+----------+-----------+----------+----------------+---------------+----------+--------------------------------------------------+----------------+-------------+
8 rows in set (0.862 sec)

mysql> SELECT * FROM ROAD_DIVERSIONS;
+-------------------+--------------+------------+------------+-----------------------+---------------------+---------------------+---------+
| ROAD_DIVERSION_ID | ROAD_NAME    | LATITUDE   | LONGITUDE  | REASON                | START_TIME          | END_TIME            | STATUS  |
+-------------------+--------------+------------+------------+-----------------------+---------------------+---------------------+---------+
|                 1 | Subhash Road | 30.3255000 | 78.0413000 | Road repair work      | 2026-10-10 09:00:00 | 2026-10-10 18:00:00 | PLANNED |
|                 2 | Canal Road   | 30.3300000 | 78.0400000 | Pothole repair work   | 2026-10-11 10:00:00 | 2026-10-11 16:00:00 | PLANNED |
|                 3 | GMS Road     | 30.3150000 | 78.0200000 | Road maintenance work | 2026-10-12 08:00:00 | 2026-10-12 17:00:00 | PLANNED |
+-------------------+--------------+------------+------------+-----------------------+---------------------+---------------------+---------+
3 rows in set (0.011 sec)

mysql>
mysql> USE smart_road_assistance;
Database changed
mysql>
mysql> SELECT
    ->     P.POTHOLE_ID,
    ->     P.ROAD_NAME,
    ->     P.STATUS,
    ->     D.DETECTION_ID,
    ->     D.CONFIDENCE,
    ->     D.DETECTED_AT
    -> FROM POTHOLES P
    -> INNER JOIN DETECTION D
    ->     ON P.POTHOLE_ID = D.POTHOLE_ID;
+------------+--------------------------------------------------+----------+--------------+------------+---------------------+
| POTHOLE_ID | ROAD_NAME                                        | STATUS   | DETECTION_ID | CONFIDENCE | DETECTED_AT         |
+------------+--------------------------------------------------+----------+--------------+------------+---------------------+
|          1 | Subhash Road                                     | Reported |            1 |     0.9500 | 2026-10-09 10:00:00 |
|          2 | MKP Chowk                                        | Reported |            2 |     0.8700 | 2026-10-09 10:05:00 |
|          3 | Approach Road to Government Doon Medical College | Reported |            3 |     0.9200 | 2026-10-09 10:10:00 |
|          4 | Canal Road                                       | Reported |            4 |     0.8900 | 2026-10-09 10:15:00 |
|          5 | Dharampur                                        | Reported |            5 |     0.9600 | 2026-10-09 10:20:00 |
+------------+--------------------------------------------------+----------+--------------+------------+---------------------+
5 rows in set (0.011 sec)

mysql>
mysql> SELECT
    ->     P.POTHOLE_ID,
    ->     P.ROAD_NAME,
    ->     P.STATUS,
    ->     D.DETECTION_ID,
    ->     D.CONFIDENCE
    -> FROM POTHOLES P
    -> LEFT JOIN DETECTION D
    ->     ON P.POTHOLE_ID = D.POTHOLE_ID;
+------------+--------------------------------------------------+----------+--------------+------------+
| POTHOLE_ID | ROAD_NAME                                        | STATUS   | DETECTION_ID | CONFIDENCE |
+------------+--------------------------------------------------+----------+--------------+------------+
|          1 | Subhash Road                                     | Reported |            1 |     0.9500 |
|          2 | MKP Chowk                                        | Reported |            2 |     0.8700 |
|          3 | Approach Road to Government Doon Medical College | Reported |            3 |     0.9200 |
|          4 | Canal Road                                       | Reported |            4 |     0.8900 |
|          5 | Dharampur                                        | Reported |            5 |     0.9600 |
|          6 | GMS Road                                         | Reported |         NULL |       NULL |
|          7 | Ballupur                                         | Reported |         NULL |       NULL |
|          8 | Premnagar                                        | Reported |         NULL |       NULL |
+------------+--------------------------------------------------+----------+--------------+------------+
8 rows in set (0.364 sec)

mysql> SELECT STATUS,COUNT(*) AS TOTAL_POTHOLES FROM POTHOLES GROUP BY STATUS;
+----------+----------------+
| STATUS   | TOTAL_POTHOLES |
+----------+----------------+
| Reported |              8 |
+----------+----------------+
1 row in set (17.545 sec)

mysql>
mysql> SELECT
    ->     ROUND(AVG(CONFIDENCE), 4) AS AVG_CONFIDENCE,
    ->     MAX(CONFIDENCE) AS MAX_CONFIDENCE,
    ->     MIN(CONFIDENCE) AS MIN_CONFIDENCE
    -> FROM DETECTION;
+----------------+----------------+----------------+
| AVG_CONFIDENCE | MAX_CONFIDENCE | MIN_CONFIDENCE |
+----------------+----------------+----------------+
|         0.9180 |         0.9600 |         0.8700 |
+----------------+----------------+----------------+
1 row in set (3.038 sec)

mysql> SELECT TYPE, COUNT(*) AS TOTAL_FACILITIES FROM FACILITIES GROUP BY TYPE ORDER BY TOTAL_FACILITIES DESC;
+---------------+------------------+
| TYPE          | TOTAL_FACILITIES |
+---------------+------------------+
| Hospital      |                2 |
| Petrol Pump   |                1 |
| Puncture Shop |                1 |
| Restaurant    |                1 |
+---------------+------------------+
4 rows in set (0.013 sec)

mysql>
mysql> UPDATE POTHOLES
    -> SET STATUS = 'Under Inspection'
    -> WHERE POTHOLE_ID = 1;
Query OK, 1 row affected (8.082 sec)
Rows matched: 1  Changed: 1  Warnings: 0

mysql>
mysql>
mysql> SELECT POTHOLE_ID, ROAD_NAME, STATUS
    -> FROM POTHOLES
    -> WHERE POTHOLE_ID = 1;
+------------+--------------+------------------+
| POTHOLE_ID | ROAD_NAME    | STATUS           |
+------------+--------------+------------------+
|          1 | Subhash Road | Under Inspection |
+------------+--------------+------------------+
1 row in set (0.006 sec)

mysql> UPDATE FACILITIES SET ADDRESS='TYAGI ROAD' WHERE FACILITY_ID=1;
Query OK, 1 row affected (19.500 sec)
Rows matched: 1  Changed: 1  Warnings: 0

mysql> SELCT * FROM FACILITIES WHERE FACILITY_ID=1;
ERROR 1064 (42000): You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'SELCT * FROM FACILITIES WHERE FACILITY_ID=1' at line 1
mysql> SELECT * FROM FACILITIES WHERE FACILITY_ID=1;
+-------------+---------------+----------+------------+------------+------------+
| FACILITY_ID | NAME          | TYPE     | LATITUDE   | LONGITUDE  | ADDRESS    |
+-------------+---------------+----------+------------+------------+------------+
|           1 | City Hospital | Hospital | 30.3255000 | 78.0413000 | TYAGI ROAD |
+-------------+---------------+----------+------------+------------+------------+
1 row in set (0.006 sec)

mysql> SELECT * FROM ROAD_DIVERSIONS WHERE ROAD_DIVERSION_ID=3;
+-------------------+-----------+------------+------------+-----------------------+---------------------+---------------------+---------+
| ROAD_DIVERSION_ID | ROAD_NAME | LATITUDE   | LONGITUDE  | REASON                | START_TIME          | END_TIME            | STATUS  |
+-------------------+-----------+------------+------------+-----------------------+---------------------+---------------------+---------+
|                 3 | GMS Road  | 30.3150000 | 78.0200000 | Road maintenance work | 2026-10-12 08:00:00 | 2026-10-12 17:00:00 | PLANNED |
+-------------------+-----------+------------+------------+-----------------------+---------------------+---------------------+---------+
1 row in set (0.303 sec)

mysql> DELETE FROM ROAD_DIVERSIONS WHERE ROAD_DIVERSION_ID=3;
Query OK, 1 row affected (2.924 sec)

mysql> SELECT * FROM ROAD_DIVERSIONS;
+-------------------+--------------+------------+------------+---------------------+---------------------+---------------------+---------+
| ROAD_DIVERSION_ID | ROAD_NAME    | LATITUDE   | LONGITUDE  | REASON              | START_TIME          | END_TIME            | STATUS  |
+-------------------+--------------+------------+------------+---------------------+---------------------+---------------------+---------+
|                 1 | Subhash Road | 30.3255000 | 78.0413000 | Road repair work    | 2026-10-10 09:00:00 | 2026-10-10 18:00:00 | PLANNED |
|                 2 | Canal Road   | 30.3300000 | 78.0400000 | Pothole repair work | 2026-10-11 10:00:00 | 2026-10-11 16:00:00 | PLANNED |
+-------------------+--------------+------------+------------+---------------------+---------------------+---------------------+---------+
2 rows in set (0.008 sec)

mysql>mysql> SELECT
    ->     POTHOLE_ID,
    ->     COUNT(*) AS TOTAL_DETECTIONS,
    ->     MIN(DETECTED_AT) AS FIRST_DETECTION,
    ->     MAX(DETECTED_AT) AS LAST_DETECTION,
    ->     AVG(CONFIDENCE) AS AVERAGE_CONFIDENCE
    -> FROM DETECTION
    -> GROUP BY POTHOLE_ID
    -> ORDER BY TOTAL_DETECTIONS DESC;
+------------+------------------+---------------------+---------------------+--------------------+
| POTHOLE_ID | TOTAL_DETECTIONS | FIRST_DETECTION     | LAST_DETECTION      | AVERAGE_CONFIDENCE |
+------------+------------------+---------------------+---------------------+--------------------+
|          1 |                1 | 2026-10-09 10:00:00 | 2026-10-09 10:00:00 |         0.95000000 |
|          2 |                1 | 2026-10-09 10:05:00 | 2026-10-09 10:05:00 |         0.87000000 |
|          3 |                1 | 2026-10-09 10:10:00 | 2026-10-09 10:10:00 |         0.92000000 |
|          4 |                1 | 2026-10-09 10:15:00 | 2026-10-09 10:15:00 |         0.89000000 |
|          5 |                1 | 2026-10-09 10:20:00 | 2026-10-09 10:20:00 |         0.96000000 |
+------------+------------------+---------------------+---------------------+--------------------+
5 rows in set (10.656 sec)

mysql>
mysql>
mysql> SELECT
    ->     POTHOLE_ID,
    ->     COUNT(*) AS TOTAL_DETECTIONS
    -> FROM DETECTION
    -> GROUP BY POTHOLE_ID
    -> HAVING COUNT(*) > 1;
Empty set (0.014 sec)
mysql> SELECT
    ->     p.POTHOLE_ID,
    ->     p.ROAD_NAME,
    ->     COUNT(d.DETECTION_ID) AS TOTAL_DETECTIONS
    -> FROM POTHOLES p
    -> LEFT JOIN DETECTION d
    ->     ON p.POTHOLE_ID = d.POTHOLE_ID
    -> GROUP BY p.POTHOLE_ID, p.ROAD_NAME
    -> ORDER BY TOTAL_DETECTIONS DESC;
+------------+--------------------------------------------------+------------------+
| POTHOLE_ID | ROAD_NAME                                        | TOTAL_DETECTIONS |
+------------+--------------------------------------------------+------------------+
|          1 | Subhash Road                                     |                1 |
|          2 | MKP Chowk                                        |                1 |
|          3 | Approach Road to Government Doon Medical College |                1 |
|          4 | Canal Road                                       |                1 |
|          5 | Dharampur                                        |                1 |
|          6 | GMS Road                                         |                0 |
|          7 | Ballupur                                         |                0 |
|          8 | Premnagar                                        |                0 |
+------------+--------------------------------------------------+------------------+
8 rows in set (0.438 sec)

mysql>
mysql> SHOW CREATE TABLE DETECTION;
+-----------+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Table     | Create Table                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
+-----------+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| DETECTION | CREATE TABLE `detection` (
  `DETECTION_ID` int NOT NULL AUTO_INCREMENT,
  `POTHOLE_ID` int NOT NULL,
  `CONFIDENCE` decimal(5,4) DEFAULT NULL,
  `DETECTED_AT` datetime DEFAULT CURRENT_TIMESTAMP,
  `LATITUDE` decimal(10,7) DEFAULT NULL,
  `LONGITUDE` decimal(10,7) DEFAULT NULL,
  PRIMARY KEY (`DETECTION_ID`),
  KEY `POTHOLE_ID` (`POTHOLE_ID`),
  CONSTRAINT `detection_ibfk_1` FOREIGN KEY (`POTHOLE_ID`) REFERENCES `potholes` (`POTHOLE_ID`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci |
+-----------+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
1 row in set (2.291 sec)
mysql> SHOW CREATE TABLE FACILITIES;
+------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Table      | Create Table                                                                                                                                                                                                                                                                                                                                                                  |
+------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| FACILITIES | CREATE TABLE `facilities` (
  `FACILITY_ID` int NOT NULL AUTO_INCREMENT,
  `NAME` varchar(20) NOT NULL,
  `TYPE` varchar(20) NOT NULL,
  `LATITUDE` decimal(10,7) DEFAULT NULL,
  `LONGITUDE` decimal(10,7) DEFAULT NULL,
  `ADDRESS` varchar(30) NOT NULL,
  PRIMARY KEY (`FACILITY_ID`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci |
+------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
1 row in set (0.016 sec)

mysql> SHOW CREATE TABLE ROAD_DIVERSIONS;
+-----------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Table           | Create Table                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
+-----------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| ROAD_DIVERSIONS | CREATE TABLE `road_diversions` (
  `ROAD_DIVERSION_ID` int NOT NULL AUTO_INCREMENT,
  `ROAD_NAME` varchar(20) NOT NULL,
  `LATITUDE` decimal(10,7) DEFAULT NULL,
  `LONGITUDE` decimal(10,7) DEFAULT NULL,
  `REASON` varchar(300) DEFAULT NULL,
  `START_TIME` datetime DEFAULT NULL,
  `END_TIME` datetime DEFAULT NULL,
  `STATUS` varchar(20) DEFAULT 'ACTIVE',
  PRIMARY KEY (`ROAD_DIVERSION_ID`)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci |
+-----------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
1 row in set (0.010 sec)

