#ifndef __WBE_CLOCK_HH__
#define __WBE_CLOCK_HH__

#include <chrono>
namespace WhiteBirdEngine {

/**
 * @class Clock
 * @brief The global clock.
 *
 */
class Clock {
public:
    Clock() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    ~Clock() {}

    /**
     * @brief Get the duration from construction.
     *
     * @return The duration in seconds.
     */
    double get_duration() const {
        std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - start_time;
        return duration.count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;

};

}

#endif
