#pragma once

#include <stddef.h>
#include <stdint.h>
#include "flight_path.h"
#include "crsf_definitions.h"

// Telemetry frames are sent in 50 ms slots, the schedule repeats every second
enum { CRSF_TX_SLOT_MS = 50 };
enum { CRSF_TX_SLOT_COUNT = 20 };
enum { CRSF_TX_FLIGHT_MODE_MAX_LEN = 15 }; // longer flight mode names are cut

struct crsf_tx_frame_t {
	uint8_t bytes [CRSF_FRAME_SIZE_MAX];
	size_t size; // 0 when there is nothing to send
};

// Frame type to send in the given slot, 0 for an empty slot
uint8_t CRSF_TX_frameTypeForSlot(uint8_t slot);
// Encodes a telemetry frame from the flight path sample, needs the CRC8 LUT (created by CRSF_RX_begin)
crsf_tx_frame_t CRSF_TX_encode(uint8_t frame_type, const flight_sample_t & sample);
