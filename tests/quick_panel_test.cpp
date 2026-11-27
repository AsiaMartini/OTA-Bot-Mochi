/** @file quick_panel_test.cpp @brief Native gesture, layout and translucent raster checks. */
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../OTA-bot-Mochi/src/power/battery.h"
#include "../OTA-bot-Mochi/src/power/charging.h"
#include "../OTA-bot-Mochi/src/settings/settings.h"
#include "../OTA-bot-Mochi/src/sound/sound.h"
#include "../OTA-bot-Mochi/src/ui/quick_panel.h"
#include "../OTA-bot-Mochi/src/ui/quick_panel_gesture.h"
#include <cassert>

static CompanionSettings preferences;
const CompanionSettings &settings_get() { return preferences; }
uint8_t battery_percent() { return 255; }
bool charging_active() { return false; }
SoundStatus sound_status() { return SoundStatus::Off; }

/** @brief Exercise edge-only opening, drag reversal, settled taps and an upward close. */
static void gestures() {
	QuickPanelGesture panel;
	const auto update = [&](bool down, int16_t x, int16_t y, uint32_t now) {
		return panel.sample(down, x, y, now, LCD_HEIGHT, ui_s(24));
	};
	assert(!update(true, 120, 70, 0) && !update(true, 120, 130, 30));
	assert(!update(false, 0, 0, 40));
	assert(!update(true, 120, 10, 50) && !update(true, 50, 12, 60));
	assert(!update(true, 120, 85, 75) && !update(false, 0, 0, 90));
	assert(!update(true, 120, 10, 100) && update(true, 121, 70, 110));
	assert(panel.height(110, LCD_HEIGHT) == 60);
	assert(update(true, 120, 10, 125) && update(false, 0, 0, 140));
	assert(panel.height(300, LCD_HEIGHT) == 0);
	assert(!update(true, 120, 10, 310) && update(true, 120, 90, 330));
	assert(update(false, 0, 0, 340));
	assert(panel.height(380, LCD_HEIGHT) > 80 && panel.height(380, LCD_HEIGHT) < LCD_HEIGHT);
	assert(update(true, 120, 150, 380) && !panel.takes_tap());
	assert(update(false, 0, 0, 550));
	assert(update(true, 120, 150, 560) && panel.takes_tap());
	assert(update(true, 120, 70, 600) && !panel.takes_tap());
	assert(panel.height(600, LCD_HEIGHT) == LCD_HEIGHT - 80);
	assert(update(false, 0, 0, 610) && panel.height(650, LCD_HEIGHT) < LCD_HEIGHT - 80);
	assert(update(true, 120, 150, 650) && !panel.takes_tap());
	assert(update(false, 0, 0, 800));
	assert(panel.height(800, LCD_HEIGHT) == 0);
}

/** @brief Exercise short flicks, stale velocity and lift-only motion from the wide grip band. */
static void flicks() {
	QuickPanelGesture panel;
	const int16_t edge = ui_s(QuickPanelGesture::EdgeSize), grip_y = LCD_HEIGHT - edge * 2 + 2;
	const int16_t flick_y = grip_y - ui_s(17);
	const auto update = [&](bool down, int16_t y, uint32_t now, bool lift = false) {
		return panel.sample(down, 8, y, now, LCD_HEIGHT, edge, lift);
	};
	const auto open = [&](uint32_t start) {
		assert(!update(true, 2, start));
		assert(update(true, ui_s(70), start + 20) && update(false, 0, start + 30));
		assert(panel.interactive(start + 200, LCD_HEIGHT));
	};
	open(0);
	assert(update(true, grip_y, 220) && update(true, flick_y, 250));
	assert(update(false, 0, 260) && panel.height(430, LCD_HEIGHT) == 0);
	open(500);
	assert(update(true, grip_y, 720) && update(true, flick_y, 750));
	assert(update(false, 0, 900) && panel.interactive(1070, LCD_HEIGHT));
	assert(update(true, grip_y, 1080) && update(true, flick_y, 1230));
	assert(update(false, 0, 1250) && panel.interactive(1420, LCD_HEIGHT));
	assert(update(true, grip_y, 1440) && update(false, flick_y, 1460, true));
	assert(panel.height(1640, LCD_HEIGHT) == 0);
	open(1700);
	panel.close(1930, LCD_HEIGHT);
	assert(panel.height(2100, LCD_HEIGHT) == 0);
}

/** @brief Check that hit targets match their rows and the first pull only shades its revealed area. */
static void layout() {
	Arduino_GFX gfx;
	QuickPanel panel;
	preferences.language = 1;
	ui_theme_color(0x07FF);
	const int16_t top = (LCD_HEIGHT - ui_s(128)) / 2;
	for (int i = 0; i < 3; ++i) {
		const auto action = QuickAction(i + 1);
		const int16_t y = top + i * ui_s(44);
		assert(QuickPanel::hit(ui_s(12), y, LCD_WIDTH, LCD_HEIGHT) == action);
		assert(QuickPanel::hit(LCD_WIDTH / 2, y + ui_s(40) - 1, LCD_WIDTH, LCD_HEIGHT) == action);
		assert(QuickPanel::hit(LCD_WIDTH / 2, y + ui_s(40), LCD_WIDTH, LCD_HEIGHT) == QuickAction::None);
	}
	assert(QuickPanel::hit(ui_s(12) - 1, top, LCD_WIDTH, LCD_HEIGHT) == QuickAction::None);
	const int grip = LCD_HEIGHT - 2 * ui_s(QuickPanelGesture::EdgeSize);
	assert(QuickPanel::hit(0, grip, LCD_WIDTH, LCD_HEIGHT) == QuickAction::Close);
	assert(QuickPanel::hit(LCD_WIDTH - 1, grip, LCD_WIDTH, LCD_HEIGHT) == QuickAction::Close);
	assert(QuickPanel::hit(0, grip - 1, LCD_WIDTH, LCD_HEIGHT) == QuickAction::None);
	gfx.fillRoundRect(LCD_WIDTH / 4, LCD_HEIGHT / 3, ui_s(40), ui_s(90), ui_s(19), UI_ACCENT);
	const auto sample = size_t(LCD_HEIGHT / 2) * LCD_WIDTH + LCD_WIDTH / 3;
	assert(gfx.pixels[sample] == UI_ACCENT);
	assert(panel.draw(&gfx, gfx.pixels.data(), false, ui_s(30), QuickPhone::Ready, false, false, 0, {}));
	assert(gfx.pixels[0] != UI_BG && gfx.pixels[sample] == UI_ACCENT);
	gfx.text.clear();
	assert(panel.draw(&gfx, gfx.pixels.data(), false, LCD_HEIGHT, QuickPhone::Ready, true, false, 0, {}));
	assert(gfx.pixels[sample] != UI_ACCENT && gfx.pixels[sample] != UI_BG);
	assert(std::find(gfx.text.begin(), gfx.text.end(), "COMANDI RAPIDI") != gfx.text.end());
	assert(std::find(gfx.text.begin(), gfx.text.end(), "Trova telefono") != gfx.text.end());
	assert(std::find(gfx.text.begin(), gfx.text.end(), "Timer 5 min") != gfx.text.end());
	assert(std::find(gfx.text.begin(), gfx.text.end(), "Silenzioso") != gfx.text.end());
	assert(!panel.draw(&gfx, gfx.pixels.data(), false, LCD_HEIGHT, QuickPhone::Ready, true, false, 0, {}));
	gfx.text.clear();
	assert(panel.draw(&gfx, gfx.pixels.data(), false, LCD_HEIGHT, QuickPhone::AndroidOnly, true, true, 0, {}));
	assert(std::find(gfx.text.begin(), gfx.text.end(), "Solo Android") != gfx.text.end());
	assert(std::find(gfx.text.begin(), gfx.text.end(), "Apri timer") != gfx.text.end());
}

/** @brief Confirm the thumb contrasts with the track in both states, even with white eyes. */
static void switch_contrast() {
	Arduino_GFX gfx;
	QuickPanel panel;
	const int left = LCD_WIDTH - ui_s(12) - ui_s(46);
	const int width = ui_s(36), radius = ui_s(9);
	const int y = (LCD_HEIGHT - ui_s(128)) / 2 + 2 * ui_s(44) + ui_s(40) / 2;
	const auto pixel = [&](int x) { return gfx.pixels[size_t(y) * LCD_WIDTH + x]; };
	ui_theme_color(0xFFFF);
	assert(panel.draw(&gfx, gfx.pixels.data(), false, LCD_HEIGHT, QuickPhone::Ready, true, false, 0, {}));
	assert(pixel(left + radius) == UI_ACCENT && pixel(left + width - radius) == UI_BG);
	ui_theme_color(0x07FF);
	assert(panel.draw(&gfx, gfx.pixels.data(), false, LCD_HEIGHT, QuickPhone::Ready, false, false, 1, {}));
	assert(pixel(left + radius) == UI_ACCENT && pixel(left + width - radius) != UI_ACCENT);
	const auto handle = size_t(LCD_HEIGHT - ui_s(12) + 1) * LCD_WIDTH + LCD_WIDTH / 2;
	assert(gfx.pixels[handle] == UI_ACCENT);
	gfx.fillScreen(UI_BG);
	QuickPanel::handle(&gfx);
	const auto home_handle = size_t(ui_s(8) + 1) * LCD_WIDTH + LCD_WIDTH / 2;
	assert(gfx.pixels[home_handle] == ui_mix(UI_BG, UI_ACCENT, 38));
}

/** @brief Check that both halos appear only on the settled panel and ignore the underlying screen. */
static void fixed_halos() {
	Arduino_GFX normal, different;
	QuickPanel panel;
	ui_theme_color(0x07FF);
	different.fillScreen(0xF800);
	const int16_t height = ui_s(32);
	assert(panel.draw(&normal, normal.pixels.data(), false, height, QuickPhone::Ready, false, false, 0, {}));
	const auto flat = normal.pixels[size_t(ui_s(8)) * LCD_WIDTH];
	assert(flat != UI_BG && normal.pixels[size_t(ui_s(8)) * LCD_WIDTH + LCD_WIDTH - 1] == flat);
	assert(panel.draw(&normal, normal.pixels.data(), false, LCD_HEIGHT, QuickPhone::Ready, false, false, 0, {}));
	panel.reset();
	assert(panel.draw(&different, different.pixels.data(), false, LCD_HEIGHT, QuickPhone::Ready, false, false, 0, {}));
	assert(normal.pixels == different.pixels);
	const size_t gap = size_t((LCD_HEIGHT - ui_s(128)) / 2 + ui_s(42)) * LCD_WIDTH;
	assert(normal.pixels[gap + LCD_WIDTH / 2] == flat);
	assert(normal.pixels[gap + LCD_WIDTH / 2 - ui_s(44)] != flat);
	assert(normal.pixels[gap + LCD_WIDTH / 2 + ui_s(44)] != flat);
}

/** @brief A timer update repaints the open panel's status row without touching its cards. */
static void status_updates() {
	Arduino_GFX gfx;
	QuickPanel panel;
	UiStatusBar status;
	status.timer_seconds = 222;
	status.battery = 72;
	status.charging = status.phone_warning = true;
	const auto draw = [&] {
		return panel.draw(&gfx, gfx.pixels.data(), false, LCD_HEIGHT, QuickPhone::Offline, false, true, 0, status);
	};
	assert(draw());
	const size_t center = size_t(ui_s(16) - ui_s(16) / 2) * LCD_WIDTH + LCD_WIDTH / 2;
	assert(gfx.pixels[center] == UI_CARD);
	const size_t body = size_t(LCD_HEIGHT / 2) * LCD_WIDTH;
	gfx.pixels[body] = 0xF800;
	--status.timer_seconds;
	assert(draw() && gfx.pixels[body] == 0xF800);
	assert(!draw());
	status.timer_seconds = 0;
	assert(draw() && gfx.pixels[center] != UI_CARD);
}

int main() {
	gestures();
	flicks();
	layout();
	switch_contrast();
	fixed_halos();
	status_updates();
}
