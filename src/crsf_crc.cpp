#include "crsf_crc.h"

static uint8_t crc8_lut [256];

void CRC8_createLut(uint8_t poly)
{
	for (int i=0; i < 256; i++) {
		uint8_t crc = i;
		for (int j=0; j < 8; j++) {
			crc = (crc << 1) ^ ((crc & 0x80) ? poly : 0);
		}
		crc8_lut[i] = crc;
	}
}

uint8_t CRC8_calculate(const uint8_t * data, int len) {
	uint8_t crc = 0;
	for (int i=0; i < len; i++) {
		crc = crc8_lut[crc ^ *data++];
	}
	return crc;
}
