#pragma once
#include <algorithm>
#include <functional>
#include <mutex>
#include <vector>

#include "Axion/Common/Common.h"

AXION_NAMESPACE_BEGIN

namespace Event {

/**
 * @brief Base class for all window or renderer events.
 *
 * You can extend this with event types like `WindowResizeEvent`, `KeyEvent`, etc.
 */
struct Event {
    bool handled = false;
};

/**
 * @brief Generic thread-safe event dispatcher with RAII-safe subscriptions.
 */
template <typename EventType>
class EventDispatcher
{
public:
    using Callback = std::function<void(const EventType&)>;

    /**
     * @brief RAII subscription handle. Unsubscribes automatically on destruction.
     */
    class Subscription
    {
    public:
        Subscription(EventDispatcher* dispatcher, size_t id)
            : _dispatcher(dispatcher), _id(id) {}

        ~Subscription() { unsubscribe(); }

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept
            : _dispatcher(other._dispatcher), _id(other._id) {
            other._dispatcher = nullptr;
        }

        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                unsubscribe();
                _dispatcher = other._dispatcher;
                _id = other._id;
                other._dispatcher = nullptr;
            }
            return *this;
        }

        void unsubscribe() {
            if (_dispatcher) {
                _dispatcher->unsubscribe(_id);
                _dispatcher = nullptr;
            }
        }

    private:
        EventDispatcher* _dispatcher = nullptr;
        size_t _id = 0;
    };

    /**
     * @brief Subscribe to the event.
     * @return A unique_ptr to a Subscription that will automatically unsubscribe.
     */
    std::unique_ptr<Subscription> subscribe(Callback callback) {
        std::lock_guard<std::mutex> lock(_mutex);
        const size_t id = ++_nextId;
        _subscribers.emplace_back(id, std::move(callback));
        return std::make_unique<Subscription>(this, id);
    }

    /**
     * @brief Dispatch (broadcast) an event to all subscribers.
     */
    void dispatch(const EventType& event) {
        std::vector<Callback> copy;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            for (auto& [_, cb] : _subscribers)
                copy.push_back(cb);
        }

        for (auto& cb : copy) {
            cb(event);
            if constexpr (std::is_base_of_v<Event, EventType>)
                if (event.handled)
                    break;
        }
    }

private:
    void unsubscribe(size_t id) {
        std::lock_guard<std::mutex> lock(_mutex);
        _subscribers.erase(
            std::remove_if(
                _subscribers.begin(),
                _subscribers.end(),
                [id](const auto& p) { return p.first == id; }),
            _subscribers.end());
    }

    std::vector<std::pair<size_t, Callback>> _subscribers;
    size_t _nextId = 0;
    std::mutex _mutex;
};

} // namespace Event

AXION_NAMESPACE_END
