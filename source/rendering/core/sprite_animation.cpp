//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/sprite_animation.h"
#include <algorithm>
#include <cstdlib>

namespace rme::rendering {

int SpriteAnimation::getFrame(
	const SpriteAnimationData& data,
	int64_t elapsed_ms,
	int tile_x,
	int tile_y,
	int tile_z
) noexcept {
	const int frames = std::max<int>(1, static_cast<int>(data.frame_count));
	if (frames <= 1) {
		return 0;
	}

	if (elapsed_ms < 0) {
		elapsed_ms = 0;
	}

	int spatial_phase = 0;
	if (data.is_async) {
		spatial_phase = std::abs(tile_x * 17 + tile_y * 31 + tile_z * 7);
	}

	int start_phase = 0;
	if (data.start_frame > 0 && data.start_frame < 255) {
		start_phase = static_cast<int>(data.start_frame) % frames;
	}

	const int phase_offset = spatial_phase + start_phase;

	// Ping-pong mode (loop_count == -1)
	if (data.loop_count == -1) {
		if (data.durations.empty()) {
			const int tick = static_cast<int>(elapsed_ms / 500);
			return getPingPongPhase(tick + phase_offset, frames);
		}

		const int num_phases = static_cast<int>(data.durations.size());
		if (num_phases == 0) {
			return phase_offset % frames;
		}

		constexpr int MAX_PHASES = 32;
		const int effective_phases = std::min<int>(num_phases, MAX_PHASES);

		// Stack-allocated phase durations (zero heap churn)
		int phase_durs[MAX_PHASES];
		int cycle_length = 0;
		for (int i = 0; i < effective_phases; ++i) {
			phase_durs[i] = static_cast<int>(data.durations[i].getDuration());
			cycle_length += phase_durs[i];
		}

		// Ping-pong timeline: [0, 1, ..., N-1, N-2, ..., 1] without duplicate ends
		constexpr int MAX_TIMELINE = MAX_PHASES * 2 - 2;
		int timeline[MAX_TIMELINE];
		int timeline_len = 0;
		for (int i = 0; i < effective_phases; ++i) {
			timeline[timeline_len++] = i;
		}
		for (int i = effective_phases - 2; i >= 1; --i) {
			timeline[timeline_len++] = i;
			cycle_length += phase_durs[i];
		}

		if (cycle_length <= 0) {
			return phase_offset % frames;
		}

		int start_offset = 0;
		const int initial_phase = phase_offset % effective_phases;
		for (int i = 0; i < initial_phase; ++i) {
			start_offset += phase_durs[i];
		}

		int64_t total_elapsed = elapsed_ms + start_offset;
		if (total_elapsed < 0) {
			total_elapsed = 0;
		}
		int elapsed = static_cast<int>(total_elapsed % cycle_length);

		for (int i = 0; i < timeline_len; ++i) {
			const int dur = phase_durs[timeline[i]];
			if (elapsed < dur) {
				return timeline[i] % frames;
			}
			elapsed -= dur;
		}
		return 0;
	}

	// Forward loop
	if (data.durations.empty()) {
		const int tick = static_cast<int>(elapsed_ms / 500);
		return (tick + phase_offset) % frames;
	}

	const int total = static_cast<int>(data.total_duration_ms);
	if (total <= 0) {
		return phase_offset % frames;
	}

	int64_t total_elapsed = elapsed_ms;
	int elapsed = static_cast<int>(total_elapsed % total);

	for (size_t phase = 0; phase < data.durations.size(); ++phase) {
		const int dur = static_cast<int>(data.durations[phase].getDuration());
		if (elapsed < dur) {
			return (static_cast<int>(phase) + phase_offset) % frames;
		}
		elapsed -= dur;
	}

	return phase_offset % frames;
}

} // namespace rme::rendering
