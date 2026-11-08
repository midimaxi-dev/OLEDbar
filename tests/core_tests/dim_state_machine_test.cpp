#include <gtest/gtest.h>

#include <chrono>

#include "core/dim_state_machine.h"

using namespace std::chrono_literals;
using dimmer::DimPolicy;
using dimmer::DimState;
using dimmer::DimStateMachine;

namespace {

class FakeClock final : public dimmer::IClock {
public:
    dimmer::TimePoint Now() const override { return now_; }
    void Advance(std::chrono::milliseconds delta) { now_ += delta; }

private:
    dimmer::TimePoint now_{};
};

class DimStateMachineTest : public ::testing::Test {
protected:
    void AdvanceAndUpdate(std::chrono::milliseconds delta) {
        clock_.Advance(delta);
        machine_.Update();
    }

    void DimFully() {
        AdvanceAndUpdate(policy_.idleTimeout);
        AdvanceAndUpdate(policy_.fadeToDim);
        ASSERT_EQ(machine_.State(), DimState::Dimmed);
    }

    FakeClock clock_;
    DimPolicy policy_{};
    DimStateMachine machine_{clock_, policy_};
};

TEST_F(DimStateMachineTest, StartsBrightAndCountingDown) {
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
    ASSERT_TRUE(machine_.NextDeadline().has_value());
    EXPECT_TRUE(*machine_.NextDeadline() == clock_.Now() + policy_.idleTimeout);
}

TEST_F(DimStateMachineTest, StaysBrightBeforeTimeout) {
    AdvanceAndUpdate(policy_.idleTimeout - 1ms);
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
}

TEST_F(DimStateMachineTest, DimsWithFadeAfterTimeout) {
    AdvanceAndUpdate(policy_.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
    EXPECT_TRUE(machine_.IsAnimating());

    AdvanceAndUpdate(policy_.fadeToDim / 2);
    EXPECT_GT(machine_.Opacity(), 0.0);
    EXPECT_LT(machine_.Opacity(), policy_.dimLevel);

    AdvanceAndUpdate(policy_.fadeToDim);
    EXPECT_EQ(machine_.State(), DimState::Dimmed);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), policy_.dimLevel);
    EXPECT_FALSE(machine_.IsAnimating());
    EXPECT_FALSE(machine_.NextDeadline().has_value());
}

TEST_F(DimStateMachineTest, HoverWakesQuickly) {
    DimFully();
    machine_.SetHover(true);
    EXPECT_EQ(machine_.State(), DimState::Waking);

    AdvanceAndUpdate(policy_.fadeToWake);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
}

TEST_F(DimStateMachineTest, StationaryHoverDimsAtIdleDeadline) {
    machine_.SetHover(true);
    ASSERT_TRUE(machine_.NextDeadline().has_value());
    EXPECT_EQ(*machine_.NextDeadline(), clock_.Now() + policy_.idleTimeout);
    AdvanceAndUpdate(policy_.idleTimeout - 1ms);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    machine_.SetHover(true);
    AdvanceAndUpdate(1ms);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
    AdvanceAndUpdate(policy_.fadeToDim);
    EXPECT_EQ(machine_.State(), DimState::Dimmed);
    EXPECT_FALSE(machine_.NextDeadline().has_value());
}

TEST_F(DimStateMachineTest, PointerActivityRestartsHoverTimeoutAndWakes) {
    machine_.SetHover(true);
    AdvanceAndUpdate(policy_.idleTimeout - 1ms);
    machine_.PointerActivity();
    EXPECT_EQ(*machine_.NextDeadline(), clock_.Now() + policy_.idleTimeout);
    AdvanceAndUpdate(policy_.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
    AdvanceAndUpdate(policy_.fadeToDim);
    machine_.PointerActivity();
    EXPECT_EQ(machine_.State(), DimState::Waking);
    AdvanceAndUpdate(policy_.fadeToWake);
    EXPECT_EQ(machine_.State(), DimState::Awake);
}

TEST_F(DimStateMachineTest, PointerActivityOutsideTaskbarDoesNotResetCountdown) {
    AdvanceAndUpdate(policy_.idleTimeout - 1ms);
    machine_.PointerActivity();
    AdvanceAndUpdate(1ms);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, ContinuousPointerActivityKeepsHoverBright) {
    machine_.SetHover(true);
    for (int sample = 0; sample < 100; ++sample) {
        AdvanceAndUpdate(1s);
        machine_.PointerActivity();
        EXPECT_EQ(machine_.State(), DimState::Awake);
        EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
    }
}

TEST_F(DimStateMachineTest, ShorteningHoverTimeoutDimsWithoutAnotherCountdown) {
    machine_.SetHover(true);
    AdvanceAndUpdate(5s);
    DimPolicy shorter = policy_;
    shorter.idleTimeout = 4s;
    machine_.SetPolicy(shorter);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, ExtendingHoverTimeoutUpdatesDeadline) {
    machine_.SetHover(true);
    AdvanceAndUpdate(5s);
    DimPolicy longer = policy_;
    longer.idleTimeout = 20s;
    machine_.SetPolicy(longer);
    ASSERT_TRUE(machine_.NextDeadline().has_value());
    EXPECT_EQ(*machine_.NextDeadline(), clock_.Now() + 15s);
    AdvanceAndUpdate(15s);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, SuppressionRestartsStationaryHoverTimeout) {
    machine_.SetHover(true);
    AdvanceAndUpdate(5s);
    machine_.SetSuppressed(true);
    AdvanceAndUpdate(1h);
    machine_.PointerActivity();
    EXPECT_EQ(machine_.State(), DimState::Suppressed);
    EXPECT_FALSE(machine_.NextDeadline().has_value());
    machine_.SetSuppressed(false);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    EXPECT_EQ(*machine_.NextDeadline(), clock_.Now() + policy_.idleTimeout);
    AdvanceAndUpdate(policy_.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, ShellInteractionHoldsHoverAndRestartsTimeoutOnClose) {
    machine_.SetHover(true);
    machine_.SetInteraction(true);
    AdvanceAndUpdate(1h);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    EXPECT_FALSE(machine_.NextDeadline().has_value());
    machine_.SetInteraction(false);
    EXPECT_EQ(*machine_.NextDeadline(), clock_.Now() + policy_.idleTimeout);
    AdvanceAndUpdate(policy_.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, PokeRestartsStationaryHoverTimeout) {
    machine_.SetHover(true);
    AdvanceAndUpdate(policy_.idleTimeout);
    AdvanceAndUpdate(policy_.fadeToDim);
    machine_.Poke();
    EXPECT_EQ(machine_.State(), DimState::Waking);
    EXPECT_EQ(*machine_.NextDeadline(), clock_.Now() + policy_.idleTimeout);
    AdvanceAndUpdate(policy_.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, DragHoldsAwakeAndReleaseRestartsHoverTimeout) {
    machine_.SetHover(true);
    machine_.SetPointerHeld(true);
    AdvanceAndUpdate(1h);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    EXPECT_FALSE(machine_.NextDeadline().has_value());
    machine_.SetPointerHeld(false);
    EXPECT_EQ(*machine_.NextDeadline(), clock_.Now() + policy_.idleTimeout);
    AdvanceAndUpdate(policy_.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, DragLeavingTaskbarHoldsUntilRelease) {
    machine_.SetHover(true);
    machine_.SetPointerHeld(true);
    machine_.SetHover(false);
    AdvanceAndUpdate(1h);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    machine_.SetPointerHeld(false);
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    AdvanceAndUpdate(policy_.redimDelay);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, LeavingAfterStationaryDimDoesNotWake) {
    machine_.SetHover(true);
    AdvanceAndUpdate(policy_.idleTimeout);
    AdvanceAndUpdate(policy_.fadeToDim);
    machine_.SetHover(false);
    EXPECT_EQ(machine_.State(), DimState::Dimmed);
}

TEST_F(DimStateMachineTest, LeavingStartsShortRedimCountdown) {
    machine_.SetHover(true);
    AdvanceAndUpdate(1s);
    machine_.SetHover(false);
    EXPECT_EQ(machine_.State(), DimState::Countdown);

    AdvanceAndUpdate(policy_.redimDelay - 1ms);
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    AdvanceAndUpdate(1ms);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, RedimDelayIsCappedAtIdleTimeout) {
    DimPolicy policy;
    policy.idleTimeout = 2s;
    policy.redimDelay = 5s;
    machine_.SetPolicy(policy);
    machine_.SetHover(true);
    machine_.SetHover(false);

    AdvanceAndUpdate(policy.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, SuppressionHidesInstantlyAndRestartsCountdownAfterwards) {
    DimFully();
    machine_.SetSuppressed(true);
    EXPECT_EQ(machine_.State(), DimState::Suppressed);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
    EXPECT_FALSE(machine_.IsAnimating());
    EXPECT_FALSE(machine_.NextDeadline().has_value());

    AdvanceAndUpdate(1h);
    EXPECT_EQ(machine_.State(), DimState::Suppressed);

    machine_.SetSuppressed(false);
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
    AdvanceAndUpdate(policy_.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, PokeWakesAndRestartsCountdown) {
    DimFully();
    machine_.Poke();
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    EXPECT_TRUE(machine_.IsAnimating());

    AdvanceAndUpdate(policy_.fadeToWake);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);

    AdvanceAndUpdate(policy_.idleTimeout - policy_.fadeToWake - 1ms);
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    AdvanceAndUpdate(1ms);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, InteractionHoldsAwake) {
    machine_.SetInteraction(true);
    AdvanceAndUpdate(1h);
    EXPECT_EQ(machine_.State(), DimState::Awake);

    machine_.SetInteraction(false);
    EXPECT_EQ(machine_.State(), DimState::Countdown);
    AdvanceAndUpdate(policy_.redimDelay);
    EXPECT_EQ(machine_.State(), DimState::Dimming);
}

TEST_F(DimStateMachineTest, DimNowIgnoresHoverUntilPointerLeaves) {
    machine_.SetHover(true);
    EXPECT_EQ(machine_.State(), DimState::Awake);

    machine_.DimNow();
    EXPECT_EQ(machine_.State(), DimState::Dimming);
    machine_.PointerActivity();
    machine_.SetPointerHeld(true);
    AdvanceAndUpdate(policy_.fadeToDim);
    EXPECT_EQ(machine_.State(), DimState::Dimmed);

    machine_.SetPointerHeld(false);
    machine_.SetHover(false);
    EXPECT_EQ(machine_.State(), DimState::Dimmed);
    machine_.SetHover(true);
    EXPECT_EQ(machine_.State(), DimState::Waking);
}

TEST_F(DimStateMachineTest, ZeroFadeDurationsAreInstant) {
    DimPolicy instant;
    instant.fadeToDim = 0ms;
    instant.fadeToWake = 0ms;
    machine_.SetPolicy(instant);

    AdvanceAndUpdate(instant.idleTimeout);
    EXPECT_EQ(machine_.State(), DimState::Dimmed);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), instant.dimLevel);

    machine_.SetHover(true);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
}

TEST_F(DimStateMachineTest, DimLevelIsClamped) {
    DimPolicy policy;
    policy.dimLevel = 1.5;
    machine_.SetPolicy(policy);
    EXPECT_DOUBLE_EQ(machine_.Policy().dimLevel, 1.0);
}

}  // namespace
