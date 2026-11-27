/** @file quick_panel.h @brief Frosted RGB565 surface with touch targets for common actions. */
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ui_theme.h"

enum class QuickAction : uint8_t { None, Timer, Phone, Sound, Close };
enum class QuickPhone : uint8_t { AndroidOnly, Offline, Ready, Finding };

class QuickPanel {
	int16_t previous_height = 0;
	uint32_t previous_settings = 0;
	uint8_t previous_controls = 0;
	UiStatusBar previous_status;

public:
	/** @brief Draw the pull handle over the Eyes screen. */
	static void handle(Arduino_GFX *gfx);

	/** @brief Return the action at a released touch on the fully open panel. */
	static QuickAction hit(int16_t x, int16_t y, int16_t width, int16_t height);

	/** @brief Discard the painted height on close, page switch or display rotation. */
	void reset() { previous_height = 0; }

	/** @brief Number of panel rows painted over Eyes in the previous frame. */
	int16_t visible_height() const { return previous_height; }

	/** @brief Paint a full or partial panel. @return True when the canvas needs flushing. */
	bool draw(Arduino_GFX *gfx, uint16_t *pixels, bool flipped, int16_t height,
		QuickPhone phone, bool muted, bool timer_running, uint32_t settings_revision, const UiStatusBar &status);
};
