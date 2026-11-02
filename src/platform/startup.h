#pragma once

namespace dimmer::startup {

// Per-user "run at sign-in" via HKCU\...\CurrentVersion\Run, respecting Task Manager's Startup apps toggle.
bool IsEnabled();
bool SetEnabled(bool enable);
// If enabled but the registered EXE no longer exists (it was moved), points the entry at the current EXE.
// Returns true if the registration was updated.
bool RefreshPath();

}  // namespace dimmer::startup
