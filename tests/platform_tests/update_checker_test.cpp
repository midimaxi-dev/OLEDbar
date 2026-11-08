#include <gtest/gtest.h>

#include <future>
#include <new>
#include <stdexcept>

#include <wil/resource.h>

#include "platform/update_checker.h"

using dimmer::EvaluateUpdateResponse;
using dimmer::HttpError;
using dimmer::HttpGetResult;
using dimmer::UpdateChecker;
using dimmer::UpdateOutcome;
using dimmer::Version;

namespace {

HttpGetResult Release(const char* body) {
    return {HttpError::None, ERROR_SUCCESS, 200, body};
}

TEST(UpdateCheckerTest, EvaluatesReleaseAndFailureOutcomes) {
    const Version current{1, 2, 3};
    EXPECT_EQ(EvaluateUpdateResponse(current, Release(R"({"tag_name":"v1.2.4"})")).outcome,
              UpdateOutcome::Available);
    EXPECT_EQ(EvaluateUpdateResponse(current, Release(R"({"tag_name":"v1.2.3"})")).outcome,
              UpdateOutcome::UpToDate);
    EXPECT_EQ(EvaluateUpdateResponse(current, Release(R"({"tag_name":"v1.2.2"})")).outcome,
              UpdateOutcome::NewerThanRelease);
    EXPECT_EQ(EvaluateUpdateResponse(current, Release("not json")).outcome, UpdateOutcome::InvalidRelease);

    const auto connection = EvaluateUpdateResponse(current, {HttpError::WinHttp, ERROR_TIMEOUT});
    EXPECT_EQ(connection.outcome, UpdateOutcome::ConnectionError);
    EXPECT_EQ(connection.systemError, ERROR_TIMEOUT);

    const auto forbidden = EvaluateUpdateResponse(current, {HttpError::HttpStatus, ERROR_SUCCESS, 403});
    EXPECT_EQ(forbidden.outcome, UpdateOutcome::HttpError);
    EXPECT_EQ(forbidden.httpStatus, 403u);
    EXPECT_EQ(EvaluateUpdateResponse(current, {HttpError::HttpStatus, ERROR_SUCCESS, 429}).outcome,
              UpdateOutcome::HttpError);
    EXPECT_EQ(EvaluateUpdateResponse(current, {HttpError::ResponseTooLarge, ERROR_SUCCESS, 200}).outcome,
              UpdateOutcome::ResponseTooLarge);
    EXPECT_EQ(EvaluateUpdateResponse(current, {HttpError::InvalidInput, ERROR_INVALID_PARAMETER}).outcome,
              UpdateOutcome::InternalError);
}

TEST(UpdateCheckerTest, SignalsAndConsumesCompletedCheck) {
    UpdateChecker checker;
    DWORD error = ERROR_SUCCESS;
    ASSERT_TRUE(checker.Start(Version{0, 3, 0}, [] { return Release(R"({"tag_name":"v0.4.0"})"); }, error));
    ASSERT_NE(checker.CompletionEvent(), nullptr);
    ASSERT_EQ(WaitForSingleObject(checker.CompletionEvent(), 5000), WAIT_OBJECT_0);
    const auto result = checker.TakeResult();
    ASSERT_TRUE(result);
    EXPECT_EQ(result->outcome, UpdateOutcome::Available);
    EXPECT_EQ(result->latest, (Version{0, 4, 0}));
    EXPECT_FALSE(checker.Running());
    EXPECT_EQ(checker.CompletionEvent(), nullptr);
    EXPECT_FALSE(checker.TakeResult());
}

TEST(UpdateCheckerTest, ReportsConnectionFailureFromWorker) {
    UpdateChecker checker;
    DWORD error = ERROR_SUCCESS;
    ASSERT_TRUE(checker.Start(Version{}, [] { return HttpGetResult{HttpError::WinHttp, ERROR_TIMEOUT}; }, error));
    ASSERT_EQ(WaitForSingleObject(checker.CompletionEvent(), 5000), WAIT_OBJECT_0);
    const auto result = checker.TakeResult();
    ASSERT_TRUE(result);
    EXPECT_EQ(result->outcome, UpdateOutcome::ConnectionError);
    EXPECT_EQ(result->systemError, ERROR_TIMEOUT);
}

TEST(UpdateCheckerTest, AllowsOnlyOneCheckAndRejectsAnEmptyFetcher) {
    std::promise<void> unblock;
    const std::shared_future<void> gate = unblock.get_future().share();
    UpdateChecker checker;
    DWORD error = ERROR_SUCCESS;
    EXPECT_FALSE(checker.Start(Version{}, {}, error));
    EXPECT_EQ(error, ERROR_INVALID_PARAMETER);
    ASSERT_TRUE(checker.Start(Version{}, [gate] {
        gate.wait();
        return Release(R"({"tag_name":"v1.0.0"})");
    }, error));
    EXPECT_FALSE(checker.TakeResult());
    EXPECT_FALSE(checker.Start(Version{}, [] { return Release("{}"); }, error));
    EXPECT_EQ(error, ERROR_BUSY);
    unblock.set_value();
    ASSERT_EQ(WaitForSingleObject(checker.CompletionEvent(), 5000), WAIT_OBJECT_0);
    EXPECT_TRUE(checker.TakeResult());
}

TEST(UpdateCheckerTest, ConvertsWorkerExceptionsToAnExplicitFailure) {
    UpdateChecker checker;
    DWORD error = ERROR_SUCCESS;
    ASSERT_TRUE(checker.Start(Version{}, []() -> HttpGetResult { throw std::runtime_error("test failure"); }, error));
    ASSERT_EQ(WaitForSingleObject(checker.CompletionEvent(), 5000), WAIT_OBJECT_0);
    const auto result = checker.TakeResult();
    ASSERT_TRUE(result);
    EXPECT_EQ(result->outcome, UpdateOutcome::InternalError);
    EXPECT_EQ(result->systemError, ERROR_UNHANDLED_EXCEPTION);
}

TEST(UpdateCheckerTest, ReportsWorkerAllocationFailure) {
    UpdateChecker checker;
    DWORD error = ERROR_SUCCESS;
    ASSERT_TRUE(checker.Start(Version{}, []() -> HttpGetResult { throw std::bad_alloc(); }, error));
    ASSERT_EQ(WaitForSingleObject(checker.CompletionEvent(), 5000), WAIT_OBJECT_0);
    const auto result = checker.TakeResult();
    ASSERT_TRUE(result);
    EXPECT_EQ(result->outcome, UpdateOutcome::InternalError);
    EXPECT_EQ(result->systemError, ERROR_NOT_ENOUGH_MEMORY);
}

TEST(UpdateCheckerTest, DestructionDoesNotWaitOrInvalidateWorkerState) {
    std::promise<void> unblock;
    const std::shared_future<void> gate = unblock.get_future().share();
    wil::unique_handle completion;
    {
        UpdateChecker checker;
        DWORD error = ERROR_SUCCESS;
        ASSERT_TRUE(checker.Start(Version{}, [gate] {
            gate.wait();
            return Release(R"({"tag_name":"v1.0.0"})");
        }, error));
        HANDLE duplicate = nullptr;
        ASSERT_TRUE(DuplicateHandle(GetCurrentProcess(), checker.CompletionEvent(), GetCurrentProcess(), &duplicate,
                                    0, FALSE, DUPLICATE_SAME_ACCESS));
        completion.reset(duplicate);
    }
    unblock.set_value();
    EXPECT_EQ(WaitForSingleObject(completion.get(), 5000), WAIT_OBJECT_0);
}

}  // namespace
