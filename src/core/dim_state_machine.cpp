#include "core/dim_state_machine.h"

#include <algorithm>

namespace dimmer {
namespace {

double EaseOutCubic(double t) noexcept {
    const double u = 1.0 - t;
    return 1.0 - u * u * u;
}

}  // namespace

std::wstring_view ToString(DimState state) noexcept {
    switch (state) {
    case DimState::Awake: return L"Awake";
    case DimState::Countdown: return L"Countdown";
    case DimState::Dimming: return L"Dimming";
    case DimState::Dimmed: return L"Dimmed";
    case DimState::Waking: return L"Waking";
    case DimState::Suppressed: return L"Suppressed";
    }
    return L"Unknown";
}

DimStateMachine::DimStateMachine(const IClock& clock, DimPolicy policy)
    : clock_(clock), policy_(policy), idleSince_(clock.Now()) {
    policy_.dimLevel = std::clamp(policy_.dimLevel, 0.0, 1.0);
}

void DimStateMachine::SetPolicy(const DimPolicy& policy) {
    policy_ = policy;
    policy_.dimLevel = std::clamp(policy_.dimLevel, 0.0, 1.0);
    if (state_ == DimState::Dimmed) {
        opacity_ = policy_.dimLevel;
    }
    Update();
}

void DimStateMachine::SetHover(bool hover) {
    hover_ = hover;
    if (!hover) {
        ignoreHover_ = false;
    }
    Update();
}

void DimStateMachine::SetInteraction(bool active) {
    interaction_ = active;
    Update();
}

void DimStateMachine::SetSuppressed(bool suppressed) {
    suppressed_ = suppressed;
    Update();
}

void DimStateMachine::Poke() {
    const TimePoint now = clock_.Now();
    AdvanceFade(now);
    if (suppressed_) {
        return;
    }
    StartFade(0.0, policy_.fadeToWake, now);
    state_ = DimState::Countdown;
    StartCountdown(now, false);
    Update();
}

void DimStateMachine::DimNow() {
    if (suppressed_) {
        return;
    }
    const TimePoint now = clock_.Now();
    AdvanceFade(now);
    ignoreHover_ = hover_;
    StartFade(policy_.dimLevel, policy_.fadeToDim, now);
    state_ = fade_ ? DimState::Dimming : DimState::Dimmed;
    Update();
}

void DimStateMachine::Update() {
    const TimePoint now = clock_.Now();
    AdvanceFade(now);

    if (suppressed_) {
        state_ = DimState::Suppressed;
        fade_.reset();
        opacity_ = 0.0;
        return;
    }
    if (state_ == DimState::Suppressed) {
        state_ = DimState::Countdown;
        StartCountdown(now, false);
    }

    if (Engaged()) {
        const bool alreadyWaking = fade_ && fade_->to == 0.0;
        if (!alreadyWaking) {
            StartFade(0.0, policy_.fadeToWake, now);
        }
        state_ = opacity_ > 0.0 ? DimState::Waking : DimState::Awake;
        return;
    }

    switch (state_) {
    case DimState::Awake:
    case DimState::Waking:
        // Any remaining wake fade keeps running while the countdown restarts.
        state_ = DimState::Countdown;
        StartCountdown(now, true);
        [[fallthrough]];
    case DimState::Countdown:
        if (now - idleSince_ >= CountdownDuration()) {
            StartFade(policy_.dimLevel, policy_.fadeToDim, now);
            state_ = fade_ ? DimState::Dimming : DimState::Dimmed;
        }
        break;
    case DimState::Dimming:
        if (!fade_) {
            state_ = DimState::Dimmed;
        }
        break;
    case DimState::Dimmed:
    case DimState::Suppressed:
        break;
    }
}

std::optional<TimePoint> DimStateMachine::NextDeadline() const noexcept {
    if (state_ != DimState::Countdown) {
        return std::nullopt;
    }
    return idleSince_ + CountdownDuration();
}

void DimStateMachine::StartCountdown(TimePoint now, bool afterEngagement) noexcept {
    idleSince_ = now;
    countdownAfterEngagement_ = afterEngagement;
}

std::chrono::milliseconds DimStateMachine::CountdownDuration() const noexcept {
    return countdownAfterEngagement_ ? std::min(policy_.redimDelay, policy_.idleTimeout) : policy_.idleTimeout;
}

void DimStateMachine::StartFade(double to, std::chrono::milliseconds duration, TimePoint now) {
    if (duration.count() <= 0 || opacity_ == to) {
        opacity_ = to;
        fade_.reset();
        return;
    }
    fade_ = Fade{opacity_, to, now, duration};
}

void DimStateMachine::AdvanceFade(TimePoint now) {
    if (!fade_) {
        return;
    }
    const auto elapsed = now - fade_->start;
    if (elapsed >= fade_->duration) {
        opacity_ = fade_->to;
        fade_.reset();
        return;
    }
    const double t = std::chrono::duration<double>(elapsed).count() /
                     std::chrono::duration<double>(fade_->duration).count();
    opacity_ = fade_->from + (fade_->to - fade_->from) * EaseOutCubic(t);
}

}  // namespace dimmer
