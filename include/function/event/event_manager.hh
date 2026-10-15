/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef WBE_FILE_EVENT_MANAGER_HH
#define WBE_FILE_EVENT_MANAGER_HH

#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/event/event_registration.hh"
#include "utils/interface/i_singleton.hh"
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class EventManager
 * @brief Thread-safe publish/subscribe table that delivers events to callbacks by the event's exact C++ type.
 *
 * An event is any value; its type, compared through std::type_index after removing references and cv-qualifiers, is its
 * event id. subscribe<TEvent>() registers a callback for one event type and returns the EventRegistration that owns it;
 * destroying or resetting the registration unsubscribes. emit() calls every callback registered for the emitted value's
 * type, in subscription order, on the emitting thread. Base and derived types are different events.
 *
 * All methods may be called from any thread, including from inside a callback. emit() calls the callbacks that were
 * subscribed when it started: callbacks subscribed during an emission are not called by it, and callbacks unsubscribed
 * during it are skipped if they have not run yet. Once a registration is destroyed or reset, its callback is not running and
 * will not run again, except for invocations on the destroying thread that are still on its stack (destroying a
 * registration from inside its own callback is allowed). Callbacks can run concurrently when several threads emit, so they
 * must be thread-safe themselves.
 * An exception thrown by a callback propagates out of emit() and skips the remaining callbacks.
 *
 * The allocator must be thread-safe and outlive the manager. The manager is a singleton owned by EngineFunction, which the
 * Global creates; it is neither copyable nor movable, must outlive every emission and every registration it issued, and must
 * not be destroyed while another thread uses it. Destroying it while registrations remain is a debug assertion failure.
 */
class EventManager final : public ISingleton<EventManager> {
public:
    EventManager() = delete;
    /**
     * @brief Construct an empty event table.
     * @param p_allocator Thread-safe allocator for the table and its callbacks; it must outlive the manager.
     * @throws std::runtime_error If another EventManager exists or p_allocator is null.
     */
    explicit EventManager(HeapAllocatorDefault* p_allocator);
    /**
     * @brief Destroy the table. Every registration it issued must already be destroyed or reset.
     */
    virtual ~EventManager() override;
    EventManager(const EventManager&) = delete;
    EventManager(EventManager&&) noexcept = delete;
    EventManager& operator=(const EventManager&) = delete;
    EventManager& operator=(EventManager&&) noexcept = delete;

    /**
     * @brief Get the singleton instance.
     * @return The instance, or null when none exists.
     */
    static EventManager* get_singleton() {
        return singleton;
    }

    /**
     * @brief Get the event id of an event type.
     * @tparam TEvent The event type; references and cv-qualifiers are ignored.
     */
    template <typename TEvent>
    static std::type_index get_event_id() {
        return std::type_index(typeid(std::remove_cvref_t<TEvent>));
    }

    /**
     * @brief Register a callback for an event type.
     * @tparam TEvent The event type; references and cv-qualifiers are ignored.
     * @param p_callback Callable invoked as `p_callback(const TEvent&)` for every emitted TEvent. It is copied or moved into
     * the manager and may be invoked concurrently from several threads.
     * @return The registration that owns the subscription; the callback stays subscribed until it is destroyed or reset.
     */
    template <typename TEvent, typename TCallback>
        requires std::invocable<std::remove_cvref_t<TCallback>&, const std::remove_cvref_t<TEvent>&>
    [[nodiscard]] EventRegistration subscribe(TCallback&& p_callback) {
        using EventType = std::remove_cvref_t<TEvent>;
        return EventRegistration(this, subscribe_erased(get_event_id<EventType>(), [callback = std::forward<TCallback>(p_callback)](const void* p_event) mutable {
            callback(*static_cast<const EventType*>(p_event));
        }));
    }

    /**
     * @brief Call every callback subscribed to the event's type with the event.
     * @param p_event The event; its exact type, ignoring references and cv-qualifiers, selects the callbacks.
     * @throws Whatever a callback throws.
     */
    template <typename TEvent>
    void emit(const TEvent& p_event) {
        emit_erased(get_event_id<TEvent>(), &p_event);
    }

    /**
     * @brief Get the number of callbacks currently subscribed to an event type.
     */
    template <typename TEvent>
    size_t get_subscriber_count() const {
        return get_subscriber_count_erased(get_event_id<TEvent>());
    }

private:
    friend class EventRegistration;

    using ErasedCallback = std::function<void(const void*)>;

    struct Listener {
        Listener() = delete;
        explicit Listener(HeapAllocatorDefault* p_allocator) : executing_threads(p_allocator) {
        }
        ~Listener() = default;
        Listener(const Listener&) = delete;
        Listener(Listener&&) noexcept = delete;
        Listener& operator=(const Listener&) = delete;
        Listener& operator=(Listener&&) noexcept = delete;

        EventSubscriptionID id = 0;
        ErasedCallback callback;
        // Guards is_active and executing_threads.
        std::mutex state_mutex;
        // Signaled whenever an invocation finishes.
        std::condition_variable invocation_finished;
        // Cleared by unsubscribe() before it waits for running invocations.
        bool is_active = true;
        // One entry per running invocation, naming the thread it runs on.
        Vector<std::thread::id, HeapAllocatorDefault> executing_threads;
    };

    // Records one invocation in its listener for the guard's lifetime, unless the listener is already unsubscribed.
    class InvocationGuard;

    using ListenerRef = Ref<Listener, HeapAllocatorDefault>;
    using ListenerList = Vector<ListenerRef, HeapAllocatorDefault>;
    // Immutable once published; subscribe() and unsubscribe() replace the whole list, so emit() iterates a stable copy.
    using ListenerListRef = Ref<ListenerList, HeapAllocatorDefault>;

    EventSubscriptionID subscribe_erased(std::type_index p_event_id, ErasedCallback p_callback);
    // Removes a subscription and waits for its invocations on other threads; throws if the id is unknown.
    void unsubscribe(EventSubscriptionID p_subscription);
    void emit_erased(std::type_index p_event_id, const void* p_event) const;
    size_t get_subscriber_count_erased(std::type_index p_event_id) const;

    HeapAllocatorDefault* allocator;
    mutable std::shared_mutex table_mutex;
    HashMap<std::type_index, ListenerListRef, HeapAllocatorDefault> listeners_by_event;
    HashMap<EventSubscriptionID, std::type_index, HeapAllocatorDefault> event_by_subscription;
    std::atomic<EventSubscriptionID> next_subscription_id = 1;

    inline static EventManager* singleton = nullptr;
};

} // namespace WhiteBirdEngine

#endif
