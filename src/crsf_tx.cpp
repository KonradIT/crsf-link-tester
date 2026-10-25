#include "crsf_tx.h"
#include "crsf_crc.h"

#include <math.h>

// All multi-byte fields are big-endian.
// See: https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md

static void put8(crsf_tx_frame_t & frame, uint8_t value) {
	frame.bytes[frame.size++] = value;
}

static void put16(crsf_tx_frame_t & frame, uint16_t value) {
	put8(frame, value >> 8);
	put8(frame, value);
}

static void put32(crsf_tx_frame_t & frame, uint32_t value) {
	put16(frame, value >> 16);
	put16(frame, value);
}

// clamp to [low, high], then round to the nearest integer
static long rounded(double value, long low, long high) {
	return lround(fmax((double) low, fmin((double) high, value)));
}

uint8_t CRSF_TX_frameTypeForSlot(uint8_t slot) {
	// 17 frames / second, staggered so a slow RF downlink can coalesce updates.
	// GPS + attitude 5 Hz; battery + baro + vario 2 Hz; flight mode 1 Hz.
	static const uint8_t schedule [CRSF_TX_SLOT_COUNT] = {
		CRSF_FRAMETYPE_GPS, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAMETYPE_BARO_ALTITUDE,
		CRSF_FRAMETYPE_GPS, CRSF_FRAMETYPE_VARIO_SENSOR, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAMETYPE_FLIGHT_MODE,
		CRSF_FRAMETYPE_GPS, 0, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAMETYPE_BATTERY_SENSOR,
		CRSF_FRAMETYPE_GPS, CRSF_FRAMETYPE_BARO_ALTITUDE, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAMETYPE_VARIO_SENSOR,
		CRSF_FRAMETYPE_GPS, 0, CRSF_FRAMETYPE_ATTITUDE, 0
	};
	return schedule[slot % CRSF_TX_SLOT_COUNT];
}

crsf_tx_frame_t CRSF_TX_encode(uint8_t frame_type, const flight_sample_t & s) {
	crsf_tx_frame_t f = {};
	put8(f, CRSF_SYNC_BYTE);
	put8(f, 0); // frame length, set below
	put8(f, frame_type);
	switch (frame_type) {
		case CRSF_FRAMETYPE_GPS:
			put32(f, (int32_t) rounded(s.latitude_deg * 1e7, -900000000, 900000000));
			put32(f, (int32_t) rounded(s.longitude_deg * 1e7, -1800000000, 1800000000));
			put16(f, rounded(s.ground_speed_mps * 36.0, 0, 65535)); // km/h * 10
			put16(f, rounded(s.course_deg * 100.0, 0, 36000) % 36000);
			put16(f, rounded(s.altitude_m + 1000.0, 0, 65535)); // +1000 m offset
			put8(f, s.satellites);
			break;
		case CRSF_FRAMETYPE_BATTERY_SENSOR: {
			put16(f, rounded(s.voltage_v * 10.0, 0, 32767));
			put16(f, rounded(s.current_a * 10.0, 0, 32767));
			uint32_t used_mah = rounded(s.used_mah, 0, 0xffffff); // 24 bit
			put8(f, used_mah >> 16);
			put16(f, used_mah);
			put8(f, s.remaining_percent > 100 ? 100 : s.remaining_percent);
			break;
		}
		case CRSF_FRAMETYPE_ATTITUDE: // radians * 10000
			put16(f, rounded(s.pitch_rad * 10000.0, -31416, 31416));
			put16(f, rounded(s.roll_rad * 10000.0, -31416, 31416));
			put16(f, rounded(s.yaw_rad * 10000.0, -31416, 31416));
			break;
		case CRSF_FRAMETYPE_VARIO_SENSOR: // cm/s
			put16(f, rounded(s.vertical_speed_mps * 100.0, -32768, 32767));
			break;
		case CRSF_FRAMETYPE_BARO_ALTITUDE: {
			// decimeters + 10000, or meters with the top bit set when above 2276.7 m
			long dm = rounded(s.baro_altitude_m * 10.0, -10000, 327660);
			put16(f, dm < 22768 ? dm + 10000 : 0x8000 | ((dm + 5) / 10));
			// unlike 0x07, vertical speed here is signed and logarithmic
			long packed = rounded(log1p(fabs(s.vertical_speed_mps)) / 0.026, 0, 127);
			if (s.vertical_speed_mps < 0) {
				packed = -packed;
			}
			put8(f, packed);
			break;
		}
		case CRSF_FRAMETYPE_FLIGHT_MODE:
			for (int i = 0; s.flight_mode[i] != '\0' && i < CRSF_TX_FLIGHT_MODE_MAX_LEN; i++) {
				put8(f, s.flight_mode[i]);
			}
			put8(f, 0); // NUL terminated
			break;
		default:
			return crsf_tx_frame_t {};
	}
	f.bytes[1] = f.size - 1; // type + payload + CRC
	uint8_t crc = CRC8_calculate(&f.bytes[2], f.size - 2);
	put8(f, crc);
	return f;
}
