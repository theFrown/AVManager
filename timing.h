#pragma once

#include <iostream>
#include <format>
#include <chrono>
#include <thread>
#include <optional>
#include <atomic>

class Timer {
private:
    using Clock_ = std::chrono::steady_clock;
    using MilliSeconds_ = std::chrono::milliseconds;
    int default_period_;
    std::optional<Clock_::time_point> deadline_;

public:
    explicit Timer(int ms = 0, bool set_now = false) : default_period_(ms) { 
        if (set_now) Set(ms);
    }

    void Set(int ms) { deadline_ = Clock_::now() + MilliSeconds_(ms); }
    void Set() { Set(default_period_); }
    void Reset() { deadline_.reset(); }
    void SetDefault(int ms) { default_period_ = ms; }
    void Wait() const { if (IsPending()) std::this_thread::sleep_until(*deadline_); }
    bool IsSet() const { return deadline_.has_value(); }
    bool IsExpired() const { return deadline_.has_value() && (Clock_::now() > *deadline_); }
    bool IsPending() const { return IsSet() && !IsExpired(); }
    std::optional<int> GetRemaining() const {
        if (!IsSet()) return std::nullopt;
        auto remaining = *deadline_ - Clock_::now();
        int remaining_ms = static_cast<int>(std::chrono::ceil<MilliSeconds_>(remaining).count());
        return ((remaining_ms > 0) ? remaining_ms : 0);
    }
};

class Stopwatch {
/*
Most methods take a single optional argument 'bool reset', which defaults to false. When set 
to true the method will switch out the atomic operation used for the base function of the 
method to an operation that combines that original function with reseting the timer value. 
Exceptions: the constructor, where reset doesn't make sense, and Reset() itself.
*/
private:
    using Clock_ = std::chrono::steady_clock;
    using MilliSeconds_ = std::chrono::milliseconds;
    std::atomic<Clock_::time_point> start_;
    constexpr static Clock_::time_point zero_{};


public:
    explicit Stopwatch(bool start = true) { if (start) Start(); }
    ~Stopwatch() = default;
    Stopwatch(const Stopwatch&) = delete;
    Stopwatch(Stopwatch&&) = delete;
    Stopwatch& operator=(const Stopwatch&) = delete;
    Stopwatch& operator=(Stopwatch&&) = delete;

    static_assert(std::atomic<Clock_::time_point>::is_always_lock_free);
    void Start(bool reset = false) {
        if (reset) {
            start_.store(Clock_::now());
        }
        else {
            Clock_::time_point expected = zero_;
            start_.compare_exchange_strong(expected, Clock_::now());
        }
    }
    void Reset() { start_.store(zero_); }
    std::optional<int> Read(bool reset = false) {
        Clock_::time_point start = (reset) ? start_.exchange(zero_) : start_.load();
        if (start == zero_) {
            return std::nullopt;
        }
        else {
            auto delta = Clock_::now() - start;
            return static_cast<int>(std::chrono::duration_cast<MilliSeconds_>(delta).count());
        }
    }
    void Print(bool reset = false) {
        auto value = Read(reset);
        if (value.has_value()) {
            std::cout << std::format("Time elapsed: {} ms\n", *value);
        }
        else {
            std::cout << "stopwatch wasn't running!\n";
        }
    }
};
