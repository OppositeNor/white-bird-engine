#ifndef __WBE_DEBUG_MUTEX_HH__
#define __WBE_DEBUG_MUTEX_HH__

#include "boost/thread/pthread/shared_mutex.hpp"
#include <atomic>
#include <thread>
namespace WhiteBirdEngine {

#ifdef _DEBUG

/**
 * @class DebugSharedMutex
 * @brief A debugable mutex.
 *
 */
class DebugSharedMutex {
public:
    DebugSharedMutex() = default;

    /**
     * @brief Unique lock.
     */
    void lock() {
        mtx.lock();
        unique_owner = std::this_thread::get_id();
    }

    /**
     * @brief Try unique lock.
     *
     * @return True if success, false otherwise.
     */
    bool try_lock() {
        if (mtx.try_lock()) {
            unique_owner = std::this_thread::get_id();
            return true;
        }
        return false;
    }

    /**
     * @brief Unlock unique lock.
     */
    void unlock() {
        unique_owner = std::thread::id{};
        mtx.unlock();
    }

    /**
     * @brief Shared lock.
     */
    void lock_shared() {
        mtx.lock_shared();
    }

    /**
     * @brief Try shared lock.
     *
     * @return True if success, false otherwise.
     */
    bool try_lock_shared() {
        if (mtx.try_lock_shared()) {
            return true;
        }
        return false;
    }

    /**
     * @brief Unlock shared lock.
     */
    void unlock_shared() {
        mtx.unlock_shared();
    }

    /**
     * @brief Is the unique lock locked in current thread.
     *
     * @return True if yes, false otherwise.
     */
    bool is_unique_locked_by_current_thread() const {
        return unique_owner == std::this_thread::get_id();
    }

private:
    boost::shared_mutex mtx;

    std::atomic<std::thread::id> unique_owner{};
};

#endif
}

#endif
