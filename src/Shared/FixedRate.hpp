#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#endif

namespace Timing {

    /**
     * Temporarily requests 1 ms timer resolution on Windows.
     *
     * Windows may otherwise wake timed waits at a coarse granularity, causing
     * visible jitter in fixed-rate loops. Other platforms use a no-op
     * implementation. The previous resolution is restored on destruction.
     */
    class CScopedTimerResolution {
      public:
        CScopedTimerResolution() noexcept
#ifdef _WIN32
            :
            mActive(timeBeginPeriod(1) == TIMERR_NOERROR)
#endif
        {
        }

        ~CScopedTimerResolution() {
#ifdef _WIN32
            if (mActive)
                timeEndPeriod(1);
#endif
        }

        CScopedTimerResolution(const CScopedTimerResolution&)            = delete;
        CScopedTimerResolution& operator=(const CScopedTimerResolution&) = delete;

      private:
#ifdef _WIN32
        bool mActive = false;
#endif
    };

    /**
     * Paces a loop against absolute deadlines measured by a monotonic clock.
     *
     * Unlike a relative sleep after each iteration, this does not add the
     * iteration's work time to the interval or accumulate timing drift. If the
     * loop falls more than one interval behind, missed deadlines are skipped
     * instead of running repeated catch-up iterations.
     */
    class CFixedRate {
      public:
        using Clock = std::chrono::steady_clock;

        explicit CFixedRate(Clock::duration interval) :
            mInterval(interval), mNextDeadline(Clock::now() + interval) {}

        /**
         * Waits for the next deadline or until stopRequested becomes true.
         *
         * @return true when the next iteration should run, false when stopping.
         */
        template <typename TPredicate>
        bool wait(std::condition_variable& condition, std::unique_lock<std::mutex>& lock,
                  TPredicate stopRequested) {
            if (condition.wait_until(lock, mNextDeadline, stopRequested))
                return false;

            const auto now = Clock::now();
            mNextDeadline += mInterval;
            if (mNextDeadline <= now)
                mNextDeadline = now + mInterval;
            return true;
        }

      private:
        Clock::duration   mInterval;
        Clock::time_point mNextDeadline;
    };

} // namespace Timing
