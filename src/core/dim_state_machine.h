#pragma once

#include <chrono>
#include <optional>
#include <string_view>

#include "core/clock.h"

namespace dimmer {

struct DimPolicy {
    std::chrono::milliseconds idleTimeout{10'000};
    // Shorter countdown used after the user was on the taskbar (hover, Start/Search). Capped at idleTimeout.
    std::chrono::milliseconds redimDelay{3'000};
    double dimLevel{0.70};  // Fraction of light removed: 0 = no dimming, 1 = black.
    std::chrono::milliseconds fadeToDim{1'500};
    std::chrono::milliseconds fadeToWake{120};
};

enum class DimState { Awake, Countdown, Dimming, Dimmed, Waking, Suppressed };

std::wstring_view ToString(DimState state) noexcept;

// Pure, time-driven model of the taskbar dimming behaviour.
// Every input calls Update(); the host re-calls Update() on animation frames and at NextDeadline().
class DimStateMachine {
public:
    explicit DimStateMachine(const IClock& clock, DimPolicy policy = {});

    void SetPolicy(const DimPolicy& policy);
    void SetHover(bool hover);
    void PointerActivity();
    void SetPointerHeld(bool held);
    void SetInteraction(bool active);
    void SetSuppressed(bool suppressed);
    // Momentary wake that restarts the idle countdown (e.g. taskbar received focus).
    void Poke();
    // Dims immediately; the current hover is ignored until the pointer leaves the taskbar.
    void DimNow();
    void Update();

    DimState State() const noexcept { return state_; }
    double Opacity() const noexcept { return opacity_; }
    bool IsAnimating() const noexcept { return fade_.has_value(); }
    std::optional<TimePoint> NextDeadline() const noexcept;
    const DimPolicy& Policy() const noexcept { return policy_; }

private:
    struct Fade {
        double from;
        double to;
        TimePoint start;
        std::chrono::milliseconds duration;
    };

    void StartFade(double to, std::chrono::milliseconds duration, TimePoint now);
    void AdvanceFade(TimePoint now);
    void StartCountdown(TimePoint now, bool afterEngagement) noexcept;
    std::chrono::milliseconds CountdownDuration() const noexcept;
    bool Engaged(TimePoint now) const noexcept {
        return interaction_ || (!ignoreHover_ &&
            (pointerHeld_ || (hover_ && now - pointerActivitySince_ < policy_.idleTimeout)));
    }

    const IClock& clock_;
    DimPolicy policy_;
    DimState state_ = DimState::Countdown;
    double opacity_ = 0.0;
    std::optional<Fade> fade_;
    TimePoint idleSince_;
    TimePoint pointerActivitySince_{};
    bool countdownAfterEngagement_ = false;
    bool hover_ = false;
    bool pointerHeld_ = false;
    bool ignoreHover_ = false;
    bool interaction_ = false;
    bool suppressed_ = false;
};

}  // namespace dimmer
