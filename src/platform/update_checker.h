#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <optional>

#include "core/version.h"
#include "platform/http_client.h"

namespace dimmer {

enum class UpdateOutcome {
    Available,
    UpToDate,
    NewerThanRelease,
    ConnectionError,
    HttpError,
    InvalidRelease,
    ResponseTooLarge,
    InternalError,
};

struct UpdateCheckResult {
    UpdateOutcome outcome = UpdateOutcome::InternalError;
    std::optional<Version> latest;
    DWORD systemError = ERROR_SUCCESS;
    DWORD httpStatus = 0;
};

UpdateCheckResult EvaluateUpdateResponse(const Version& current, const HttpGetResult& response);

// The worker owns its state independently, so destroying the checker never waits on the network.
class UpdateChecker {
public:
    using FetchRelease = std::function<HttpGetResult()>;

    UpdateChecker() = default;
    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;

    bool Start(const Version& current, FetchRelease fetch, DWORD& error);
    bool Running() const noexcept { return state_ != nullptr; }
    HANDLE CompletionEvent() const noexcept;
    std::optional<UpdateCheckResult> TakeResult();

private:
    struct SharedState;
    static unsigned __stdcall WorkerMain(void* context) noexcept;

    std::shared_ptr<SharedState> state_;
};

}  // namespace dimmer
