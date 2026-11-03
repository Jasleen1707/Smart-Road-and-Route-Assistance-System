
#include <stdio.h>

int main(void) {
    double latitude, longitude;

    FILE *file = fopen("location.txt", "r");


    if (file == NULL) {
        printf("Error: Cannot open location.txt\n");
        return 1;
    }

    if (fscanf(file, "%lf %lf", &latitude, &longitude) != 2) {
        printf("Error: Invalid coordinates in file\n");
        fclose(file);
        return 1;
    }

    fclose(file);

    printf("User Latitude: %.6f\n", latitude);
    printf("User Longitude: %.6f\n", longitude);

    return 0;
}
