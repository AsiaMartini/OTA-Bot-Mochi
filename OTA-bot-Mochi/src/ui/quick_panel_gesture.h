/** @file quick_panel_gesture.h @brief Modal pull-down gesture and bounded panel animation. */
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

class QuickPanelGesture {
	int16_t start_x = 0, start_y = 0, offset = 0, from = 0;
	int16_t last_y = 0;
	uint32_t started = 0, last_motion = 0, flick_at = 0;
	bool down = false, eligible = false, dragging = false, opened = false;
	bool owns_contact = false, may_tap = false, near_grip = false, flicking = false;
	static constexpr int32_t Duration = 160;

	/** @brief Follow the drag and remember a recent decisive upward movement. */
	void drag(int16_t x, int16_t y, uint32_t now, int16_t full, int16_t edge) {
		const int16_t dx = x > start_x ? x - start_x : start_x - x;
		const int16_t dy = opened ? start_y - y : y - start_y;
		if (y == last_y && now - last_motion > 45) { last_motion = now; }
		if (!dragging) {
			if (dx >= edge || dy <= -edge / 2) { eligible = false; return; }
			if (dy < edge / (near_grip ? 3 : 2) || dy < dx * (near_grip ? 1 : 2)) { return; }
			dragging = true;
		}
		if (opened && y != last_y) {
			const int16_t rise = last_y - y;
			const uint32_t elapsed = now - last_motion;
			if (rise < 0) { flicking = false; }
			else if (rise >= edge / 6 && elapsed <= 120 &&
				uint32_t(rise) * 1000 >= uint32_t(edge) * 12 * elapsed) {
				flicking = true;
				flick_at = now;
			}
			last_y = y;
			last_motion = now;
		}
		const int16_t position = opened ? full - dy : dy;
		offset = position < 0 ? 0 : position > full ? full : position;
	}

public:
	static constexpr int16_t EdgeSize = 24; /**< Opening strip in design pixels. */

	/** @brief Clear panel state when the underlying page goes away. */
	void reset() { *this = QuickPanelGesture{}; }

	/** @brief Visible height in pixels, following the finger during a drag. */
	int16_t height(uint32_t now, int16_t full) const {
		if (dragging) { return offset; }
		const int16_t target = opened ? full : 0;
		const uint32_t age = now - started;
		if (age >= uint32_t(Duration) || target == from) { return target; }
		const int32_t left = Duration - age;
		return int16_t(target + (from - target) * left * left / (Duration * Duration));
	}

	/** @brief True when the open panel can accept released taps. */
	bool interactive(uint32_t now, int16_t full) const {
		return opened && !dragging && height(now, full) == full;
	}

	/** @brief True if this contact started on the settled panel and did not become a drag. */
	bool takes_tap() const { return down && may_tap && !dragging; }

	/** @brief Animate closed after a tap on the lower edge. */
	void close(uint32_t now, int16_t full) {
		cancel_contact(now, full);
		opened = false;
	}

	/** @brief Return a cancelled drag to its previous panel state. */
	void cancel_contact(uint32_t now, int16_t full) {
		from = height(now, full);
		started = now;
		down = eligible = dragging = owns_contact = false;
	}

	/** @brief Track a contact and report whether its events belong to the panel. */
	bool sample(bool touching, int16_t x, int16_t y, uint32_t now, int16_t full, int16_t edge,
		bool lift_position = false) {
		const int16_t visible = height(now, full);
		const bool modal = opened || visible != 0 || dragging;
		if (!touching) {
			if (lift_position && down && eligible && opened) { drag(x, y, now, full, edge); }
			const bool consumed = modal || owns_contact;
			if (dragging) {
				const int16_t travel = opened ? full - offset : offset;
				const bool flick = opened && flicking && now - flick_at <= 90 && travel >= edge / 2;
				if (flick || travel >= (near_grip ? edge : edge * 3 / 2)) { opened = !opened; }
				from = offset;
				started = now;
			}
			down = dragging = eligible = owns_contact = false;
			return consumed;
		}
		if (!down) {
			down = true;
			start_x = x;
			start_y = last_y = y;
			last_motion = now;
			owns_contact = modal;
			may_tap = interactive(now, full);
			near_grip = may_tap && y >= full - edge * 2;
			flicking = false;
			eligible = may_tap || (!modal && y < edge);
		}
		if (eligible) { drag(x, y, now, full, edge); }
		return modal || owns_contact || dragging;
	}
};
