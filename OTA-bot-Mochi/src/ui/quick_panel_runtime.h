/** @file quick_panel_runtime.h @brief Quick controls shared by the firmware and SDL preview. */
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "quick_panel.h"
#include "screens.h"
#include "view_state.h"
#include "../companion/companion_service.h"
#include "../companion/companion_state.h"
#include "../eyes/eyes_api.h"
#include "../settings/settings.h"

namespace quick_controls {
/** @brief Resolve the shortcut against the active phone backend. */
inline QuickPhone phone(bool ios) {
	if (ios) { return QuickPhone::AndroidOnly; }
	if (!companion_snapshot().connected) { return QuickPhone::Offline; }
	return companion_finding() ? QuickPhone::Finding : QuickPhone::Ready;
}

/** @brief Start five minutes or preserve the running timer for display. */
inline void start_timer(uint32_t now) {
	auto &timer = view_timer();
	timer.tick(now);
	if (timer.running) { return; }
	timer.mode = DeskTimer::FIVE_MINUTES;
	timer.reset(now);
	timer.toggle(now);
}

/** @brief Queue the existing start or stop command for a connected Android phone. */
inline void find_phone(bool ios) {
	const auto state = phone(ios);
	if (state == QuickPhone::Ready) { companion_request(CompanionAction::FindPhone); }
	if (state == QuickPhone::Finding) { companion_request(CompanionAction::StopFind); }
}

/** @brief Paint the home or its modal controls, restoring the Eyes dirty regions on close. */
inline bool draw_home(QuickPanel &panel, Arduino_GFX *gfx, uint16_t *pixels, uint32_t now,
	bool invalidated, int16_t height, bool flipped, bool ios) {
	const bool uncovering = height < panel.visible_height();
	const bool opening = height && !panel.visible_height();
	if (!height || (height < gfx->height() && (uncovering || opening || invalidated))) {
		if (invalidated || uncovering || opening) { eyes_state().dirtyInit = false; }
		eyes_update(now);
	}
	if (!height) {
		panel.reset();
		QuickPanel::handle(gfx);
		return true;
	}
	return panel.draw(gfx, pixels, flipped, height, phone(ios), !settings_get().sound,
		view_timer().running, settings_revision(), screens_status_bar());
}
}
