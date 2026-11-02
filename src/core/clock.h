#pragma once

#include <chrono>

namespace dimmer {

using TimePoint = std::chrono::steady_clock::time_point;

class IClock {
public:
    virtual ~IClock() = default;
    virtual TimePoint Now() const = 0;
};

class SteadyClock final : public IClock {
public:
    TimePoint Now() const override { return std::chrono::steady_clock::now(); }
};

}  // namespace dimmer
