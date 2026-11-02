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

TEST_F(DimStateMachineTest, HoverHoldsAwake) {
    machine_.SetHover(true);
    AdvanceAndUpdate(1h);
    EXPECT_EQ(machine_.State(), DimState::Awake);
    EXPECT_DOUBLE_EQ(machine_.Opacity(), 0.0);
    EXPECT_FALSE(machine_.NextDeadline().has_value());
}

TEST_F(DimStateMachineTest, LeavingStartsShortRedimCountdown) {
    machine_.SetHover(true);
    AdvanceAndUpdate(1min);
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
    AdvanceAndUpdate(policy_.fadeToDim);
    EXPECT_EQ(machine_.State(), DimState::Dimmed);

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
