#pragma once

#include <stdint.h>

// Fake telemetry state at one moment of the flight path bundled at build time
// (custom_flight_path in platformio.ini, converted by scripts/kml_to_flight_path.py).
struct flight_sample_t {
	double latitude_deg;
	double longitude_deg;
	double altitude_m;          // GPS altitude above mean sea level
	double baro_altitude_m;     // barometric altitude above takeoff
	double ground_speed_mps;
	double course_deg;          // course over ground, 0..360
	double vertical_speed_mps;
	double pitch_rad;
	double roll_rad;
	double yaw_rad;             // -pi..pi
	double voltage_v;
	double current_a;
	double used_mah;
	uint8_t remaining_percent;
	uint8_t satellites;
	const char * flight_mode;
	const char * phase;         // telemetry page only, may be empty
	const char * waypoint;      // next waypoint, telemetry page only, may be empty
	double mission_seconds;     // time since the start of the current run
	uint32_t mission_number;    // 1 for the first run
};

// Interpolated sample, elapsed_ms counts from the first point. With repeat the flight
// restarts after its last point, otherwise it holds the last point.
flight_sample_t FLIGHT_PATH_sampleAt(uint64_t elapsed_ms, bool repeat);
