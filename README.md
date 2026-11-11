# OLEDbar

[![Build](https://github.com/midimaxi-dev/OLEDbar/actions/workflows/build.yml/badge.svg)](https://github.com/midimaxi-dev/OLEDbar/actions/workflows/build.yml)

**Helps reduce the risk of OLED taskbar burn-in while keeping the taskbar visible.** [Check OLEDbar Website](https://midimaxi-dev.github.io/OLEDbar/)

After a few seconds without mouse activity on the Windows 11 taskbar, a dark, click-through layer fades in over it, even if the pointer is parked on it. This dims the background, icons, text, clock and tray. Moving onto or within the taskbar, clicking or scrolling wakes it. It stays bright during a drag and, by default, while Start or Search is open.

> **Status:** version 0.4.1, a usable early release.

- **Tiny footprint, not a heavy suite:** a **~715 KB release EXE**, using **under 1 MB of idle private working set** in our local test (as shown in Task Manager; memory use varies by system). No runtime to install; no admin rights required.
- No injection into Explorer: it only draws a click-through layer above the taskbar.
- Settings panel with a live preview of the dim level on your taskbar.
- By default, hides the overlay for fullscreen apps and when the taskbar is auto-hidden.
- By default, keeps the dimming overlay out of supported screenshots and screen sharing.
- Pauses itself while the PC is locked or the screen is off.
- Optional keyboard shortcut (and tray menu item) to show or hide the desktop icons.

## Who this is for

**OLEDbar is for you if:**

- you use an OLED laptop or monitor on Windows 11
- you don't want taskbar auto-hide
- you want a tiny utility, not a heavy suite
- you care about reducing static UI brightness

## Download and install

Get the latest version from **[Releases](https://github.com/midimaxi-dev/OLEDbar/releases/latest)**. It requires Windows 11 22H2 or later (x64).

| Download | Best for |
|---|---|
| `OLEDbar-<version>-x64-setup.exe` | Most people. Installs for your user only, no admin rights. Adds a Start menu entry, can start OLEDbar when you sign in, and appears in *Settings → Apps* for uninstalling. |
| `OLEDbar-<version>-x64.zip` | Portable use. Unzip anywhere and run `OLEDbar.exe`. Turn on *Start OLEDbar when I sign in* in its settings if you like. |

`SHA256SUMS.txt` lists checksums for both files. To verify a download, compare its hash with the matching entry, for example: `Get-FileHash .\OLEDbar-0.4.1-x64-setup.exe`.

Windows may warn about unsigned releases; see [Unsigned releases](#unsigned-releases).

## Settings

Left-click the OLEDbar tray icon to open the settings panel. The icon may be hidden behind the **^** arrow.

![OLEDbar settings panel with dimming, behavior and keyboard shortcut options](docs/Screenshot-OLEDbar-settings.png)

Adjust the dim level with a live preview, change delays and fade durations, or choose behavior options. You can also start OLEDbar when you sign in and set a keyboard shortcut to toggle desktop icons (off by default; for example, Ctrl+Alt+D).

Press **OK** or **Apply** to save; **Restore defaults** resets the controls. For advanced configuration, see [Settings file](#settings-file).

## Version and updates

Open **About** from the settings panel, or right-click the tray icon and choose **About OLEDbar...**, to see the running version. Select **Check for updates** to check GitHub for the latest stable release. If a newer version is available, **Download...** opens the Releases page in your browser.

Checks are always initiated by you; OLEDbar does not automatically check, download, or install updates. The Releases link in About is also available if a check fails.

## FAQ

- **Does it work with auto-hide?** Yes. The overlay hides while the taskbar is hidden and can resume when it reappears.
- **Are clicks and scrolling always detected?** Normally yes, through mouse Raw Input. If registration fails, OLEDbar falls back to polling: movement and held buttons on the taskbar still wake it, but stationary scrolling and brief clicks between polls may be missed. HID-only touchpad, touch or pen activity without cursor movement may also be missed; moving the pointer on the taskbar wakes it.
- **Does it affect fullscreen apps?** By default, OLEDbar hides its overlay when it detects a fullscreen app or video.
- **Does it show up in screenshots?** Normally not. By default, OLEDbar asks Windows to exclude the dimming overlay from screenshots and screen sharing; you can change this in Settings.
- **Does it support multi-monitor?** Yes, but only the primary display's taskbar is dimmed. Additional taskbars are not dimmed yet.
- **Does it require admin rights?** No. Both the per-user installer and the portable app run without administrator rights.
- **Is it signed?** Releases produced by the signing-enabled workflow are signed and timestamped with Microsoft Artifact Signing. SmartScreen warnings may still appear while download reputation builds; older releases remain unsigned.
- **Does it send data anywhere?** Only if you choose to check for updates. GitHub receives the connection's public IP address and the OLEDbar version; settings and logs stay local. See [Privacy](#privacy).

## Uninstall

- **Installer version:** *Settings → Apps → Installed apps → OLEDbar → Uninstall*. This closes OLEDbar and removes the app, its startup entry, settings and logs.
- **Portable version:** untick *Start OLEDbar when I sign in* in its settings, choose **Exit** from the tray menu, and delete the folder. Settings and logs are in `%LOCALAPPDATA%\OLEDbar`; delete that folder too.

## Privacy

OLEDbar does not send telemetry or check for updates automatically. When you select **Check for updates**, it sends an HTTPS request to `api.github.com` for the latest release. GitHub receives the connection's public IP address (which may be a proxy's) and a `User-Agent` containing the OLEDbar version; settings and logs are not sent. Your configured proxy or network provider may also process the connection.

Settings and logs are stored locally in `%LOCALAPPDATA%\OLEDbar`; the optional *Start when I sign in* entry is stored in your user's registry. The website, GitHub and Releases links open in your browser, which makes its own connections.

## Building from source

Requirements:
- Windows 11 22H2 or later, x64
- Visual Studio Build Tools 2022 (C++), only needed to build from source

## Setup (one time)

1. **Install the C++ build tools.** Run this from an **elevated** terminal (Win+X → *Terminal (Admin)*). It's about 3–4 GB:

   ```powershell
   winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--wait --nocache --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.Vcpkg"
   ```

   If Build Tools are **already installed**, winget won't add components. Use the Visual Studio Installer instead: *Modify* → *Individual components*, then tick:
   - MSVC v143 x64/x86 build tools
   - Windows 11 SDK (10.0.26100)
   - C++ CMake tools for Windows
   - vcpkg package manager

2. **VS Code extensions:** install *C/C++* (`ms-vscode.cpptools`) and *CMake Tools* (`ms-vscode.cmake-tools`). VS Code offers them when you open the folder.

3. **Check the install.** All four lines should print `True`:

   ```powershell
   $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
   'VC.Tools.x86.x64','Windows11SDK.26100','VC.CMake.Project','Vcpkg' | ForEach-Object {
     "$_ = $([bool](& $vswhere -products * -requires "Microsoft.VisualStudio.Component.$_" -property installationPath))"
   }
   ```

## Build and test

**In VS Code:** select the **x64 Debug** preset in the CMake Tools status bar, then press **F7** to build.

**From a terminal:** load the compiler environment first (once per terminal session), then configure, build and test:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
cmake --preset x64-debug          # first run downloads WIL and GoogleTest via vcpkg
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Use the `x64-release` presets for a smaller, optimized build.

## Run

```powershell
.\out\build\x64-debug\OLEDbar.exe
```

Or press **F5** in VS Code to run it under the debugger. For everyday use, build the release preset (`cmake --preset x64-release; cmake --build --preset x64-release`) and run `out\build\x64-release\OLEDbar.exe`. It's a single ~715 KB EXE, and you can copy it anywhere.

Whenever OLEDbar starts, a notification tells you it is running and where to find its tray icon, even if you already have saved settings. Windows notification settings or Do not disturb may hide it.

- Run it from a **normal (non-admin)** terminal. An elevated copy can't be stopped or rebuilt from a normal terminal.
- Only one copy runs at a time; launching it again opens the settings panel of the running copy. **Exit** it from the tray menu before rebuilding, otherwise linking `OLEDbar.exe` fails.

**Tray icon** (it may be hidden behind the **^** arrow): a small screen with a taskbar strip, which looks faded while paused. **Left-click** opens the settings panel; **right-click** opens the menu:

| Item | Purpose |
|---|---|
| Pause ▸ | For 15 minutes, for 1 hour, or until you resume. Shows **Resume** while paused |
| Dim now | Dim immediately (useful for testing) |
| Toggle desktop icons | Show or hide the desktop icons, like the desktop's *View → Show desktop icons*. Shows your shortcut, if set |
| Settings... | Open the settings panel |
| Open log | Open `%LOCALAPPDATA%\OLEDbar\logs\oledbar.log` (state changes, taskbar, fullscreen, session, settings and update-check events) |
| About OLEDbar... | Show the version and optionally check for updates |
| Exit | Quit |

## Settings file

The same settings are stored in `%LOCALAPPDATA%\OLEDbar\settings.json` (the panel's **Settings file...** button opens it). If you edit and save the file directly, the changes apply immediately, with no restart. If something is wrong, a notification appears and the details go to the log.

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | `false` turns dimming off until you set it back |
| `idleTimeoutSeconds` | `10` | Delay before dimming while the pointer is stationary on the taskbar, at startup, and after a pause or fullscreen app (1–3600) |
| `redimDelaySeconds` | `3` | Delay before dimming after the pointer leaves the taskbar or Start/Search closes with the pointer elsewhere (0–3600) |
| `dimLevelPercent` | `70` | How much light to remove: 0 = none, 100 = black |
| `fadeToDimMs` | `1500` | Fade-out duration (0–5000) |
| `fadeToWakeMs` | `120` | Fade-in duration on taskbar activity or another wake signal (0–5000) |
| `wakeOnTaskbarFocus` | `true` | Wake when the taskbar gets keyboard focus (Win+T, clicking it) |
| `wakeOnShellUi` | `true` | Stay bright while Start, Search or Quick Settings is open |
| `hideInFullscreen` | `true` | Hide the overlay when a fullscreen app or video is detected |
| `excludeFromCapture` | `true` | Exclude the overlay from supported screenshots and screen sharing |
| `logLevel` | `"info"` | `"debug"` also logs every foreground-window change and hover, for troubleshooting |
| `toggleDesktopIconsHotkey` | `""` | Global shortcut that shows/hides desktop icons, e.g. `"Ctrl+Alt+D"`. Needs Ctrl or Alt; `""` turns it off. Works even while dimming is paused or disabled |

If the shortcut is already taken by another app, OLEDbar tells you and leaves it unassigned; pick a different one.

Invalid JSON is reported and ignored: OLEDbar keeps the previous settings (defaults at startup) and doesn't touch your file. `//` comments are allowed, but saving from the settings panel rewrites the file without them.

**Start OLEDbar when I sign in** is stored in the registry (`HKCU\...\CurrentVersion\Run`), not in the JSON file. It respects the toggle in Task Manager → *Startup apps*. If you move the EXE, the entry is updated the next time you start it.

## Packaging and releases

**Local packages:** after a release build, run:

```powershell
.\packaging\package.ps1
```

This writes `OLEDbar-<version>-x64.zip`, `SHA256SUMS.txt` and, if [Inno Setup 6](https://jrsoftware.org/isinfo.php) is installed (`winget install JRSoftware.InnoSetup`), `OLEDbar-<version>-x64-setup.exe` to `out\dist`. The installer is defined in `packaging/installer/oledbar.iss`.

**CI:** GitHub Actions ([.github/workflows/build.yml](.github/workflows/build.yml)) builds, tests and packages every push and pull request. The zip and installer are attached to each run as artifacts.

**Releasing a version:**
1. Bump `project(... VERSION x.y.z)` in `CMakeLists.txt` and commit. This is the only place the version is set.
2. Tag and push:
   ```powershell
   git tag v0.4.1; git push origin v0.4.1
   ```
3. The workflow checks that the tag matches the project version, builds, and creates a **draft** GitHub Release with the installer, zip and checksums. Review it on GitHub, then click **Publish**.

## Known limitations

- **Multiple monitors:** only the main taskbar (the one on your primary display) is dimmed. Extra taskbars shown with *Settings → Personalization → Taskbar → Show my taskbar on all displays* are not dimmed yet.

## Troubleshooting

- **Builds or tests are slow the first time:** Windows security software may scan newly built executables on their first launch. Later runs are usually faster.
- **`winget` says "No available upgrade found":** Build Tools are already installed. Add the missing components with the Visual Studio Installer (see Setup).
- **The installer exits with code 5007:** a `--passive` install was run from a non-elevated terminal. Use an admin terminal, or drop `--passive` so the installer can ask for elevation itself.
- **An update check fails:** Check your connection, proxy or firewall. GitHub may also deny or rate-limit requests. You can open the Releases page from About without a successful check.

## Project layout

```
src/core/        Pure C++: dimming state machine, settings, shortcuts and version/update parsing (no Windows headers, unit tested)
src/platform/    Win32: overlay, input and shell events, settings, startup, desktop icons, HTTPS/update worker and logging
src/app/         WinMain, tray icon, Settings and About dialogs (.rc), wiring (AppHost), manifest
tests/           GoogleTest unit tests
packaging/       package.ps1 (zip + installer) and installer/oledbar.iss (Inno Setup)
.github/         GitHub Actions workflow (build, test, package, draft release on tags)
```

## Code signing policy

### Signed releases

Version-tag releases are built and signed by GitHub Actions from this repository's source code ([.github/workflows/build.yml](.github/workflows/build.yml)). The executable and installer are timestamped with Microsoft Artifact Signing; the portable ZIP contains the signed executable. Nothing built locally is signed.

The public certificate identifies the individual publisher. It includes the publisher's name and city, state/province, and country; it does not include an email address, street address, or postal code. Review the certificate subject before publishing a release.

The signing job runs only for version tags and uses GitHub OpenID Connect (OIDC), not a stored Azure password. To enable signing:

1. Create a Microsoft Entra app registration and service principal.
2. Add a federated credential with issuer `https://token.actions.githubusercontent.com`, audience `api://AzureADTokenExchange`, and subject `repo:midimaxi-dev@158115211/OLEDbar@1443120479:environment:release`. The subject uses GitHub's immutable owner and repository IDs; it must match the repository's OIDC settings. Select **Other issuer** if the GitHub Actions scenario cannot generate that exact subject.
3. Assign the service principal the **Artifact Signing Certificate Profile Signer** role on the `OLEDbar` account.
4. Create the GitHub `release` environment and add these environment secrets:

    - `AZURE_CLIENT_ID`
    - `AZURE_TENANT_ID`
    - `AZURE_SUBSCRIPTION_ID`

For stronger release protection, require approval for the `release` environment and restrict who can create version tags.

Previous releases remain unchanged and unsigned. SmartScreen reputation can take time to build even for signed files.

## License

[MIT](LICENSE)
