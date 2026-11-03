#pragma once

namespace dimmer {

// Same as right-clicking the desktop -> View -> Show desktop icons. Explorer persists the new state itself.
// Returns false if the desktop view window could not be found or the request could not be sent.
bool ToggleDesktopIcons();

}  // namespace dimmer
