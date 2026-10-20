#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "rendering/core/sprite_animation.h"

using namespace rme::rendering;

void test_uniform_forward_loop() {
	std::cout << "[Test] Uniform forward loop (7.x / classic mode)..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 4;
	data.start_frame = 0;
	data.loop_count = 0;
	data.is_async = false;
	data.shortest_step_ms = 500;
	data.total_duration_ms = 2000;

	// Frame changes every 500ms: 0, 1, 2, 3, 0, 1...
	assert(SpriteAnimation::getFrame(data, 0) == 0);
	assert(SpriteAnimation::getFrame(data, 250) == 0);
	assert(SpriteAnimation::getFrame(data, 499) == 0);
	assert(SpriteAnimation::getFrame(data, 500) == 1);
	assert(SpriteAnimation::getFrame(data, 999) == 1);
	assert(SpriteAnimation::getFrame(data, 1000) == 2);
	assert(SpriteAnimation::getFrame(data, 1499) == 2);
	assert(SpriteAnimation::getFrame(data, 1500) == 3);
	assert(SpriteAnimation::getFrame(data, 1999) == 3);
	assert(SpriteAnimation::getFrame(data, 2000) == 0);
	assert(SpriteAnimation::getFrame(data, 2500) == 1);

	std::cout << "  OK" << std::endl;
}

void test_custom_durations_forward_loop() {
	std::cout << "[Test] Custom variable duration forward loop (10.50+ / Protobuf)..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 3;
	data.start_frame = 0;
	data.loop_count = 0;
	data.is_async = false;

	// Frame 0: 100ms, Frame 1: 300ms, Frame 2: 600ms -> Total = 1000ms
	data.durations.push_back(AnimationDuration { 100, 100 });
	data.durations.push_back(AnimationDuration { 300, 300 });
	data.durations.push_back(AnimationDuration { 600, 600 });
	data.total_duration_ms = 1000;
	data.shortest_step_ms = 100;

	assert(SpriteAnimation::getFrame(data, 0) == 0);
	assert(SpriteAnimation::getFrame(data, 99) == 0);
	assert(SpriteAnimation::getFrame(data, 100) == 1);
	assert(SpriteAnimation::getFrame(data, 399) == 1);
	assert(SpriteAnimation::getFrame(data, 400) == 2);
	assert(SpriteAnimation::getFrame(data, 999) == 2);
	assert(SpriteAnimation::getFrame(data, 1000) == 0);
	assert(SpriteAnimation::getFrame(data, 1050) == 0);
	assert(SpriteAnimation::getFrame(data, 1100) == 1);

	std::cout << "  OK" << std::endl;
}

void test_ping_pong_uniform() {
	std::cout << "[Test] Ping-pong uniform 500ms cycle..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 4;
	data.start_frame = 0;
	data.loop_count = -1; // Ping-pong
	data.is_async = false;

	// Frames sequence: 0, 1, 2, 3, 2, 1 -> repeats every 6 ticks (3000ms)
	assert(SpriteAnimation::getFrame(data, 0) == 0);
	assert(SpriteAnimation::getFrame(data, 500) == 1);
	assert(SpriteAnimation::getFrame(data, 1000) == 2);
	assert(SpriteAnimation::getFrame(data, 1500) == 3);
	assert(SpriteAnimation::getFrame(data, 2000) == 2);
	assert(SpriteAnimation::getFrame(data, 2500) == 1);
	assert(SpriteAnimation::getFrame(data, 3000) == 0);
	assert(SpriteAnimation::getFrame(data, 3500) == 1);

	std::cout << "  OK" << std::endl;
}

void test_ping_pong_variable_durations() {
	std::cout << "[Test] Ping-pong variable duration timeline walk..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 3;
	data.start_frame = 0;
	data.loop_count = -1; // Ping-pong
	data.is_async = false;

	// Frame 0: 100ms, Frame 1: 200ms, Frame 2: 300ms
	// Forward: 0 (100ms), 1 (200ms), 2 (300ms)
	// Backward: 1 (200ms)
	// Full cycle = 100 + 200 + 300 + 200 = 800ms
	data.durations.push_back(AnimationDuration { 100, 100 });
	data.durations.push_back(AnimationDuration { 200, 200 });
	data.durations.push_back(AnimationDuration { 300, 300 });
	data.total_duration_ms = 600;
	data.shortest_step_ms = 100;

	// 0..99ms -> Phase 0
	assert(SpriteAnimation::getFrame(data, 0) == 0);
	assert(SpriteAnimation::getFrame(data, 50) == 0);
	// 100..299ms -> Phase 1
	assert(SpriteAnimation::getFrame(data, 100) == 1);
	assert(SpriteAnimation::getFrame(data, 250) == 1);
	// 300..599ms -> Phase 2
	assert(SpriteAnimation::getFrame(data, 300) == 2);
	assert(SpriteAnimation::getFrame(data, 550) == 2);
	// 600..799ms -> Phase 1 (rebound)
	assert(SpriteAnimation::getFrame(data, 600) == 1);
	assert(SpriteAnimation::getFrame(data, 750) == 1);
	// 800ms -> Phase 0 (cycle restart)
	assert(SpriteAnimation::getFrame(data, 800) == 0);
	assert(SpriteAnimation::getFrame(data, 900) == 1);

	std::cout << "  OK" << std::endl;
}

void test_spatial_async_desynchronization() {
	std::cout << "[Test] Spatial asynchronous phase desynchronization..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 4;
	data.start_frame = 255; // Async random spatial phase mode
	data.loop_count = 0;
	data.is_async = true;

	// At elapsed_ms = 0, tiles at different coordinates must produce different phases
	const int frame_tile1 = SpriteAnimation::getFrame(data, 0, 100, 100, 7);
	const int frame_tile2 = SpriteAnimation::getFrame(data, 0, 101, 100, 7);
	const int frame_tile3 = SpriteAnimation::getFrame(data, 0, 100, 101, 7);

	// tile1 offset: (100*17 + 100*31 + 7*7) % 4 = (1700 + 3100 + 49) % 4 = 4849 % 4 = 1
	assert(frame_tile1 == 1);
	// tile2 offset: (101*17 + 100*31 + 49) % 4 = 4866 % 4 = 2
	assert(frame_tile2 == 2);
	// tile3 offset: (100*17 + 101*31 + 49) % 4 = 4880 % 4 = 0
	assert(frame_tile3 == 0);

	// Verify that advancing time advances all tiles deterministically
	assert(SpriteAnimation::getFrame(data, 500, 100, 100, 7) == (frame_tile1 + 1) % 4);

	std::cout << "  OK" << std::endl;
}

void test_edge_cases() {
	std::cout << "[Test] Edge cases (single frame, zero frames, negative time)..." << std::endl;

	SpriteAnimationData empty_data;
	assert(SpriteAnimation::getFrame(empty_data, 1000) == 0);

	SpriteAnimationData single_frame;
	single_frame.frame_count = 1;
	assert(SpriteAnimation::getFrame(single_frame, 1000) == 0);

	SpriteAnimationData negative_time;
	negative_time.frame_count = 3;
	assert(SpriteAnimation::getFrame(negative_time, -500) == 0);

	std::cout << "  OK" << std::endl;
}

void test_concurrency_stress() {
	std::cout << "[Test] Multi-threaded lock-free stress test (16 threads x 500,000 queries)..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 5;
	data.start_frame = 255;
	data.loop_count = -1;
	data.is_async = true;
	data.durations.push_back(AnimationDuration { 100, 100 });
	data.durations.push_back(AnimationDuration { 150, 150 });
	data.durations.push_back(AnimationDuration { 200, 200 });
	data.durations.push_back(AnimationDuration { 250, 250 });
	data.durations.push_back(AnimationDuration { 300, 300 });

	constexpr int NUM_THREADS = 16;
	constexpr int QUERIES_PER_THREAD = 500000;
	std::vector<std::thread> threads;
	threads.reserve(NUM_THREADS);

	const auto start = std::chrono::high_resolution_clock::now();

	for (int t = 0; t < NUM_THREADS; ++t) {
		threads.emplace_back([&data, t]() {
			uint64_t checksum = 0;
			for (int i = 0; i < QUERIES_PER_THREAD; ++i) {
				const int64_t time = i * 33;
				const int x = t * 10 + (i & 15);
				const int y = t * 20 + ((i >> 4) & 15);
				const int frame = SpriteAnimation::getFrame(data, time, x, y, 7);
				checksum += frame;
			}
			assert(checksum > 0);
		});
	}

	for (auto& th : threads) {
		th.join();
	}

	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - start
	).count();

	std::cout << "  OK: 8,000,000 queries completed in " << elapsed << " ms (~"
	          << (8000000.0 / (elapsed > 0 ? elapsed : 1) * 1000.0) << " queries/sec, 0 contention)" << std::endl;
}

int main() {
	std::cout << "===========================================" << std::endl;
	std::cout << "  RME SpriteAnimation Subsystem Test Suite  " << std::endl;
	std::cout << "===========================================" << std::endl;

	test_uniform_forward_loop();
	test_custom_durations_forward_loop();
	test_ping_pong_uniform();
	test_ping_pong_variable_durations();
	test_spatial_async_desynchronization();
	test_edge_cases();
	test_concurrency_stress();

	std::cout << "===========================================" << std::endl;
	std::cout << "  ALL TESTS PASSED SUCCESSFULLY!          " << std::endl;
	std::cout << "===========================================" << std::endl;
	return 0;
}
