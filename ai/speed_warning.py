
def speed_warning(speed_kmh, distance_m):

    # Convert km/h into metres per second
    speed_ms = speed_kmh / 3.6

    # Calculate estimated time to reach pothole
    time_to_reach = distance_m / speed_ms

    print("\n----- SMART ROAD WARNING -----")
    print(f"Vehicle Speed: {speed_kmh} km/h")
    print(f"Pothole Distance: {distance_m} metres")
    print(f"Estimated Time: {time_to_reach:.2f} seconds")

    if time_to_reach <= 1.5:
        print("WARNING: IMMEDIATE ATTENTION REQUIRED!")

    elif time_to_reach <= 3:
        print("CAUTION: Pothole approaching!")

    else:
        print("NOTICE: Pothole detected ahead.")

    print("------------------------------")


# Simulated vehicle speeds
speeds = [30, 60, 80, 100, 120]

# Assumed pothole distance for simulation
distance = 30

for speed in speeds:
    speed_warning(speed, distance)
