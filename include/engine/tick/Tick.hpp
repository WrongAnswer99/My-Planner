#pragma once

#include <chrono>
#include <cstdint>

// Converts elapsed real time into fixed-rate game ticks. Rendering may run at
// any frame rate; every update driven by this manager advances exactly one tick.
class TickManager {
public:
	using TickCount = std::uint64_t;
	using Clock = std::chrono::steady_clock;

	static constexpr TickCount ticksPerSecond = 60;
	static constexpr std::chrono::nanoseconds tickDuration =
		std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)) /
		ticksPerSecond;
	static_assert(tickDuration.count() > 0, "Tick duration must be greater than zero");
	TickManager() = default;
	TickManager(const TickManager&) = delete;
	TickManager& operator=(const TickManager&) = delete;

	// Returns how many fixed updates must run to catch up with elapsed time.
	// The sub-tick remainder is retained for the next call.
	[[nodiscard]] TickCount getCurrentTick() {
		const Clock::time_point now = Clock::now();
		accumulatedTime += std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime);
		lastTime = now;

		const auto elapsedTicks = accumulatedTime / tickDuration;
		accumulatedTime -= tickDuration * elapsedTicks;
		return static_cast<TickCount>(elapsedTicks);
	}

	// Discards pending elapsed time, for example after loading or unpausing.
	void reset() {
		lastTime = Clock::now();
		accumulatedTime = std::chrono::nanoseconds::zero();
	}

private:
	Clock::time_point lastTime = Clock::now();
	std::chrono::nanoseconds accumulatedTime = std::chrono::nanoseconds::zero();
};
