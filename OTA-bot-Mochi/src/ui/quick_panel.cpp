/** @file quick_panel.cpp @brief Full-screen quick controls with fixed eye-colored halos. */
// SPDX-License-Identifier: GPL-3.0-or-later
#include "quick_panel.h"
#include "quick_panel_gesture.h"
#include "i18n.h"
#include "ui_font.h"

namespace {
constexpr uint16_t Glass = 0x18E4;
constexpr int16_t RowHeight = 40, RowStep = 44, Margin = 12;
constexpr int Count = 3;

/** @brief Center the same row layout on both display aspect ratios. */
int16_t first_row(int16_t height) { return (height - ui_s(2 * RowStep + RowHeight)) / 2; }

/** @brief Draw two stationary, softly shaded capsules on the settled panel. */
void draw_halos(Arduino_GFX *gfx) {
	const int16_t x = gfx->width() / 2, y = gfx->height() / 2;
	const int16_t separation = ui_s(44);
	for (int level = 0; level < 12; ++level) {
		const int16_t w = ui_s(70 - 4 * level), h = ui_s(132 - 7 * level);
		const uint16_t ink = ui_mix(Glass, UI_ACCENT, (level + 1) * 4);
		gfx->fillRoundRect(x - separation - w / 2, y - h / 2, w, h, w / 2, ink);
		gfx->fillRoundRect(x + separation - w / 2, y - h / 2, w, h, w / 2, ink);
	}
}

/** @brief Draw a compact timer, phone or speaker symbol. */
void icon(Arduino_GFX *gfx, int index, int16_t x, int16_t y, uint16_t ink, bool muted) {
	const int16_t r = ui_s(8), t = ui_s(4);
	if (index == 0) {
		gfx->drawCircle(x, y, r, ink);
		gfx->drawLine(x, y - t, x, y, ink);
		gfx->drawLine(x, y, x + t, y + t / 2, ink);
	} else if (index == 1) {
		gfx->drawRoundRect(x - t, y - r, t * 2 + 1, r * 2 + 1, ui_s(2), ink);
		gfx->drawLine(x - r, y - t, x - r, y + t, ink);
		gfx->drawLine(x + r, y - t, x + r, y + t, ink);
	} else {
		ui_speaker_icon(gfx, x - ui_s(11), y - ui_s(6), ink, muted, ui_scale());
	}
}

/** @brief Shade a rounded row directly over the frosted glass. */
void row_surface(Arduino_GFX *gfx, uint16_t *pixels, bool flipped, int x, int y, int w, int h) {
	const int radius = ui_s(ui_font::Supported ? 14 : 12);
	if (!pixels) { gfx->fillRoundRect(x, y, w, h, radius, UI_CARD); return; }
	const int width = gfx->width(), height = gfx->height();
	for (int dy = y < 0 ? -y : 0; dy < h && y + dy < height; ++dy) {
		const int edge = dy < h - 1 - dy ? dy : h - 1 - dy;
		int inset = 0;
		if (edge < radius) {
			const int distance = radius - 1 - edge;
			while ((radius - inset) * (radius - inset) + distance * distance > radius * radius) { ++inset; }
		}
		const int span = w - 2 * inset;
		const int pos = (y + dy) * width + x + inset;
		auto *line = pixels + (flipped ? width * height - pos - span : pos);
		for (int col = 0; col < span; ++col) { line[col] = ui_mix(line[col], UI_CARD, 164); }
	}
}

/** @brief Use an accent outline when off and a solid accent with dark thumb when on. */
void mute_switch(Arduino_GFX *gfx, int16_t right, int16_t center_y, bool on) {
	const int16_t width = ui_s(36), radius = ui_s(9), inset = ui_s(2);
	const int16_t x = right - ui_s(46), y = center_y - radius;
	gfx->fillRoundRect(x, y, width, 2 * radius, radius, UI_ACCENT);
	if (!on) {
		gfx->fillRoundRect(x + inset, y + inset, width - 2 * inset, 2 * (radius - inset),
			radius - inset, UI_CARD);
	}
	const int16_t thumb = on ? x + width - radius : x + radius;
	gfx->fillCircle(thumb, center_y, ui_s(5), on ? UI_BG : UI_ACCENT);
}

/** @brief Draw the centered grip in the selected eye color. */
void draw_handle(Arduino_GFX *gfx, int16_t y) {
	const int16_t width = ui_s(36);
	gfx->fillRoundRect((gfx->width() - width) / 2, y, width, ui_s(4), ui_s(2), UI_ACCENT);
}

/** @brief Paint one action row, clipping its text and glyph during the pull animation. */
void row(Arduino_GFX *gfx, uint16_t *pixels, bool flipped, int index, int16_t y, Text title, const char *hint,
	bool enabled, bool muted) {
	const int16_t h = ui_s(RowHeight), x = ui_s(Margin), w = gfx->width() - 2 * x;
	if (y + h <= 0) { return; }
	row_surface(gfx, pixels, flipped, x, y, w, h);
	const int16_t mid = y + h / 2, label_x = x + ui_s(39);
	const uint16_t ink = enabled ? UI_TEXT : UI_MUTED, accent = enabled ? UI_ACCENT : UI_MUTED;
	if (mid >= ui_s(10)) { icon(gfx, index, x + ui_s(20), mid, accent, muted); }
	const int16_t text_y = mid - ui_font::height(1) / 2 - (hint ? ui_s(7) : 0);
	const int16_t reserve = ui_s(index == 2 ? 50 : 8);
	if (text_y >= 0) { ui_font::label(gfx, tr(title), label_x, text_y, x + w - reserve - label_x, 1, ink, UI_CARD); }
	if (hint && mid + ui_s(3) >= 0) {
		ui_font::label(gfx, hint, label_x, mid + ui_s(3), x + w - reserve - label_x, 1, ink, UI_CARD);
	}
	if (index == 2 && mid >= ui_s(9)) { mute_switch(gfx, x + w, mid, muted); }
}
}

void QuickPanel::handle(Arduino_GFX *gfx) {
	const int16_t width = ui_s(24);
	gfx->fillRoundRect((gfx->width() - width) / 2, ui_s(8), width, ui_s(2), ui_s(1),
		ui_mix(UI_BG, UI_ACCENT, 38));
}

QuickAction QuickPanel::hit(int16_t x, int16_t y, int16_t width, int16_t height) {
	if (x < 0 || y < 0 || x >= width || y >= height) { return QuickAction::None; }
	if (y >= height - 2 * ui_s(QuickPanelGesture::EdgeSize)) { return QuickAction::Close; }
	if (x < ui_s(Margin) || x >= width - ui_s(Margin)) { return QuickAction::None; }
	for (int i = 0; i < Count; ++i) {
		const int16_t top = first_row(height) + i * ui_s(RowStep);
		if (y >= top && y < top + ui_s(RowHeight)) { return QuickAction(i + 1); }
	}
	return QuickAction::None;
}

bool QuickPanel::draw(Arduino_GFX *gfx, uint16_t *pixels, bool flipped, int16_t height,
	QuickPhone phone, bool muted, bool timer_running, uint32_t settings_revision, const UiStatusBar &status) {
	const int16_t full = gfx->height();
	const uint8_t controls = uint8_t(phone) | (muted ? 4 : 0) | (timer_running ? 8 : 0);
	if (height == full && height == previous_height && controls == previous_controls &&
		settings_revision == previous_settings) {
		if (status == previous_status) { return false; }
		previous_status = status;
		ui_status_bar(gfx, status, Glass);
		return true;
	}
	previous_height = height;
	previous_controls = controls;
	previous_settings = settings_revision;
	gfx->fillRect(0, 0, gfx->width(), height, Glass);
	if (height == full) {
		draw_halos(gfx);
		previous_status = status;
		ui_status_bar(gfx, status, Glass);
	}
	else { pixels = nullptr; } // Cards use simple fills while the panel follows the finger.
	const int16_t shift = height - full, top = first_row(full) + shift;
	const int16_t title = top - ui_s(39);
	if (title >= 0) { ui_center(gfx, tr(Text::QuickPanel), title * 100 / ui_scale(), 2, UI_TEXT); }
	row(gfx, pixels, flipped, 0, top, timer_running ? Text::QuickTimerActive : Text::QuickTimer, nullptr, true, muted);
	const char *phone_hint = phone == QuickPhone::AndroidOnly ? tr(Text::QuickAndroidOnly) :
		phone == QuickPhone::Offline ? tr(Text::QuickPhoneOffline) : nullptr;
	row(gfx, pixels, flipped, 1, top + ui_s(RowStep), phone == QuickPhone::Finding ? Text::QuickStopFind : Text::QuickFind,
		phone_hint, !phone_hint, muted);
	row(gfx, pixels, flipped, 2, top + 2 * ui_s(RowStep), Text::QuickMute, nullptr, true, muted);
	const int16_t mid = gfx->width() / 2, handle_y = height - ui_s(12);
	if (handle_y >= ui_s(12)) {
		gfx->drawLine(mid - ui_s(5), handle_y - ui_s(6), mid, handle_y - ui_s(10), UI_ACCENT);
		gfx->drawLine(mid, handle_y - ui_s(10), mid + ui_s(5), handle_y - ui_s(6), UI_ACCENT);
		draw_handle(gfx, handle_y);
	}
	return true;
}
