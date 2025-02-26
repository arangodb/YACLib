#pragma once

#include <yaclib/algo/detail/base_core.hpp>
#include <yaclib/exe/executor.hpp>
#include <yaclib/util/detail/default_event.hpp>
#include <yaclib/util/detail/mutex_event.hpp>
#include <yaclib/util/helper.hpp>
#include <yaclib/util/ref.hpp>

#if YACLIB_CORO != 0
#  include <yaclib/coro/coro.hpp>
#endif

namespace yaclib {

class OneShotEventAwait;
class OneShotEventAwaitOn;

/**
 * TODO
 */
class OneShotEvent {
 public:
  OneShotEvent() noexcept;

  /**
   * TODO
   */
  bool TryAdd(Job* job) noexcept;

  /**
   * TODO
   */
  bool Ready() noexcept;

  /**
   * TODO
   */
  void Wait() noexcept;

  /**
   * TODO
   */
  template <typename Rep, typename Period>
  YACLIB_INLINE bool WaitFor(const std::chrono::duration<Rep, Period>& timeout_duration) {
    return TimedWait(timeout_duration);
  }

  /**
   * TODO
   */
  template <typename Clock, typename Duration>
  YACLIB_INLINE bool WaitUntil(const std::chrono::time_point<Clock, Duration>& timeout_time) {
    return TimedWait(timeout_time);
  }

#if YACLIB_CORO != 0
  /**
   * TODO
   */
  OneShotEventAwait operator co_await() noexcept;

  /**
   * TODO
   */
  OneShotEventAwait Await() noexcept;

  /**
   * TODO
   */
  OneShotEventAwaitOn AwaitOn(IExecutor& executor) noexcept;
#endif

  /**
   * TODO
   */
  void Set() noexcept;

  /**
   * TODO
   */
  void Reset() noexcept;

 private:
  template <typename Timeout>
  bool TimedWait(const Timeout& timeout) {
    struct OneShotTimedEventWaiter : Job, detail::MutexEvent {
      void Call() noexcept final {
        Set();  // under condition? Only if ref count != 1
        DecRef();
      }
    };
    auto waiter = MakeShared<OneShotTimedEventWaiter>(2);
    if (TryAdd(waiter.Get())) {
      auto token = waiter->Make();
      return waiter->Wait(token, timeout);
    } else {
      delete waiter.Release();
      return true;
    }
  }

  static constexpr std::uint64_t kEmpty = 0;
  static constexpr std::uint64_t kAllDone = 1;

  yaclib_std::atomic_uint64_t _head;
};

#if YACLIB_CORO != 0

class [[nodiscard]] OneShotEventAwait : public Job {
 public:
  explicit OneShotEventAwait(OneShotEvent& event) noexcept : _event{event} {
  }

  YACLIB_INLINE bool await_ready() const noexcept {
    return _event.Ready();
  }

  template <typename Promise>
  YACLIB_INLINE bool await_suspend(yaclib_std::coroutine_handle<Promise> handle) noexcept {
    _core = &handle.promise();
    return _event.TryAdd(this);
  }

  constexpr void await_resume() const noexcept {
  }

 protected:
  void Call() noexcept final {
    _core->_executor->Submit(*_core);
  }

  detail::BaseCore* _core = nullptr;
  OneShotEvent& _event;
};

class [[nodiscard]] OneShotEventAwaitOn final : public OneShotEventAwait {
 public:
  explicit OneShotEventAwaitOn(OneShotEvent& event, IExecutor& executor) noexcept
    : OneShotEventAwait{event}, _executor{executor} {
  }

  YACLIB_INLINE bool await_ready() const noexcept {
    return false;
  }

  template <typename Promise>
  YACLIB_INLINE void await_suspend(yaclib_std::coroutine_handle<Promise> handle) noexcept {
    _core = &handle.promise();
    _core->_executor = &_executor;
    if (!_event.TryAdd(this)) {
      Call();
    }
  }

 private:
  IExecutor& _executor;
};

YACLIB_INLINE OneShotEventAwait OneShotEvent::operator co_await() noexcept {
  return Await();
}

YACLIB_INLINE OneShotEventAwait OneShotEvent::Await() noexcept {
  return OneShotEventAwait{*this};
}

YACLIB_INLINE OneShotEventAwaitOn OneShotEvent::AwaitOn(IExecutor& executor) noexcept {
  return OneShotEventAwaitOn{*this, executor};
}

#endif

}  // namespace yaclib
