#include "telemetry.h"
#include "crsf_tx.h"

#include <Arduino.h>

#ifndef MISSION_REPEAT
	#define MISSION_REPEAT 1 // 0: hold at the last point of the flight path instead of restarting it
#endif

static uint64_t flight_elapsed_ms = 0; // 64 bit, keeps counting across the millis() rollover
static uint32_t last_clock_ms = 0;
static flight_sample_t latest_sample;
static uint32_t sent_frames = 0;
static uint32_t last_slot_ms = 0;
static uint8_t slot = 0;
static portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;

// Serial1 and the CRC8 LUT must already be set up by CRSF_RX_begin()
void CRSF_TELEMETRY_begin() {
	uint32_t now = millis();
	last_clock_ms = now;
	last_slot_ms = now - CRSF_TX_SLOT_MS;
	latest_sample = FLIGHT_PATH_sampleAt(0, MISSION_REPEAT != 0);
}

// Call from the CRSF polling task: sends at most one frame per 50 ms slot
void CRSF_TELEMETRY_loop() {
	uint32_t now = millis();
	if (now - last_slot_ms < CRSF_TX_SLOT_MS) {
		return;
	}
	last_slot_ms = now; // no catch-up burst after a slow pass
	flight_elapsed_ms += now - last_clock_ms;
	last_clock_ms = now;
	flight_sample_t sample = FLIGHT_PATH_sampleAt(flight_elapsed_ms, MISSION_REPEAT != 0);
	crsf_tx_frame_t frame = CRSF_TX_encode(CRSF_TX_frameTypeForSlot(slot), sample);
	slot = (slot + 1) % CRSF_TX_SLOT_COUNT;
	if (frame.size != 0) {
		Serial1.write(frame.bytes, frame.size);
	}
	portENTER_CRITICAL(&status_lock);
	latest_sample = sample;
	if (frame.size != 0) {
		sent_frames++;
	}
	portEXIT_CRITICAL(&status_lock);
}

void CRSF_TELEMETRY_getStatus(flight_sample_t * sample, uint32_t * frames) {
	portENTER_CRITICAL(&status_lock);
	*sample = latest_sample;
	*frames = sent_frames;
	portEXIT_CRITICAL(&status_lock);
}
