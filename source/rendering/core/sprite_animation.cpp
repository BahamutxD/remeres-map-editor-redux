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

	const int tile_offset = data.is_async ? std::abs(tile_x * 17 + tile_y * 31 + tile_z * 7) : 0;

	int start_phase = 0;
	if ((data.start_frame == 255 || data.start_frame == -1) && data.is_async) {
		start_phase = tile_offset % frames;
	} else if (data.start_frame > 0) {
		start_phase = static_cast<int>(data.start_frame) % frames;
	}

	// Ping-pong mode (loop_count == -1)
	if (data.loop_count == -1) {
		if (data.durations.empty()) {
			const int tick = static_cast<int>(elapsed_ms / 500);
			const int phase_shift = ((data.start_frame == 255 || data.start_frame == -1) && data.is_async) ? start_phase : (tile_offset + start_phase);
			return getPingPongPhase(tick + phase_shift, frames);
		}

		const int num_phases = static_cast<int>(data.durations.size());
		if (num_phases == 0) {
			return start_phase;
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
			return start_phase;
		}

		int start_offset = 0;
		for (int i = 0; i < start_phase && i < effective_phases; ++i) {
			start_offset += phase_durs[i];
		}

		int64_t total_elapsed = elapsed_ms + static_cast<int64_t>(tile_offset) * 500 + start_offset;
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
		const int phase_shift = ((data.start_frame == 255 || data.start_frame == -1) && data.is_async) ? start_phase : (tile_offset + start_phase);
		return (tick + phase_shift) % frames;
	}

	const int total = static_cast<int>(data.total_duration_ms);
	if (total <= 0) {
		return start_phase;
	}

	int64_t total_elapsed = elapsed_ms + static_cast<int64_t>(tile_offset) * 500;
	if (total_elapsed < 0) {
		total_elapsed = 0;
	}
	int elapsed = static_cast<int>(total_elapsed % total);

	for (size_t phase = 0; phase < data.durations.size(); ++phase) {
		const int dur = static_cast<int>(data.durations[phase].getDuration());
		if (elapsed < dur) {
			return (static_cast<int>(phase) + start_phase) % frames;
		}
		elapsed -= dur;
	}

	return start_phase;
}

} // namespace rme::rendering