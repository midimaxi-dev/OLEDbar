#include "platform/update_checker.h"

#include <process.h>

#include <exception>
#include <new>
#include <utility>

#include <wil/resource.h>

#include "core/update_check.h"

namespace dimmer {

UpdateCheckResult EvaluateUpdateResponse(const Version& current, const HttpGetResult& response) {
    UpdateCheckResult result;
    result.systemError = response.systemError;
    result.httpStatus = response.statusCode;
    switch (response.error) {
    case HttpError::InvalidInput:
        result.outcome = UpdateOutcome::InternalError;
        return result;
    case HttpError::WinHttp:
        result.outcome = UpdateOutcome::ConnectionError;
        return result;
    case HttpError::HttpStatus:
        result.outcome = UpdateOutcome::HttpError;
        return result;
    case HttpError::ResponseTooLarge:
        result.outcome = UpdateOutcome::ResponseTooLarge;
        return result;
    case HttpError::None:
        break;
    }

    if (response.statusCode != 200) {
        result.outcome = UpdateOutcome::HttpError;
        return result;
    }
    result.latest = ParseLatestReleaseTag(response.body);
    if (!result.latest) {
        result.outcome = UpdateOutcome::InvalidRelease;
        return result;
    }
    switch (CompareVersions(current, *result.latest)) {
    case VersionRelation::UpdateAvailable:
        result.outcome = UpdateOutcome::Available;
        break;
    case VersionRelation::UpToDate:
        result.outcome = UpdateOutcome::UpToDate;
        break;
    case VersionRelation::NewerThanRelease:
        result.outcome = UpdateOutcome::NewerThanRelease;
        break;
    }
    return result;
}

struct UpdateChecker::SharedState {
    wil::unique_handle event;
    SRWLOCK lock = SRWLOCK_INIT;
    bool ready = false;
    UpdateCheckResult result;
    Version current;
    FetchRelease fetch;
};

bool UpdateChecker::Start(const Version& current, FetchRelease fetch, DWORD& error) {
    error = ERROR_SUCCESS;
    if (state_) {
        error = ERROR_BUSY;
        return false;
    }
    if (!fetch) {
        error = ERROR_INVALID_PARAMETER;
        return false;
    }
    try {
        auto state = std::make_shared<SharedState>();
        state->current = current;
        state->fetch = std::move(fetch);
        state->event.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!state->event) {
            error = GetLastError();
            return false;
        }

        auto context = std::make_unique<std::shared_ptr<SharedState>>(state);
        auto* raw = context.release();
        _set_doserrno(0);
        const uintptr_t thread = _beginthreadex(nullptr, 0, &UpdateChecker::WorkerMain, raw, 0, nullptr);
        if (!thread) {
            unsigned long nativeError = 0;
            _get_doserrno(&nativeError);
            delete raw;
            error = nativeError ? nativeError : ERROR_NOT_ENOUGH_MEMORY;
            return false;
        }
        wil::unique_handle threadHandle(reinterpret_cast<HANDLE>(thread));
        state_ = std::move(state);
        return true;
    } catch (const std::bad_alloc&) {
        error = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
}

unsigned __stdcall UpdateChecker::WorkerMain(void* context) noexcept {
    const std::unique_ptr<std::shared_ptr<SharedState>> owner(static_cast<std::shared_ptr<SharedState>*>(context));
    const std::shared_ptr<SharedState>& state = *owner;
    UpdateCheckResult result;
    try {
        result = EvaluateUpdateResponse(state->current, state->fetch());
    } catch (const std::bad_alloc&) {
        result.outcome = UpdateOutcome::InternalError;
        result.systemError = ERROR_NOT_ENOUGH_MEMORY;
    } catch (const std::exception&) {
        result.outcome = UpdateOutcome::InternalError;
        result.systemError = ERROR_UNHANDLED_EXCEPTION;
    }
    AcquireSRWLockExclusive(&state->lock);
    state->result = result;
    state->ready = true;
    ReleaseSRWLockExclusive(&state->lock);
    if (!SetEvent(state->event.get())) {
        OutputDebugStringW(L"OLEDbar: failed to signal update-check completion\n");
    }
    return 0;
}

HANDLE UpdateChecker::CompletionEvent() const noexcept {
    return state_ ? state_->event.get() : nullptr;
}

std::optional<UpdateCheckResult> UpdateChecker::TakeResult() {
    if (!state_) {
        return std::nullopt;
    }
    AcquireSRWLockShared(&state_->lock);
    const std::optional<UpdateCheckResult> result = state_->ready ? std::optional(state_->result) : std::nullopt;
    ReleaseSRWLockShared(&state_->lock);
    if (result) {
        state_.reset();
    }
    return result;
}

}  // namespace dimmer
