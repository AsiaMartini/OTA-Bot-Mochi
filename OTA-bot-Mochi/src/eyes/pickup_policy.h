/**
 * @file pickup_policy.h
 * @brief Conservative parked-to-motion-to-settled pickup heuristic, with no I/O.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>
#include <math.h>

class PickupPolicy {
	bool sampled = false, quiet = false, armed = false, moving = false, cooling = false;
	bool upward = false, braked = false;
	uint32_t previous_at = 0, quiet_at = 0, motion_at = 0, event_at = 0;
	float previous[3] = {}, parked[3] = {};

	/** @brief Squared distance in g, independent of mounting orientation. */
	static float distance(const float *a, const float *b) {
		float sum = 0;
		for (uint8_t i = 0; i < 3; ++i) { float d = a[i] - b[i]; sum += d * d; }
		return sum;
	}
public:
	/** @brief Clear history on initialization, calibration or orientation changes. */
	void reset() { *this = PickupPolicy{}; }
	/** @brief Require rest and a settled pose change or distinct upward lift and braking. */
	bool update(float x, float y, float z, uint32_t now, bool enabled) {
		float axes[] = {x, y, z};
		if (!enabled || !isfinite(x) || !isfinite(y) || !isfinite(z)) { reset(); return false; }
		if (!sampled || uint32_t(now - previous_at) > 150) {
			sampled = true; quiet = armed = moving = false;
			for (uint8_t i = 0; i < 3; ++i) { previous[i] = axes[i]; }
			previous_at = now; return false;
		}
		float magnitude = sqrtf(x * x + y * y + z * z);
		const bool near_gravity = fabsf(magnitude - 1) < 0.08f;
		const bool still = near_gravity && distance(axes, previous) < 0.0025f;
		for (uint8_t i = 0; i < 3; ++i) { previous[i] = axes[i]; }
		previous_at = now;
		if (cooling && uint32_t(now - event_at) < 6000) { quiet = armed = moving = false; return false; }
		if (magnitude < 0.5f || magnitude > 1.8f) { quiet = armed = moving = false; return false; }
		const float vertical = (x - parked[0]) * parked[0] + (y - parked[1]) * parked[1] +
			(z - parked[2]) * parked[2];
		const bool rising = vertical > 0.065f;
		if (armed && (rising || magnitude > 1.18f)) {
			moving = true; armed = quiet = false; motion_at = now;
			upward = rising;
			braked = false;
		}
		if (moving && uint32_t(now - motion_at) > 1500) { moving = quiet = false; }
		if (moving && upward && !braked && uint32_t(now - motion_at) >= 100 &&
			vertical < -0.055f) { braked = true; quiet = false; }
		if (armed && near_gravity && distance(axes, parked) > 0.01f) { armed = quiet = false; }
		if (!still) { quiet = false; return false; }
		if (!quiet) { quiet = true; quiet_at = now; }
		if (moving && uint32_t(now - quiet_at) >= 200 && (braked || distance(axes, parked) > 0.0225f)) {
			moving = armed = quiet = false; cooling = true; event_at = now; return true;
		}
		if (!moving && !armed && uint32_t(now - quiet_at) >= 1200) {
			armed = true;
			for (uint8_t i = 0; i < 3; ++i) { parked[i] = axes[i]; }
		}
		return false;
	}
};
