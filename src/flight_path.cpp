#include "flight_path.h"

#include <math.h>
#include <stddef.h>

// One KML track point, field order matches scripts/kml_to_flight_path.py
struct flight_path_point_t {
	uint32_t time_ms;           // since the first point
	int32_t latitude;           // degrees * 1e7
	int32_t longitude;          // degrees * 1e7
	float altitude_m;
	float yaw_deg;              // 0..360
	float pitch_deg;
	float roll_deg;
	float ground_speed_mps;
	float course_deg;
	float vertical_speed_mps;
	float baro_altitude_m;
	float voltage_v;
	float current_a;
	float used_mah;
	uint8_t remaining_percent;
	uint8_t satellites;
	uint8_t flight_mode;        // index into FLIGHT_PATH_STRINGS
	uint8_t phase;
	uint8_t waypoint;
};

#include "flight_path_data.h" // generated in the build folder

static const size_t FLIGHT_PATH_POINT_COUNT = sizeof(FLIGHT_PATH_POINTS) / sizeof(FLIGHT_PATH_POINTS[0]);
static const double DEG_TO_RAD_FACTOR = 3.14159265358979323846 / 180.0;

static double lerp(double a, double b, double f) {
	return a + (b - a) * f;
}

// interpolates the short way around the circle, result in 0..360
static double lerpDegrees(double a, double b, double f) {
	double diff = fmod(b - a + 540.0, 360.0) - 180.0;
	return fmod(a + diff * f + 360.0, 360.0);
}

flight_sample_t FLIGHT_PATH_sampleAt(uint64_t elapsed_ms, bool repeat) {
	uint32_t duration_ms = FLIGHT_PATH_POINTS[FLIGHT_PATH_POINT_COUNT - 1].time_ms;
	flight_sample_t s = {};
	uint32_t t = duration_ms;
	s.mission_number = 1;
	if (repeat) {
		s.mission_number = elapsed_ms / duration_ms + 1;
		t = elapsed_ms % duration_ms;
	}
	else if (elapsed_ms < duration_ms) {
		t = elapsed_ms;
	}
	s.mission_seconds = t / 1000.0;

	// last point at or before t
	size_t low = 0;
	size_t high = FLIGHT_PATH_POINT_COUNT - 1;
	while (low < high) {
		size_t mid = (low + high + 1) / 2;
		if (FLIGHT_PATH_POINTS[mid].time_ms <= t) {
			low = mid;
		}
		else {
			high = mid - 1;
		}
	}
	const flight_path_point_t & a = FLIGHT_PATH_POINTS[low];
	const flight_path_point_t & b = FLIGHT_PATH_POINTS[low + 1 < FLIGHT_PATH_POINT_COUNT ? low + 1 : low];
	double f = (b.time_ms > a.time_ms) ? (double) (t - a.time_ms) / (b.time_ms - a.time_ms) : 0.0;

	s.latitude_deg = lerp(a.latitude, b.latitude, f) / 1e7;
	s.longitude_deg = lerp(a.longitude, b.longitude, f) / 1e7;
	s.altitude_m = lerp(a.altitude_m, b.altitude_m, f);
	s.baro_altitude_m = lerp(a.baro_altitude_m, b.baro_altitude_m, f);
	s.ground_speed_mps = lerp(a.ground_speed_mps, b.ground_speed_mps, f);
	s.course_deg = lerpDegrees(a.course_deg, b.course_deg, f);
	s.vertical_speed_mps = lerp(a.vertical_speed_mps, b.vertical_speed_mps, f);
	s.pitch_rad = lerp(a.pitch_deg, b.pitch_deg, f) * DEG_TO_RAD_FACTOR;
	s.roll_rad = lerp(a.roll_deg, b.roll_deg, f) * DEG_TO_RAD_FACTOR;
	double yaw_deg = lerpDegrees(a.yaw_deg, b.yaw_deg, f);
	s.yaw_rad = (yaw_deg > 180.0 ? yaw_deg - 360.0 : yaw_deg) * DEG_TO_RAD_FACTOR;
	s.voltage_v = lerp(a.voltage_v, b.voltage_v, f);
	s.current_a = lerp(a.current_a, b.current_a, f);
	s.used_mah = lerp(a.used_mah, b.used_mah, f);
	s.remaining_percent = lround(lerp(a.remaining_percent, b.remaining_percent, f));
	s.satellites = lround(lerp(a.satellites, b.satellites, f));
	// text changes at the point that sets it
	s.flight_mode = FLIGHT_PATH_STRINGS[a.flight_mode];
	s.phase = FLIGHT_PATH_STRINGS[a.phase];
	s.waypoint = FLIGHT_PATH_STRINGS[a.waypoint];
	return s;
}
