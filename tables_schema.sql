Enter password: **********
Welcome to the MySQL monitor.  Commands end with ; or \g.
Your MySQL connection id is 12
Server version: 26.7.0 MySQL Community Server - GPL

Copyright (c) 2000, 2026, Oracle and/or its affiliates.

Oracle is a registered trademark of Oracle Corporation and/or its
affiliates. Other names may be trademarks of their respective
owners.

Type 'help;' or '\h' for help. Type '\c' to clear the current input statement.

mysql> SHOW DATABASES;
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
8 rows in set (2.365 sec)

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
4 rows in set (3.281 sec)

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
5 rows in set (0.283 sec)

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
5 rows in set (0.012 sec)

mysql> SELECT * FROM POTHOLES;
+------------+----------+-----------+----------+----------------+---------------+------------------+--------------------------------------------------+----------------+-------------+
| POTHOLE_ID | LATITUDE | LONGITUDE | SEVERITY | FIRST_DETECTED | LAST_DETECTED | STATUS           | road_name                                        | data_source    | verified_on |
+------------+----------+-----------+----------+----------------+---------------+------------------+--------------------------------------------------+----------------+-------------+
|          1 |     NULL |      NULL | NULL     | NULL           | NULL          | Under Inspection | Subhash Road                                     | Times of India | NULL        |
|          2 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported         | MKP Chowk                                        | Times of India | NULL        |
|          3 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported         | Approach Road to Government Doon Medical College | Times of India | NULL        |
|          4 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported         | Canal Road                                       | Times of India | NULL        |
|          5 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported         | Dharampur                                        | Pioneer Edge   | NULL        |
|          6 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported         | GMS Road                                         | Pioneer Edge   | NULL        |
|          7 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported         | Ballupur                                         | Pioneer Edge   | NULL        |
|          8 |     NULL |      NULL | NULL     | NULL           | NULL          | Reported         | Premnagar                                        | Pioneer Edge   | NULL        |
+------------+----------+-----------+----------+----------------+---------------+------------------+--------------------------------------------------+----------------+-------------+
8 rows in set (0.013 sec)

mysql> SELECT * FROM ROAD_DIVERSION;
ERROR 1146 (42S02): Table 'smart_road_assistance.road_diversion' doesn't exist
mysql> SELECT * FROM ROAD_DIVERSIONS;
+-------------------+--------------+------------+------------+---------------------+---------------------+---------------------+---------+
| ROAD_DIVERSION_ID | ROAD_NAME    | LATITUDE   | LONGITUDE  | REASON              | START_TIME          | END_TIME            | STATUS  |
+-------------------+--------------+------------+------------+---------------------+---------------------+---------------------+---------+
|                 1 | Subhash Road | 30.3255000 | 78.0413000 | Road repair work    | 2026-10-10 09:00:00 | 2026-10-10 18:00:00 | PLANNED |
|                 2 | Canal Road   | 30.3300000 | 78.0400000 | Pothole repair work | 2026-10-11 10:00:00 | 2026-10-11 16:00:00 | PLANNED |
+-------------------+--------------+------------+------------+---------------------+---------------------+---------------------+---------+
2 rows in set (1.154 sec)