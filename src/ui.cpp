
#include "ui.h"
#ifdef BOARD_M5CORE2
	#include <M5Core2.h>
	#define lcd M5.Lcd
#elif BOARD_M5STICKS3
	#include <M5Unified.h>
	#define lcd M5.Display
#else
	#include <TFT_eSPI.h>
	static TFT_eSPI _lcd = TFT_eSPI();
	#define lcd _lcd
#endif

#if defined(BOARD_M5CORE2) || defined(BOARD_M5STICKS3)
using UISprite = LGFX_Sprite;
#else
using UISprite = TFT_eSprite;
#endif

#define NUM_CHANNELS 6

static UISprite rssi_text(&lcd);
static UISprite rssi_bar(&lcd);
static UISprite lq_text(&lcd);
static UISprite lq_bar(&lcd);
static UISprite tx_pwr_text(&lcd);
static UISprite link_rate_text(&lcd);
static UISprite rx_frame_indicator_bar(&lcd);

static UISprite ch1_text(&lcd);
static UISprite ch2_text(&lcd);
static UISprite ch3_text(&lcd);
static UISprite ch4_text(&lcd);
static UISprite ch5_text(&lcd);
static UISprite ch6_text(&lcd);
static UISprite ch7_text(&lcd);
static UISprite ch8_text(&lcd);
static UISprite ch9_text(&lcd);
static UISprite ch10_text(&lcd);

static UISprite ch1_bar(&lcd);
static UISprite ch2_bar(&lcd);
static UISprite ch3_bar(&lcd);
static UISprite ch4_bar(&lcd);
static UISprite ch5_bar(&lcd);
static UISprite ch6_bar(&lcd);
static UISprite ch7_bar(&lcd);
static UISprite ch8_bar(&lcd);
static UISprite ch9_bar(&lcd);
static UISprite ch10_bar(&lcd);

static uint32_t packet_counter = 0;

static UISprite * channel_texts [] = {
	&ch1_text, &ch2_text, &ch3_text, &ch4_text, &ch5_text, &ch6_text, &ch7_text, &ch8_text, &ch9_text, &ch10_text
};

static UISprite * channel_bars [] = {
	&ch1_bar, &ch2_bar, &ch3_bar, &ch4_bar, &ch5_bar, &ch6_bar, &ch7_bar, &ch8_bar, &ch9_bar, &ch10_bar
};

static int rssi_scale_min = 120;
static int rssi_scale_max = 50;

// remember last values to prevent unnecessary sprite updates
static int current_rc_channels_data [10] = {0};
static int current_rssi = -1;
static int current_lq = -1;
static int current_pw = -1;

#if defined(BOARD_M5STICKS3)
static const int STICKS3_RSSI_TEXT_X = 4;
static const int STICKS3_RSSI_TEXT_Y = 4;
static const int STICKS3_RSSI_BAR_X = 4;
static const int STICKS3_RSSI_BAR_Y = 20;
static const int STICKS3_LQ_TEXT_X = 124;
static const int STICKS3_LQ_TEXT_Y = 4;
static const int STICKS3_LQ_BAR_X = 124;
static const int STICKS3_LQ_BAR_Y = 20;
static const int STICKS3_GRID_X = 2;
static const int STICKS3_GRID_Y = 42;
static const int STICKS3_GRID_COLS = 3;
static const int STICKS3_CELL_W = 78;
static const int STICKS3_CELL_H = 44;
static const int STICKS3_CH_TEXT_H = 22;
static const int STICKS3_CH_BAR_H = 12;
#endif


static void createElement(UISprite &sprite, uint8_t font_size, uint16_t width, uint16_t height, uint16_t color) {
	sprite.createSprite(width, height);
	sprite.setTextSize(font_size);
	sprite.setTextColor(color);
}

static void clearSprite(UISprite & s) {
	s.setCursor(0, 0);
	s.fillSprite(0);
}

static void drawProgressBar(UISprite & s, uint16_t color, int val, int v_min, int v_max) {
	s.fillSprite(0);
	s.drawRect(0, 0, s.width()-1, s.height()-1, color); // draw outline
	s.fillRect(0, 0, map(val, v_min, v_max, 0, s.width()-1), s.height()-1, color);
}

void UI_setup() {
	#if defined(TFT_MISO) && defined(TFT_MOSI) && defined(TFT_SCLK) && defined(TFT_CS) && defined(TFT_DC) && defined(TFT_RST)
		// custom initialization code for non m5 board
		lcd.init();
		lcd.setRotation(1); // use landscape layout
		lcd.fillScreen(TFT_BLACK);
		#ifdef TFT_BACKLIGHT
			// turn on the backlight if any
			pinMode(TFT_BACKLIGHT, OUTPUT);
			digitalWrite(TFT_BACKLIGHT, HIGH);
		#endif
	#endif // BOARD_M5CORE2

	#if defined(BOARD_M5STICKS3)
		lcd.setRotation(1); // 240x135 (landscape) on StickS3
		lcd.fillScreen(TFT_BLACK);
		createElement(rssi_text, 2, 112, 16, TFT_CYAN);
		createElement(rssi_bar, 2, 112, 14, TFT_CYAN);
		createElement(lq_text, 2, 112, 16, TFT_GREENYELLOW);
		createElement(lq_bar, 2, 112, 14, TFT_GREENYELLOW);
		for (uint8_t i=0; i < NUM_CHANNELS; i++) {
			createElement(*channel_texts[i], 2, STICKS3_CELL_W - 2, STICKS3_CH_TEXT_H, TFT_WHITE);
			createElement(*channel_bars[i], 1, STICKS3_CELL_W - 2, STICKS3_CH_BAR_H, TFT_LIGHTGREY);
		}
	#else
		createElement(rssi_text, 2, 100, 20, TFT_CYAN);
		createElement(rssi_bar, 2, 200, 20, TFT_CYAN);
		createElement(lq_text, 2, 100, 20, TFT_GREENYELLOW);
		createElement(lq_bar, 2, 200, 20, TFT_GREENYELLOW);
		createElement(tx_pwr_text, 2, 100, 20, TFT_DARKGREY);
		createElement(link_rate_text, 2, 100, 20, TFT_DARKGREY);
		createElement(rx_frame_indicator_bar, 4, 200, 50, TFT_ORANGE);
		rx_frame_indicator_bar.setScrollRect(0, 0, rx_frame_indicator_bar.width(), rx_frame_indicator_bar.height(), TFT_BLACK);
		for (uint8_t i=0; i < 10; i++) {
			createElement(*channel_texts[i], 1, 90, 10, TFT_WHITE);
			createElement(*channel_bars[i], 1, 210, 9, TFT_LIGHTGREY);
		}
		rx_frame_indicator_bar.print("RX WAIT");
		rx_frame_indicator_bar.setTextColor(TFT_BLACK);
		rx_frame_indicator_bar.pushSprite(110, 70);
	#endif
	UI_setRssi(0);
	UI_setLq(0);
	UI_setTxPwr(0);
	UI_setLinkRate(0);
}

void UI_setLq(int percent) {
	if (percent != current_lq) {
		current_lq = percent;
		clearSprite(lq_text);
		lq_text.printf("LQ %d", percent);
		#if defined(BOARD_M5STICKS3)
			lq_text.pushSprite(STICKS3_LQ_TEXT_X, STICKS3_LQ_TEXT_Y);
		#else
			lq_text.pushSprite(10, 40);
		#endif
		drawProgressBar(lq_bar, TFT_GREENYELLOW, percent, 0, 100);
		#if defined(BOARD_M5STICKS3)
			lq_bar.pushSprite(STICKS3_LQ_BAR_X, STICKS3_LQ_BAR_Y);
		#else
			lq_bar.pushSprite(110, 40);
		#endif
	}
}

void UI_setRssi(int dbm) {
	if (dbm != current_rssi) {
		current_rssi = dbm;
		clearSprite(rssi_text);
		if (dbm > 0) {
			rssi_text.printf("RS -%d", dbm);
			drawProgressBar(rssi_bar, TFT_CYAN, dbm, rssi_scale_min, rssi_scale_max);
		}
		else {
			rssi_text.printf("RX WAIT");
			drawProgressBar(rssi_bar, TFT_RED, 0, 0, 1);
		}
		#if defined(BOARD_M5STICKS3)
			rssi_text.pushSprite(STICKS3_RSSI_TEXT_X, STICKS3_RSSI_TEXT_Y);
			rssi_bar.pushSprite(STICKS3_RSSI_BAR_X, STICKS3_RSSI_BAR_Y);
		#else
			rssi_text.pushSprite(10, 10);
			rssi_bar.pushSprite(110, 10);
		#endif
	}
}

void UI_setTxPwr(int value) {
#if defined(BOARD_M5STICKS3)
	(void) value;
	return;
#else
	if (value != current_pw) {
		current_pw = value;
		clearSprite(tx_pwr_text);
		tx_pwr_text.printf("PW %d", value);
		tx_pwr_text.pushSprite(10, 70);
	}
#endif
}

void UI_setLinkRate(int hz) {
#if defined(BOARD_M5STICKS3)
	(void) hz;
	return;
#else
	clearSprite(link_rate_text);
	link_rate_text.printf("PR %d", hz);
	link_rate_text.pushSprite(10, 100);
#endif
}

void UI_setRssiScale(int dbm_min, int dbm_max) {
	rssi_scale_min = dbm_min;
	rssi_scale_max = dbm_max;
}

void UI_setChannels(uint32_t * channel_data) {
	for (uint8_t i=0; i < NUM_CHANNELS; i++) {
		int value =  channel_data[i];
		// update graphics only if the current channel value is different from previous
		if (value != current_rc_channels_data[i]) {
			current_rc_channels_data[i] = value;
			UISprite& text = *channel_texts[i];
			UISprite& bar = *channel_bars[i];
			#if defined(BOARD_M5STICKS3)
				int col = i % STICKS3_GRID_COLS;
				int row = i / STICKS3_GRID_COLS;
				int cell_x = STICKS3_GRID_X + col * STICKS3_CELL_W;
				int cell_y = STICKS3_GRID_Y + row * STICKS3_CELL_H;
			#endif
			clearSprite(text);
			#if defined(BOARD_M5STICKS3)
				text.printf("%d:%4d", i + 1, value);
				text.pushSprite(cell_x, cell_y);
			#else
				int v_offset = 140 + 10 * i;
				text.printf("CH%2d: %4d", i+1, value);
				text.pushSprite(10, v_offset);
			#endif

			clearSprite(bar);
			drawProgressBar(bar, TFT_LIGHTGREY, value, 172, 1810);
			#if defined(BOARD_M5STICKS3)
				bar.pushSprite(cell_x, cell_y + STICKS3_CH_TEXT_H);
			#else
				int v_offset = 140 + 10 * i;
				bar.pushSprite(110, v_offset);
			#endif
		}
	}
}

void UI_pushDataFrameIndication(uint8_t * results, int count) {
#if defined(BOARD_M5STICKS3)
	(void) results;
	(void) count;
	return;
#else
	for (int i=0; i < count; i++) {
		rx_frame_indicator_bar.scroll(1, 0);
		uint16_t color = TFT_DARKGREY;
		switch ((crsf_packet_result_e) results[i]) {
			case CRSF_RESULT_PACKET_OK:
				color = TFT_GREEN;
				break;
			case CRSF_RESULT_PACKET_TIMEOUT:
				color = TFT_RED;
				break;
			default:
				color = TFT_DARKGREY;
		}
		// draw vertical line
		rx_frame_indicator_bar.drawFastVLine(1, 0, rx_frame_indicator_bar.height(), color);
		// draw small vertical bars every 25 pixels
		if ((packet_counter + i) % 25 == 0) {
			rx_frame_indicator_bar.drawFastVLine(1, rx_frame_indicator_bar.height() - 5, rx_frame_indicator_bar.height(), TFT_WHITE);
		}
	}
	rx_frame_indicator_bar.drawRect(0, 0, rx_frame_indicator_bar.width(), rx_frame_indicator_bar.height(), TFT_DARKCYAN);
	rx_frame_indicator_bar.pushSprite(110, 70);
	packet_counter += count;
#endif
}

