#pragma once

#include <stdint.h>

// CRC-8 shared by received and sent CRSF frames, CRSF uses poly 0xD5 (DVB-S2)
void CRC8_createLut(uint8_t poly);
uint8_t CRC8_calculate(const uint8_t * data, int len);
