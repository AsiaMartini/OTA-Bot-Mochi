/**
 * @file ui_theme.cpp
 * @brief Compact, high-contrast drawing helpers for the LCD_WIDTH x LCD_HEIGHT canvas.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui_theme.h"
#include "ui_font.h"
#include "../../pin_config.h"
#include "../power/backlight.h"
#include "../power/battery.h"
#include "../sound/sound.h"
#include <cstdio>

uint16_t UI_CARD = 0x1082;
uint16_t UI_LINE = 0x2945;
uint16_t UI_TEXT = 0xEF5D;
uint16_t UI_MUTED = 0x8C51;
uint16_t UI_ACCENT = 0xFFFF;
uint16_t UI_WARM = 0xFD20;

int16_t ui_scale() {
	int16_t sx = int16_t(LCD_WIDTH) * 100 / UI_DESIGN_W;
	int16_t sy = int16_t(LCD_HEIGHT) * 100 / UI_DESIGN_H;
	int16_t base = sx < sy ? sx : sy;
	return base < 100 ? 100 : (base > 200 ? 200 : base);
}

/**
 * @brief Minimal dark palette: neutral ink and surfaces, the eye color only as accent.
 *
 * Text stays near-white and secondary text neutral grey on every eye color, so
 * contrast no longer depends on the selected hue. Surfaces are neutral grey;
 * warnings use a fixed amber.
 * @param color Selected eye color (RGB565).
 */
void ui_theme_color(uint16_t color) {
	UI_ACCENT = color;
	UI_TEXT = 0xEF5D;	// RGB565 greys keep green at exactly twice red/blue: no green cast.
	UI_MUTED = 0x8C51;
	UI_WARM = color == 0xFD6C ? 0xF9D6 : 0xFD20;	// Amber eyes warn in pink.
	UI_CARD = 0x1082;	// Neutral surfaces: no accent tint, so no hue cast on large areas.
	UI_LINE = 0x2945;
}

void ui_box(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fill, bool outline) {
	int16_t r = ui_s(12) < h / 2 ? ui_s(12) : h / 2;
	gfx->fillRoundRect(x, y, w, h, r, fill);
	if (outline) { gfx->drawRoundRect(x, y, w, h, r, UI_LINE); }
}

void ui_button(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, const String &label, bool primary, bool enabled) {
	const uint16_t fill = primary && enabled ? UI_ACCENT : UI_CARD;
	ui_box(gfx, x, y, w, h, fill, !primary || !enabled);
	const int16_t room = w - ui_s(12);
	const String text = ui_font::fit(ui_ascii(label), room, 1);
	const int16_t width = ui_font::width(text, 1);
	ui_font::label(gfx, text, x + (w - width) / 2, y + (h - ui_font::height(1)) / 2, room, 1,
		!enabled ? UI_MUTED : (primary ? UI_BG : UI_TEXT), fill);
}

/**
 * @brief Convert text to safe ASCII, approximating common Latin accents.
 * @param text UTF-8 source.
 * @return ASCII text with unsupported glyphs represented by a question mark.
 */
String ui_ascii(const String &text) {
	String out;
	out.reserve(text.length());
	for (unsigned int i = 0; i < text.length(); ++i) {
		uint8_t c = text[i];
		if (c < 128) {
			out += (c == '\n' || c >= 32) ? char(c) : ' ';
			continue;
		}
		char replacement = '?';
		if (c == 0xC3 && i + 1 < text.length()) {
			uint8_t next = text[i + 1];
			if (next >= 0xA0 && next <= 0xA5) { replacement = 'a'; }
			if (next >= 0xA8 && next <= 0xAB) { replacement = 'e'; }
			if (next >= 0xAC && next <= 0xAF) { replacement = 'i'; }
			if (next >= 0xB2 && next <= 0xB6) { replacement = 'o'; }
			if (next >= 0xB9 && next <= 0xBC) { replacement = 'u'; }
		}
		out += replacement;
		while (i + 1 < text.length() && (uint8_t(text[i + 1]) & 0xC0) == 0x80) {
			++i;
		}
	}
	return out;
}

/**
 * @brief Draw a single ASCII line with ellipsis clipping.
 * @param gfx Canvas.
 * @param text Source.
 * @param x Left edge.
 * @param y Top edge.
 * @param width Available width.
 * @param size Font scale.
 * @param color Foreground.
 */
void ui_label(Arduino_GFX *gfx, const String &text, int16_t x, int16_t y,
	int16_t width, uint8_t size, uint16_t color) {
	if (ui_font::draw(gfx, text, x, y, width, size, color)) { return; }
	size = ui_text_size(size);
	if (size == 0 || width < 6 * size) { return; }
	String safe = ui_ascii(text);
	safe.replace('\n', ' ');
	unsigned int columns = width / (6 * size);
	if (safe.length() > columns) {
		safe = safe.substring(0, columns > 3 ? columns - 3 : 0);
		for (unsigned int i = 0; i < (columns < 3 ? columns : 3); ++i) { safe += '.'; }
	}
	gfx->setFont(nullptr);
	gfx->setTextWrap(false);
	gfx->setTextSize(size);
	gfx->setTextColor(color);
	gfx->setCursor(x, y);
	gfx->print(safe);
}

void ui_center(Arduino_GFX *gfx, const String &text, int16_t y, uint8_t size, uint16_t color) {
	const int16_t room = LCD_WIDTH - ui_s(48);
	const String clipped = ui_font::fit(ui_ascii(text), room, size);
	const int16_t width = ui_font::width(clipped, size);
	ui_label(gfx, clipped, (LCD_WIDTH - width) / 2, ui_s(y), room, size, color);
}

void ui_bar(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, int32_t permille, uint16_t color) {
	permille = permille < 0 ? 0 : (permille > 1000 ? 1000 : permille);
	gfx->fillRoundRect(x, y, w, h, h / 2, UI_LINE);
	int16_t filled = int16_t(int32_t(w) * permille / 1000);
	if (filled >= h) { gfx->fillRoundRect(x, y, filled, h, h / 2, color); }
}

void ui_status(Arduino_GFX *gfx, int16_t y, bool good, const String &text) {
	int16_t r = ui_s(3) < 2 ? 2 : ui_s(3);
	gfx->fillCircle(ui_s(32), ui_s(y) + 7 * ui_text_size(1) / 2, r, good ? UI_ACCENT : UI_LINE);
	ui_label(gfx, text, ui_s(44), ui_s(y), ui_s(168), 1, good ? UI_TEXT : UI_MUTED);
}

/** @brief Draw a content card. @param gfx Canvas. @param y Top (design space). @param height Height (design space). */
void ui_card(Arduino_GFX *gfx, int16_t y, int16_t height) {
	ui_box(gfx, ui_s(12), ui_s(y), LCD_WIDTH - 2 * ui_s(12), ui_s(height), UI_CARD, true);
}

/**
 * @brief Draw the compact speaker glyph, crossed out whenever audio cannot sound.
 * @param gfx Canvas.
 * @param x Left edge.
 * @param y Top edge.
 */
void ui_sound_badge(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t scale) {
	SoundStatus audio = sound_status();
	bool audible = audio == SoundStatus::Armed || audio == SoundStatus::Ready;
	ui_speaker_icon(gfx, x, y, audible ? UI_ACCENT : UI_MUTED, !audible, scale);
}

void ui_speaker_icon(Arduino_GFX *gfx, int16_t x, int16_t y, uint16_t ink, bool muted, int16_t scale) {
	const auto s = [scale](int value) { return int16_t(value * scale / 100); };
	const auto line = [&](int x1, int y1, int x2, int y2) {
		gfx->drawLine(x + s(x1), y + s(y1), x + s(x2), y + s(y2), ink);
	};
	gfx->fillRect(x, y + s(4), s(5), s(5), ink);
	gfx->fillTriangle(x + s(4), y + s(4), x + s(10), y, x + s(10), y + s(12), ink);
	gfx->fillTriangle(x + s(4), y + s(4), x + s(10), y + s(12), x + s(4), y + s(8), ink);
	if (!muted) {
		line(14, 3, 16, 5);
		line(16, 5, 16, 7);
		line(16, 7, 14, 9);
		line(17, 0, 20, 3);
		line(20, 3, 21, 6);
		line(21, 6, 20, 9);
		line(20, 9, 17, 12);
	} else {
		line(14, 2, 20, 10);
		line(20, 2, 14, 10);
	}
}

namespace {
/** @brief Fit three digits at full charge or the bolt and two digits while charging. */
int16_t status_battery_width() { return ui_s(ui_font::Supported ? 43 : 50); }

/** @brief Draw a compact charging bolt in a 5 x 8 design box. */
void charge_bolt(Arduino_GFX *gfx, int16_t x, int16_t y, uint16_t ink, int16_t scale) {
	const auto s = [scale](int value) { return int16_t(value * scale / 100); };
	gfx->fillTriangle(x + s(3), y, x, y + s(4), x + s(2), y + s(4), ink);
	gfx->fillTriangle(x + s(1), y + s(7), x + s(4), y + s(3), x + s(2), y + s(3), ink);
}

/** @brief Show a slashed phone outline without relying on a status color. */
void disconnected_phone(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t height) {
	gfx->drawRoundRect(x + ui_s(2), y, ui_s(10), height, ui_s(2), UI_MUTED);
	gfx->drawLine(x, y + height - 1, x + ui_s(12), y, UI_MUTED);
}
}

/**
 * @brief Draw a battery outline and inset bolt while the PMU reports charging.
 * @param gfx Canvas.
 * @param x Left edge; glyph spans x..x+7 and y..y+12.
 * @param y Top edge.
 */
void ui_charge_badge(Arduino_GFX *gfx, int16_t x, int16_t y) {
	uint16_t color = UI_ACCENT;
	gfx->fillRect(x + 2, y, 4, 2, color);
	gfx->drawRect(x, y + 2, 8, 11, color);
	charge_bolt(gfx, x + 2, y + 4, color, 100);
}

void ui_battery_indicator(Arduino_GFX *gfx, int16_t x, int16_t y, uint8_t charge, bool charging) {
	const int16_t w = status_battery_width(), h = ui_s(16), radius = ui_s(4);
	const int16_t center = y + h / 2, terminal = ui_s(5);
	const uint16_t rim = charging ? UI_CHARGING : UI_MUTED;
	gfx->fillRoundRect(x, y, w, h, radius, UI_CARD);
	gfx->drawRoundRect(x, y, w, h, radius, rim);
	gfx->fillRoundRect(x + w, center - terminal / 2, ui_s(3), terminal, ui_s(1), rim);
	if (charging && charge != 100) {
		const int16_t scale = ui_scale() * 125 / 100, height = 7 * scale / 100 + 1;
		charge_bolt(gfx, x + ui_s(7), center - height / 2, UI_CHARGING, scale);
	}
	const String label = charge == BATTERY_UNAVAILABLE ? String("--%") : String(charge) + "%";
	const int16_t right = x + w - ui_s(5);
	if (!ui_font::draw_inter_tabular(gfx, label, right, center, UI_TEXT, UI_CARD)) {
		const int16_t label_width = ui_font::width(label, 1);
		ui_label(gfx, label, right - label_width, center - ui_font::height(1) / 2, label_width, 1, UI_TEXT);
	}
}

namespace {
/** @brief Fit a running countdown in a compact, neutral card with no outline. */
void timer_pill(Arduino_GFX *gfx, uint32_t seconds, int16_t center) {
	char text[8];
	const unsigned minutes = unsigned(seconds / 60);
	if (minutes >= 100) { std::snprintf(text, sizeof(text), "%uh%02u", minutes / 60, minutes % 60); }
	else { std::snprintf(text, sizeof(text), "%02u:%02u", minutes, unsigned(seconds % 60)); }
	const int16_t w = ui_s(35), h = ui_s(16), mid = LCD_WIDTH / 2;
	gfx->fillRoundRect(mid - w / 2, center - h / 2, w, h, h / 2, UI_CARD);
	if (!ui_font::draw_inter_tabular(gfx, text, mid, center, UI_TEXT, UI_CARD, true)) {
		const int16_t text_w = ui_font::width(text, 1);
		ui_label(gfx, text, mid - text_w / 2, center - ui_font::height(1) / 2, text_w, 1, UI_TEXT);
	}
}
}

void ui_status_bar(Arduino_GFX *gfx, const UiStatusBar &status, uint16_t background) {
	const bool rounded = ui_font::Supported;
	const int16_t center = ui_s(16), icon_height = ui_s(14), icon_y = center - icon_height / 2;
	const int16_t speaker_scale = ((icon_height - 1) * 100 + 11) / 12;
	const int16_t inset = ui_s(rounded ? 13 : 5), gap = ui_s(rounded ? 2 : 3);
	const int16_t battery_x = LCD_WIDTH - inset - ui_s(3) - status_battery_width();
	const int16_t phone_x = battery_x - gap - ui_s(12) - 1;
	const int16_t speaker_x = phone_x - gap - (21 * speaker_scale / 100 + 1);
	gfx->fillRect(0, 0, LCD_WIDTH, ui_s(27), background);
	ui_label(gfx, "MOCHI", ui_s(16), center - ui_font::height(1) / 2 + (ui_font::active() ? 2 : 1),
		ui_s(72), 1, UI_MUTED);
	if (status.timer_seconds) { timer_pill(gfx, status.timer_seconds, center); }
	ui_speaker_icon(gfx, speaker_x, icon_y, status.audible ? UI_ACCENT : UI_MUTED, !status.audible, speaker_scale);
	if (status.phone_warning) { disconnected_phone(gfx, phone_x, icon_y, icon_height); }
	ui_battery_indicator(gfx, battery_x, center - ui_s(16) / 2, status.battery, status.charging);
}

/**
 * @brief Render header and pagination with the selected eye-color accent.
 * @param gfx Canvas.
 * @param title Screen title.
 * @param status Shared status indicators.
 * @param page Screen index.
 * @param settings_page True when the settings footer occupies the carousel indicator area.
 */
void ui_chrome(Arduino_GFX *gfx, const char *title, const UiStatusBar &status, uint8_t page, bool settings_page) {
	gfx->fillRect(0, 0, LCD_WIDTH, ui_s(52), UI_BG);
	ui_status_bar(gfx, status);
	gfx->fillRoundRect(ui_s(6), ui_s(30), ui_s(4), ui_s(16), ui_s(2), UI_ACCENT);
	ui_label(gfx, title, ui_s(16), ui_s(30), ui_s(208), 2, UI_TEXT);
	gfx->fillRect(0, ui_s(258), LCD_WIDTH, ui_s(22), UI_BG);
	if (settings_page && ui_font::Supported) { return; }
	// Pagination: a short accent pill marks the current page between small dots.
	const int16_t step = ui_s(12), pill = ui_s(16), dot = ui_s(2) < 2 ? 2 : ui_s(2);
	int16_t x = int16_t((LCD_WIDTH - (7 * step + pill)) / 2), y = ui_s(267);
	for (uint8_t i = 0; i < 8; ++i) {
		if (i == page) {
			gfx->fillRoundRect(x, y - dot - 1, pill, 2 * dot + 3, dot + 1, UI_ACCENT);
			x += pill + step - 2 * dot;
		} else {
			gfx->fillCircle(x + dot, y, dot, UI_MUTED);
			x += step;
		}
	}
}

/**
 * @brief Draw an empty state that explains how data arrives.
 * @param gfx Canvas.
 * @param title Primary message.
 * @param hint Next action.
 */
void ui_empty(Arduino_GFX *gfx, const char *title, const char *hint) {
	ui_card(gfx, 66, 172);
	gfx->drawRoundRect(ui_s(96), ui_s(92), ui_s(48), ui_s(38), ui_s(12), UI_ACCENT);
	gfx->fillCircle(ui_s(109), ui_s(110), 3, UI_TEXT);
	gfx->fillCircle(ui_s(131), ui_s(110), 3, UI_TEXT);
	ui_label(gfx, title, ui_s(28), ui_s(154), ui_s(184), 1, UI_TEXT);
	ui_label(gfx, hint, ui_s(28), ui_s(181), ui_s(184), 1, UI_MUTED);
}

/**
 * @brief Draw sun, clouds, rain, snow, or wind using bounded primitives.
 * @param gfx Canvas.
 * @param code Chronos condition.
 * @param x Center X.
 * @param y Center Y.
 */
void ui_weather_icon(Arduino_GFX *gfx, int code, int16_t x, int16_t y) {
	if (code < 0 || code > 7) {
		gfx->drawCircle(x, y, ui_s(18), UI_MUTED);
		ui_label(gfx, "?", x - ui_s(5), y - ui_s(8), ui_s(12), 2, UI_MUTED);
		return;
	}
	if (code == 0 || code == 1) {
		gfx->fillCircle(x, y - ui_s(5), ui_s(12), UI_WARM);
		for (int8_t dx : {-20, 20}) { gfx->drawFastHLine(x + ui_s(dx) - ui_s(3), y - ui_s(5), ui_s(6), UI_WARM); }
		gfx->drawFastVLine(x, y - ui_s(27), ui_s(6), UI_WARM);
	}
	if (code == 1) { return; }
	if (code == 5 || code == 6 || code == 7) {
		for (int8_t i = 0; i < 3; ++i) {
			gfx->drawFastHLine(x - ui_s(22) + i * ui_s(3), y - ui_s(12) + i * ui_s(10), ui_s(40 - i * 4), UI_MUTED);
		}
		return;
	}
	gfx->fillCircle(x - ui_s(10), y, ui_s(10), UI_TEXT);
	gfx->fillCircle(x + ui_s(2), y - ui_s(7), ui_s(14), UI_TEXT);
	gfx->fillRoundRect(x - ui_s(20), y, ui_s(42), ui_s(12), ui_s(6), UI_TEXT);
	if (code == 2 || code == 3) {
		for (int8_t i = -1; i <= 1; ++i) {
			if (code == 2) { gfx->fillCircle(x + i * ui_s(12), y + ui_s(22), 2, UI_ACCENT); }
			else { gfx->drawLine(x + i * ui_s(12), y + ui_s(17), x + i * ui_s(12) - ui_s(3), y + ui_s(24), UI_ACCENT); }
		}
	}
}
