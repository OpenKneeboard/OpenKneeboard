// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "UniqueID.hpp"

#include <OpenKneeboard/KneeboardViewID.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <OpenKneeboard/bindline.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/task.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/weak_refs.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <source_location>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace OpenKneeboard {

class EventHandlerToken final : public UniqueIDBase<EventHandlerToken> {};
class EventHookToken final : public UniqueIDBase<EventHookToken> {};

using EventsTraceLoggingThreadActivity = TraceLoggingThreadActivity<
  gTraceProvider,
  std::to_underlying(TraceLoggingEventKeywords::Events)>;

template <class... Args>
class Event;

template <class... Args>
class EventHandler;

template <class TInvocable, class... TArgs>
concept drop_extra_back_invocable
  = std::invocable<TInvocable, TArgs...>
  || ((sizeof...(TArgs) > 0) && requires(TInvocable fn) {
       fn | drop_n_back<1>() | drop_extra_back();
       {
         fn | drop_n_back<1>() | drop_extra_back()
       } -> std::invocable<TArgs...>;
     });

template <class T>
concept must_await_task = requires(T v) {
  T::must_await_v;
  { std::bool_constant<T::must_await_v> {} } -> std::same_as<std::true_type>;
};

template <class T>
concept not_must_await_task = not must_await_task<T>;

template <class TInvocable, class... TArgs>
concept event_handler_invocable
  = drop_extra_back_invocable<TInvocable, TArgs...>
  && requires(TInvocable fn, TArgs... args) {
       { std::invoke(drop_extra_back(fn), args...) } -> not_must_await_task;
     };

static_assert(event_handler_invocable<std::function<void()>>);
static_assert(event_handler_invocable<std::function<fire_and_forget()>>);
static_assert(!event_handler_invocable<std::function<task<void>()>>);

template <class... Args>
class EventHandler final {
 public:
  using self_t = EventHandler<Args...>;
  using Callback = std::function<void(Args...)>;

  constexpr EventHandler() = default;
  constexpr EventHandler(const EventHandler&) = default;
  constexpr EventHandler(EventHandler&&) = default;
  EventHandler& operator=(const EventHandler&) = default;
  EventHandler& operator=(EventHandler&&) = default;

  constexpr operator bool() const noexcept {
    return static_cast<bool>(mImpl);
  }

  template <event_handler_invocable<Args...> T>
    requires(!std::same_as<self_t, std::decay_t<T>>)
  constexpr EventHandler(T&& fn) : mImpl(drop_extra_back(std::forward<T>(fn))) {
  }

  /// Convenience wrapper for `{this, &MyClass::myMethod}`
  template <class F, convertible_to_weak_ref Bind>
  constexpr EventHandler(Bind&& bind, F&& f)
    : EventHandler(
        bind_refs_front(std::forward<F>(f), std::forward<Bind>(bind))) {
  }

  /** Deleting so error mesages lead here :)
   *
   * Unsafe, as `this` may be deleted between enqueuing and executing.
   *
   * Use `std::enable_shared_from_this()` or similar.
   */
  template <class Bind, class F>
    requires(!convertible_to_weak_ref<Bind*>)
  constexpr EventHandler(Bind*, F&& f) = delete;

  void operator()(Args... args) const noexcept {
    mImpl(args...);
  }

 private:
  Callback mImpl;
};

class EventReceiver;

class EventConnectionBase;
template <class... Args>
class EventConnection;

class EventBase {
  friend class EventConnectionBase;
  friend class EventReceiver;

 public:
  enum class HookResult {
    ALLOW_PROPAGATION,
    STOP_PROPAGATION,
  };

  static void Shutdown(HANDLE event);

 protected:
  /** Event handlers are not invoked recursively to avoid deadlocks.
   *
   * If no calls are in progress in the current thread, this will immediately
   * invoke the specified handler, then invoke any other handlers that were
   * queued up while it was executing.
   *
   * If a call is in progress in the current thread, it will queue up the new
   * one, and return immediately.
   *
   * To similarly buffer events in a non-handler context, use the `EventDelay`
   * class.
   */
  static void InvokeOrEnqueue(std::function<void()>, std::source_location);

  virtual void RemoveHandler(EventHandlerToken) = 0;
};

/** Delay any event handling in the current thread for the lifetime of this
 * class.
 *
 * For example, you may want to use this after */
class EventDelay final {
 public:
  EventDelay(std::source_location location = std::source_location::current());
  ~EventDelay();

  EventDelay(const EventDelay&) = delete;
  EventDelay(EventDelay&&) = delete;
  auto operator=(const EventDelay&) = delete;
  auto operator=(EventDelay&&) = delete;

 private:
  ThreadGuard mThreadGuard;
  std::source_location mSourceLocation;
  EventsTraceLoggingThreadActivity mActivity;
};

class EventConnectionBase {
 public:
  EventHandlerToken mToken;
  virtual void Invalidate() = 0;
};

template <class... Args>
class EventConnection final
  : public EventConnectionBase,
    public std::enable_shared_from_this<EventConnection<Args...>> {
 private:
  EventConnection(EventHandler<Args...> handler, std::source_location location)
    : mHandler(handler), mSourceLocation(location) {
  }

 public:
  EventConnection() = delete;

  static std::shared_ptr<EventConnection<Args...>> Create(
    EventHandler<Args...> handler,
    std::source_location location) {
    return std::shared_ptr<EventConnection<Args...>>(
      new EventConnection(handler, location));
  }

  constexpr operator bool() const noexcept {
    // not bothering with the lock, as it's checked with lock in Call() and
    // Invalidate() anyway
    return static_cast<bool>(mHandler);
  }

  void Call(Args... args) {
    auto stayingAlive = this->shared_from_this();
    auto handler = mHandler;
    if (handler) {
      // In release builds, ignore but drop unhandled exceptions from
      // handlers. In debug builds, break (or crash)
      try {
        handler(args...);
      } catch (const std::exception& e) {
        dprint("Uncaught std::exception from event handler: {}", e.what());
        OPENKNEEBOARD_BREAK;
      } catch (const winrt::hresult_error& e) {
        dprint(
          L"Uncaught hresult error from event handler: {} - {}",
          e.code().value,
          std::wstring_view {e.message()});
        OPENKNEEBOARD_BREAK;
      } catch (...) {
        dprint("Uncaught unknown exception from event handler");
        OPENKNEEBOARD_BREAK;
      }
    }
  }

  virtual void Invalidate() override {
    mHandler = {};
  }

 private:
  EventHandler<Args...> mHandler;
  std::source_location mSourceLocation;
};

/** a 1:n event. */
template <class... Args>
class Event final : public EventBase {
  friend class EventReceiver;

 public:
  using Hook = std::function<HookResult(Args...)>;
  using Handler = EventHandler<Args...>;

  Event() {
    mImpl = std::shared_ptr<Impl>(new Impl());
  }
  ~Event();

  void Emit(
    Args... args,
    std::source_location location = std::source_location::current()) {
    mImpl->Emit(args..., location);
  }

  OpenKneeboard::fire_and_forget EnqueueForContext(
    auto context,
    Args... args,
    std::source_location location = std::source_location::current()) {
    auto weakImpl = std::weak_ptr(mImpl);
    co_await context;
    if (auto impl = weakImpl.lock()) {
      impl->Emit(args..., location);
    }
  }

  EventHookToken AddHook(Hook, EventHookToken token = {}) noexcept;
  void RemoveHook(EventHookToken) noexcept;

 protected:
  std::shared_ptr<EventConnectionBase> AddHandler(
    const EventHandler<Args...>&,
    std::source_location current);
  virtual void RemoveHandler(EventHandlerToken token) override;

 private:
  struct Impl {
    ~Impl();

    std::unordered_map<
      EventHandlerToken,
      std::shared_ptr<EventConnection<Args...>>>
      mReceivers;
    std::unordered_map<EventHookToken, Hook> mHooks;

    void Emit(
      Args... args,
      std::source_location location = std::source_location::current());
  };
  std::shared_ptr<Impl> mImpl;
};

class EventReceiver {
  friend class EventBase;
  template <class... Args>
  friend class Event;

 public:
  EventReceiver();
  /* You **MUST** call RemoveAllEventListeners() in your
   * subclass destructor - otherwise other threads may
   * invoke an event handler while your object is partially
   * destructed */
  virtual ~EventReceiver();

  EventReceiver(const EventReceiver&) = delete;
  EventReceiver& operator=(const EventReceiver&) = delete;

 protected:
  std::vector<std::shared_ptr<EventConnectionBase>> mSenders;

  // `std::type_identity_t` makes the compiler infer `Args` from the event,
  // then match the handler, instead of attempting to infer `Args from both.
  template <class... Args>
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    const std::type_identity_t<EventHandler<Args...>>& handler,
    std::source_location location = std::source_location::current());

  template <class... Args>
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    std::type_identity_t<Event<Args...>>& forwardAs,
    std::source_location location = std::source_location::current());

  template <class... Args>
    requires(sizeof...(Args) > 0)
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    Event<>& forwardAs,
    std::source_location location = std::source_location::current());

  void RemoveEventListener(EventHandlerToken);
  void RemoveAllEventListeners();
};

template <class... Args>
std::shared_ptr<EventConnectionBase> Event<Args...>::AddHandler(
  const EventHandler<Args...>& handler,
  std::source_location location) {
  auto connection = EventConnection<Args...>::Create(handler, location);
  auto token = connection->mToken;
  mImpl->mReceivers.emplace(token, connection);
  return std::move(connection);
}

template <class... Args>
void Event<Args...>::RemoveHandler(EventHandlerToken token) {
  std::shared_ptr<EventConnectionBase> receiver;
  if (!mImpl->mReceivers.contains(token)) {
    return;
  }
  receiver = mImpl->mReceivers.at(token);
  mImpl->mReceivers.erase(token);
  receiver->Invalidate();
}

template <class... Args>
void Event<Args...>::Impl::Emit(Args... args, std::source_location location) {
  EventsTraceLoggingThreadActivity activity;
  TraceLoggingWriteStart(
    activity,
    "Event::Emit()",
    OPENKNEEBOARD_TraceLoggingSourceLocation(location));
  //  Copy in case one is erased while we're running
  auto receivers = mReceivers;
  auto hooks = mHooks;

  for (const auto& [_, hook]: hooks) {
    if (hook(args...) == HookResult::STOP_PROPAGATION) {
      TraceLoggingWriteStop(
        activity,
        "Event::Emit()",
        TraceLoggingValue("Stopped by hook", "Result"));
      return;
    }
  }

  InvokeOrEnqueue(
    [=]() {
      for (const auto& [token, receiver]: receivers) {
        if (receiver) {
          receiver->Call(args...);
        }
      }
    },
    location);
  TraceLoggingWriteStop(
    activity, "Event::Emit()", TraceLoggingValue("Done", "Result"));
}

template <class... Args>
Event<Args...>::~Event() {
}

template <class... Args>
Event<Args...>::Impl::~Impl() {
  for (const auto& [token, receiver]: mReceivers) {
    receiver->Invalidate();
  }
}

template <class... Args>
EventHookToken Event<Args...>::AddHook(
  Hook hook,
  EventHookToken token) noexcept {
  mImpl->mHooks.insert_or_assign(token, hook);
  return token;
}

template <class... Args>
void Event<Args...>::RemoveHook(EventHookToken token) noexcept {
  auto it = mImpl->mHooks.find(token);
  if (it != mImpl->mHooks.end()) {
    mImpl->mHooks.erase(it);
  }
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::type_identity_t<EventHandler<Args...>>& handler,
  std::source_location location) {
  mSenders.push_back(event.AddHandler(handler, location));
  return mSenders.back()->mToken;
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  std::type_identity_t<Event<Args...>>& forwardTo,
  std::source_location location) {
  return AddEventListener(
    event,
    [location, weak = std::weak_ptr(forwardTo.mImpl)](Args... args) {
      if (auto forwardTo = weak.lock()) {
        forwardTo->Emit(args..., location);
      }
    },
    location);
}

template <class... Args>
  requires(sizeof...(Args) > 0)
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  Event<>& forwardTo,
  std::source_location location) {
  return AddEventListener(
    event,
    [location, weak = std::weak_ptr(forwardTo.mImpl)](Args...) {
      if (auto forwardTo = weak.lock()) {
        forwardTo->Emit(location);
      }
    },
    location);
}

}// namespace OpenKneeboard
