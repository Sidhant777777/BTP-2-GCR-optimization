#include <iostream>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <string>

using namespace std;

// Constants
const double PI = 3.14159265358979323846;

// Helper Function: The core mathematics for Kharagpur
double calculate_cosine_effect(int day, double hour, double lat_deg = 22.34) {
    // 1. Convert latitude to radians
    double lat = lat_deg * PI / 180.0;
    
    // 2. Declination Angle
    double B = (360.0 / 365.0) * (day - 81) * PI / 180.0;
    double declination = 23.45 * sin(B) * PI / 180.0;
    
    // 3. Hour Angle
    double hour_angle = 15.0 * (hour - 12.0) * PI / 180.0;
    
    // 4. Zenith Angle
    double cos_zenith = sin(lat) * sin(declination) + cos(lat) * cos(declination) * cos(hour_angle);
    cos_zenith = max(-1.0, min(1.0, cos_zenith)); // Clamp to avoid NaN
    double zenith = acos(cos_zenith);
    
    // Nighttime filter (Sun is below horizon)
    if (zenith * 180.0 / PI > 90.0) {
        return 0.0; 
    }
    
    // 5. Azimuth Angle
    double cos_az;
    if (sin(zenith) == 0.0) {
        cos_az = 1.0;
    } else {
        cos_az = (sin(lat) * cos_zenith - sin(declination)) / (cos(lat) * sin(zenith));
    }
    
    cos_az = max(-1.0, min(1.0, cos_az));
    double azimuth = acos(cos_az);
    
    if (hour > 12.0) {
        azimuth = 2.0 * PI - azimuth;
    }
    
    // 6. Incidence Angle for North-South Single-Axis Tracker
    double cos_theta = sqrt(pow(cos(zenith), 2) + pow(sin(zenith) * sin(azimuth), 2));
    
    return cos_theta;
}

int main() {
    
    // ====================================================================
    // USER CONFIGURATION: Set your day of the year right here before running
    // (e.g., 81 = Equinox, 172 = Summer Solstice, 355 = Winter Solstice)
    // ====================================================================
    int target_day = 81; 
    
    // Setup the output CSV file
    string filename = "Cosine_Effect_Day_" + to_string(target_day) + ".csv";
    ofstream outFile(filename);
    
    // Write the column headers for Excel
    outFile << "Solar_Time_Hours,Cosine_Theta\n";

    cout << "--- Single-Axis Tracker Optical Simulator ---" << endl;
    cout << "Calculating solar tracking geometry for Day " << target_day << "..." << endl;
    cout << "\n----------------------------------------" << endl;
    cout << " Solar Time (Hrs) | Cosine Effect Factor " << endl;
    cout << "----------------------------------------" << endl;
    
    // Sweep the day from 6:30 AM to 5:30 PM in 15-minute intervals (0.25 hours)
    for (double h = 6.5; h <= 17.5; h += 0.25) { 
        double cos_theta = calculate_cosine_effect(target_day, h);
        
        // Only log and print the data if the sun is actually up
        if (cos_theta > 0.0) { 
            // 1. Save to the CSV file
            outFile << fixed << setprecision(2) << h << "," 
                    << setprecision(4) << cos_theta << "\n";
            
            // 2. Print to the Console
            cout << "      " << fixed << setprecision(2) << setw(5) << h 
                 << "       |      " << setprecision(4) << cos_theta << endl;
        }
    }

    cout << "----------------------------------------\n" << endl;

    outFile.close();
    
    // Print success message
    cout << "Success! Data saved locally as '" << filename << "'." << endl;

    return 0;
}
