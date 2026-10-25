#pragma once

#include <stdint.h>
#include "flight_path.h"

// Fake flight controller telemetry (GPS, battery, attitude, vario, baro, flight mode)
// sent to the receiver over the CRSF UART, played back from the bundled flight path.
void CRSF_TELEMETRY_begin();
void CRSF_TELEMETRY_loop();
void CRSF_TELEMETRY_getStatus(flight_sample_t * sample, uint32_t * sent_frames);
