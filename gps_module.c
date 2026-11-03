
#include <stdio.h>
#include <math.h>

#define EARTH_RADIUS_KM 6371.0

typedef struct {
    double lat, lon;
} Location;

typedef struct {
    char name[60];
    double lat, lon;
    char severity[20];
} Hazard;

typedef struct {
    char name[60];
    char type[30];
    double lat, lon;
} Service;

Hazard hazards[] = {
    {"Pothole_Main_Road", 30.3200, 78.0400, "High"},
    {"Pothole_Market", 30.3100, 78.0250, "Medium"},
    {"Damaged_Road", 30.3250, 78.0200, "Low"}
};

Service services[] = {
    {"City_Petrol_Pump", "Petrol Pump", 30.3180, 78.0350},
    {"Quick_Tyre_Repair", "Puncture Shop", 30.3120, 78.0280},
    {"Roadside_Cafe", "Food", 30.3220, 78.0300}
};

double toRadians(double d) {
    return d * 3.14159265358979323846 / 180.0;
}

double distanceKm(Location a, Location b) {
    double lat1 = toRadians(a.lat);
    double lat2 = toRadians(b.lat);
    double dLat = toRadians(b.lat - a.lat);
    double dLon = toRadians(b.lon - a.lon);

    double h = sin(dLat / 2) * sin(dLat / 2)
             + cos(lat1) * cos(lat2)
             * sin(dLon / 2) * sin(dLon / 2);

    if (h > 1) h = 1;
    if (h < 0) h = 0;

    return EARTH_RADIUS_KM * 2 *
           atan2(sqrt(h), sqrt(1 - h));
}

int loadGPSLocation(Location *loc) {
    FILE *f = fopen("location.txt", "r");

    if (!f) {
        printf("Cannot open location.txt\n");
        return 0;
    }

    int result = fscanf(f, "%lf %lf", &loc->lat, &loc->lon);
    fclose(f);

    if (result != 2 ||
        !isfinite(loc->lat) || !isfinite(loc->lon) ||
        loc->lat < -90 || loc->lat > 90 ||
        loc->lon < -180 || loc->lon > 180) {
        printf("Invalid GPS coordinates\n");
        return 0;
    }

    return 1;
}

void showHazards(Location current, double radius) {
    int found = 0;

    printf("\n--- NEARBY HAZARDS ---\n");

    for (int i = 0; i < 3; i++) {
        Location h = {hazards[i].lat, hazards[i].lon};
        double d = distanceKm(current, h);

        if (d <= radius) {
            printf("%s | Severity: %s | Distance: %.0f metres\n",
                   hazards[i].name, hazards[i].severity, d * 1000);
            found = 1;
        }
    }

    if (!found)
        printf("No hazards within %.2f km.\n", radius);
}

void showServices(Location current, double radius) {
    int found = 0;

    printf("\n--- NEARBY SERVICES ---\n");

    for (int i = 0; i < 3; i++) {
        Location s = {services[i].lat, services[i].lon};
        double d = distanceKm(current, s);

        if (d <= radius) {
            printf("%s (%s) | Distance: %.2f km\n",
                   services[i].name, services[i].type, d);
            found = 1;
        }
    }

    if (!found)
        printf("No services within %.2f km.\n", radius);
}

int main(void) {
    Location current, destination;
    int choice;
    double radius;

    if (!loadGPSLocation(&current)) {
        printf("Open gps.html and obtain your location first.\n");
        return 1;
    }

    do {
        printf("\n====================================\n");
        printf(" SMART ROAD ASSISTANCE GPS MODULE\n");
        printf("====================================\n");
        printf("1. Refresh GPS location\n");
        printf("2. Display current location\n");
        printf("3. Find nearby potholes\n");
        printf("4. Find nearby services\n");
        printf("5. Calculate distance to destination\n");
        printf("0. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1)
            return 1;

        switch (choice) {
            case 1:
                if (loadGPSLocation(&current))
                    printf("GPS location refreshed!\n");
                break;

            case 2:
                printf("Latitude: %.6f\n", current.lat);
                printf("Longitude: %.6f\n", current.lon);
                break;

            case 3:
            case 4:
                printf("Enter search radius in km: ");
                if (scanf("%lf", &radius) != 1 ||
                    !isfinite(radius) || radius < 0) {
                    printf("Invalid radius.\n");
                    break;
                }

                if (choice == 3)
                    showHazards(current, radius);
                else
                    showServices(current, radius);
                break;

            case 5:
                printf("Enter destination latitude: ");
                if (scanf("%lf", &destination.lat) != 1)
                    return 1;

                printf("Enter destination longitude: ");
                if (scanf("%lf", &destination.lon) != 1)
                    return 1;

                if (!isfinite(destination.lat) ||
                    !isfinite(destination.lon) ||
                    destination.lat < -90 ||
                    destination.lat > 90 ||
                    destination.lon < -180 ||
                    destination.lon > 180) {
                    printf("Invalid destination coordinates.\n");
                    break;
                }

                printf("Straight-line distance: %.2f km\n",
                       distanceKm(current, destination));
                break;

            case 0:
                printf("Exiting. Drive safely!\n");
                break;

            default:
                printf("Invalid choice.\n");
        }

    } while (choice != 0);

    return 0;
}
