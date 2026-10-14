#include <stdio.h>
#include <math.h>
#include <string.h>

#define MAX_HAZARDS 50
#define MAX_SERVICES 50
#define EARTH_RADIUS_KM 6371.0

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

typedef struct {
    double lat, lon;
} Location;

Hazard hazards[MAX_HAZARDS] = {
    {"Pothole near Main Road", 30.3200, 78.0400, "High"},
    {"Pothole near Market",    30.3100, 78.0250, "Medium"},
    {"Damaged road section",   30.3250, 78.0200, "Low"}
};
int hazardCount = 3;

Service services[MAX_SERVICES] = {
    {"City Petrol Pump", "Petrol Pump", 30.3180, 78.0350},
    {"Quick Tyre Repair", "Puncture Shop", 30.3120, 78.0280},
    {"Roadside Cafe", "Food", 30.3220, 78.0300}
};
int serviceCount = 3;

double toRadians(double degrees) {
    return degrees * 3.14159265358979323846 / 180.0;
}

/* Haversine formula: straight-line distance between two GPS coordinates */
double distanceKm(Location a, Location b) {
    double lat1 = toRadians(a.lat);
    double lat2 = toRadians(b.lat);
    double dLat = toRadians(b.lat - a.lat);
    double dLon = toRadians(b.lon - a.lon);

    double h = sin(dLat / 2) * sin(dLat / 2)
             + cos(lat1) * cos(lat2)
             * sin(dLon / 2) * sin(dLon / 2);

    if (h > 1.0) h = 1.0;
    if (h < 0.0) h = 0.0;

    return EARTH_RADIUS_KM * 2.0 * atan2(sqrt(h), sqrt(1.0 - h));
}

void readLocation(Location *loc, const char *label) {
    printf("Enter %s latitude: ", label);
    scanf("%lf", &loc->lat);
    printf("Enter %s longitude: ", label);
    scanf("%lf", &loc->lon);
}

int validLocation(Location loc) {
    return loc.lat >= -90 && loc.lat <= 90
        && loc.lon >= -180 && loc.lon <= 180;
}

void showCurrentLocation(Location current) {
    printf("\n--- CURRENT GPS LOCATION (SIMULATED) ---\n");
    printf("Latitude : %.6f\n", current.lat);
    printf("Longitude: %.6f\n", current.lon);
}

void showNearbyHazards(Location current, double radiusKm) {
    int found = 0;
    printf("\n--- ROAD HAZARDS WITHIN %.2f KM ---\n", radiusKm);

    for (int i = 0; i < hazardCount; i++) {
        Location h = {hazards[i].lat, hazards[i].lon};
        double d = distanceKm(current, h);

        if (d <= radiusKm) {
            printf("%s | Severity: %s | Distance: %.0f metres\n",
                   hazards[i].name, hazards[i].severity, d * 1000.0);
            found = 1;
        }
    }

    if (!found) {
        printf("No stored hazards found within this radius.\n");
    }
}

void showNearbyServices(Location current, double radiusKm) {
    int found = 0;
    printf("\n--- NEARBY SERVICES WITHIN %.2f KM ---\n", radiusKm);

    for (int i = 0; i < serviceCount; i++) {
        Location s = {services[i].lat, services[i].lon};
        double d = distanceKm(current, s);

        if (d <= radiusKm) {
            printf("%s (%s) | Distance: %.2f km\n",
                   services[i].name, services[i].type, d);
            found = 1;
        }
    }

    if (!found) {
        printf("No stored services found within this radius.\n");
    }
}

void addHazard(void) {
    if (hazardCount >= MAX_HAZARDS) {
        printf("Hazard storage is full.\n");
        return;
    }

    Hazard h;
    printf("Enter hazard name (one word or use underscores): ");
    scanf("%59s", h.name);
    printf("Enter severity (Low/Medium/High): ");
    scanf("%19s", h.severity);
    printf("Enter hazard latitude: ");
    scanf("%lf", &h.lat);
    printf("Enter hazard longitude: ");
    scanf("%lf", &h.lon);

    Location loc = {h.lat, h.lon};
    if (!validLocation(loc)) {
        printf("Invalid coordinates. Hazard not added.\n");
        return;
    }

    hazards[hazardCount++] = h;
    printf("Hazard added successfully.\n");
}

void estimateRoute(Location source, Location destination) {
    double d = distanceKm(source, destination);

    printf("\n--- ROUTE DISTANCE ESTIMATE ---\n");
    printf("Source      : %.6f, %.6f\n", source.lat, source.lon);
    printf("Destination : %.6f, %.6f\n", destination.lat, destination.lon);
    printf("Straight-line distance: %.2f km\n", d);
    printf("Approx. time at 30 km/h: %.0f minutes\n", (d / 30.0) * 60.0);
    printf("Note: This is not a road route; actual roads may be longer.\n");
}

int main(void) {
    Location current = {30.3165, 78.0322}; /* Example coordinates */
    Location destination;
    int choice;
    double radius;

    do {
        printf("\n====================================\n");
        printf(" SMART ROAD ASSISTANCE - GPS MODULE\n");
        printf("====================================\n");
        printf("1. Enter / update current location\n");
        printf("2. Display current location\n");
        printf("3. Find nearby potholes / hazards\n");
        printf("4. Find nearby services\n");
        printf("5. Estimate distance to destination\n");
        printf("6. Add a hazard coordinate\n");
        printf("0. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting.\n");
            return 1;
        }

        switch (choice) {
            case 1:
                readLocation(&current, "current");
                if (!validLocation(current)) {
                    printf("Invalid coordinates. Please enter valid values.\n");
                    current.lat = 30.3165;
                    current.lon = 78.0322;
                } else {
                    printf("Current location updated.\n");
                }
                break;

            case 2:
                showCurrentLocation(current);
                break;

            case 3:
                printf("Search radius in km (for example 1 or 5): ");
                scanf("%lf", &radius);
                if (radius < 0) {
                    printf("Radius cannot be negative.\n");
                } else {
                    showNearbyHazards(current, radius);
                }
                break;

            case 4:
                printf("Search radius in km (for example 1 or 5): ");
                scanf("%lf", &radius);
                if (radius < 0) {
                    printf("Radius cannot be negative.\n");
                } else {
                    showNearbyServices(current, radius);
                }
                break;

            case 5:
                readLocation(&destination, "destination");
                if (!validLocation(destination)) {
                    printf("Invalid destination coordinates.\n");
                } else {
                    estimateRoute(current, destination);
                }
                break;

            case 6:
                addHazard();
                break;

            case 0:
                printf("Exiting GPS module. Drive safely!\n");
                break;

            default:
                printf("Invalid choice. Try again.\n");
        }
    } while (choice != 0);

    return 0;
}
