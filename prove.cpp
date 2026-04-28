#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
// ... [Insert the calculate_sun_position and calculate_optical_power functions here] ...



// --- 0. HELPER FUNCTIONS & CONSTANTS ---
const double PI = 3.14159265358979323846;

double deg2rad(double degrees) {
    return degrees * (PI / 180.0);
}

double rad2deg(double radians) {
    return radians * (180.0 / PI);
}

// --- PHASE 1: ASTRONOMICAL SOLAR GEOMETRY ---
// Calculates where the sun is in the sky for Kharagpur
struct SunPosition {
    double zenith_angle;  // theta_z (radians)
    double azimuth_angle; // gamma_s (radians)
    double declination;   // delta (radians)
    double hour_angle;    // omega (radians)
};

SunPosition calculate_sun_position(int day_of_year, double solar_time_hours) {
    SunPosition sun;
    
    // Kharagpur Latitude (22.34 N)
    double latitude = deg2rad(18.92); 
    
    // 1. Declination Angle (delta)
    double B = deg2rad((360.0 / 365.0) * (day_of_year - 81));
    sun.declination = deg2rad(23.45 * sin(B));
    
    // 2. Hour Angle (omega): 15 degrees per hour from Solar Noon
    sun.hour_angle = deg2rad(15.0 * (solar_time_hours - 12.0));
    
    // 3. Zenith Angle (theta_z)
    double cos_zenith = sin(latitude) * sin(sun.declination) + 
                        cos(latitude) * cos(sun.declination) * cos(sun.hour_angle);
    sun.zenith_angle = acos(cos_zenith);
    
    // 4. Solar Azimuth (gamma_s)
    double cos_azimuth = (sin(latitude) * cos_zenith - sin(sun.declination)) / 
                         (cos(latitude) * sin(sun.zenith_angle));
    
    // Clamp to avoid NaN errors from floating point imprecision
    cos_azimuth = std::max(-1.0, std::min(1.0, cos_azimuth)); 
    sun.azimuth_angle = acos(cos_azimuth);
    
    // Adjust Azimuth based on morning/afternoon
    if (solar_time_hours > 12.0) {
        sun.azimuth_angle = 2.0 * PI - sun.azimuth_angle;
    }
    
    return sun;
}

// --- PHASE 2 & 3: TRACKING KINEMATICS & PHYSICAL LOSSES ---
// Calculates the tracking angle, incidence angle, and applies the three shading effects
double calculate_optical_power(SunPosition sun, double DNI, double row_spacing, 
                               double aperture_width, double focal_length, 
                               double row_length, double optical_efficiency) {
    
    // If the sun is below the horizon, power is 0
    if (rad2deg(sun.zenith_angle) > 90.0) return 0.0;

    // --- NEW: AXIS DEVIATION ANGLE (psi) ---
    // 0.0 means perfectly aligned North-South.
    double psi = deg2rad(0.0); 

    // 1. Calculate Solar Altitude
    double altitude = (PI / 2.0) - sun.zenith_angle; 

    // 2. Calculate Tracking Angle (beta) for North-South aligned axis
    double tracking_angle = atan(sin(sun.azimuth_angle) / tan(altitude));

    // --- NEW: PAPER'S INCIDENCE ANGLE MATHEMATICS ---
    // The shared denominator for Equations 5 and 6
    double root_term = sqrt(pow(sin(sun.azimuth_angle + psi), 2) + pow(tan(altitude), 2));

    // Equation 5: Calculate tan(theta) directly for the Optical End Loss
    double tan_theta = cos(sun.azimuth_angle + psi) / root_term;

    // Equation 6: Calculate cos(theta) directly for the Cosine Effect
    double cos_incidence = cos(altitude) * root_term;

    // --- APPLY LOSS 1: THE COSINE EFFECT ---
    double effective_DNI = DNI * cos_incidence;

    // --- APPLY LOSS 2: INTER-ROW SHADING ---
    // Shadow creeps from the left. Edge coordinate x:
    double x = -row_spacing * sin(tracking_angle) + (aperture_width / 2.0);
    
    // Clamp x to the physical boundaries of the mirror
    x = std::max(-aperture_width / 2.0, std::min(aperture_width / 2.0, x));
    
    // Calculate Active Unshaded Width
    double unshaded_width = (aperture_width / 2.0) - x;

    // --- APPLY LOSS 3: OPTICAL END LOSS ---
    // Using the integrated average loss formula for curved parabolas
    double curvature_penalty = pow(aperture_width, 2) / (48.0 * focal_length);
    
    // Replace standard tan(theta) with the paper's specific tan_theta calculation
    double avg_end_loss = (focal_length + curvature_penalty) * std::abs(tan_theta); 
    
    // Prevent the end loss from exceeding the actual physical length of the row
    avg_end_loss = std::min(row_length, avg_end_loss);
    double active_lit_length = row_length - avg_end_loss;

    // --- PHASE 4: FINAL RAW SOLAR POWER ---
    double active_area = unshaded_width * active_lit_length;
    double raw_solar_watts = effective_DNI * active_area * optical_efficiency;

    return raw_solar_watts;
}



int main() {
    // 1. Fixed Land and Hardware Parameters
    double land_width_meters = 10.0;   // Optimize for a 100m wide plot of land
    double aperture_width = 1.20;        // meters (W)
    double focal_length = 0.6;          // meters (f)
    double row_length = 10.0;           // meters
    double optical_efficiency = 0.35; 

    // 2. Optimization Tracking Variables
    double best_spacing = 0.0;
    double max_annual_yield_kWh = 0.0;
    
    std::cout << "Starting Inter-Row Spacing Optimization Sweep..." << std::endl;

    // --- NEW: Open CSV file to save data for the Excel graph ---
    std::ofstream outFile("sawtooth_graph_data.csv");
    outFile << "Distance_m,Number_of_Rows,Total_Yield_kWh\n";

    // --- THE OUTER LOOP: Sweeping the Inter-Row Distance (D) ---
    // Start at a wide 12.0 meters, step down by 10cm (0.1m) until they are almost touching (2.1m)
    for (double spacing = 12.0; spacing >= 1.2; spacing -= 0.01) {
        
        // Calculate how many parallel rows we can actually fit on the land
        int num_rows = floor(land_width_meters / spacing);
        
        double annual_energy_one_row_Joules = 0.0;

        // --- THE INNER LOOP: The 15-Minute Time Steps for a Full Year ---
        // 365 days * 24 hours * 4 (15-min intervals) = 35,040 steps
        for (int step = 0; step < 35040; step++) {
            
            // Translate the 'step' into Day of Year and Solar Time
            int day_of_year = (step / 96) + 1; // 96 steps in a day
            double solar_time = (step % 96) * 0.25; // 0.25 hours = 15 mins
            
            // Mock Weather Data (You will replace this with reading your CSV weather file)
            // For now, let's just pretend the sun shines perfectly from 6 AM to 6 PM
            double DNI = (solar_time >= 9.0 && solar_time <= 15.0) ? 1900.0 : 0.0;
            
            if (DNI > 0) {
                SunPosition sun = calculate_sun_position(day_of_year, solar_time);
                
                // Calculate average Watts over this 15-minute block
                double block_power_watts = calculate_optical_power(sun, DNI, spacing, 
                                                                   aperture_width, focal_length, 
                                                                   row_length, optical_efficiency);
                                                                   
                // Multiply Watts by seconds (15 mins = 900 seconds) to get Joules of Energy
                double block_energy_joules = block_power_watts * 900.0;
                
                annual_energy_one_row_Joules += block_energy_joules;
            }
        }
        
        // --- EVALUATE THIS SPACING ---
        // Convert Joules to kWh (1 kWh = 3.6 million Joules)
        double annual_energy_one_row_kWh = annual_energy_one_row_Joules / 3600000.0;
        
        // Multiply by the number of rows to get the total farm yield
        double total_farm_yield_kWh = annual_energy_one_row_kWh * num_rows;
        
        std::cout << "Testing Spacing: " << spacing << "m | Fit Rows: " << num_rows 
                  << " | Total Yield: " << total_farm_yield_kWh << " kWh" << std::endl;

        // --- NEW: Write this loop's final calculations to the CSV file ---
        outFile << spacing << "," << num_rows << "," << total_farm_yield_kWh << "\n";

        // Check if this spacing is the new champion
        if (total_farm_yield_kWh > max_annual_yield_kWh) {
            max_annual_yield_kWh = total_farm_yield_kWh;
            best_spacing = spacing;
        }
    }

    // --- NEW: Close the CSV file ---
    outFile.close();

    // --- FINAL RESULTS ---
    std::cout << "\n======================================" << std::endl;
    std::cout << "OPTIMIZATION COMPLETE" << std::endl;
    std::cout << "Data saved to 'sawtooth_graph_data.csv' for plotting." << std::endl;
    std::cout << "Optimal Inter-Row Distance: " << best_spacing << " meters" << std::endl;
    std::cout << "Maximum Annual Yield: " << max_annual_yield_kWh << " kWh" << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}
