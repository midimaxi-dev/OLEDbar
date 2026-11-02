#include "app/tray_icon.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace dimmer {

bool TrayIcon::Add(HWND owner, UINT callbackMessage) {
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = 1;
    data_.uCallbackMessage = callbackMessage;
    return Readd();
}

bool TrayIcon::Readd() {
    if (!data_.hWnd) {
        return false;
    }
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (added_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
    return added_;
}

void TrayIcon::Remove() {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

void TrayIcon::Update(wil::unique_hicon icon, const std::wstring& tip) {
    // Keep the previous icon alive until the shell has switched to the new one.
    const wil::unique_hicon previous = std::exchange(icon_, std::move(icon));
    data_.hIcon = icon_.get();
    wcsncpy_s(data_.szTip, tip.c_str(), _TRUNCATE);
    if (added_) {
        data_.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        Shell_NotifyIconW(NIM_MODIFY, &data_);
    }
}

void TrayIcon::Notify(const std::wstring& title, const std::wstring& text, bool warning) {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW notification = data_;
    notification.uFlags = NIF_INFO;
    notification.dwInfoFlags = warning ? NIIF_WARNING : NIIF_INFO;
    wcsncpy_s(notification.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(notification.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &notification);
}

namespace {

// 32-bit ARGB (straight alpha) square bitmap that is turned into an icon.
class Canvas {
public:
    explicit Canvas(int size) : size_(size) {
        BITMAPV5HEADER header{};
        header.bV5Size = sizeof(header);
        header.bV5Width = size;
        header.bV5Height = -size;  // Top-down rows.
        header.bV5Planes = 1;
        header.bV5BitCount = 32;
        header.bV5Compression = BI_BITFIELDS;
        header.bV5RedMask = 0x00FF0000;
        header.bV5GreenMask = 0x0000FF00;
        header.bV5BlueMask = 0x000000FF;
        header.bV5AlphaMask = 0xFF000000;
        void* bits = nullptr;
        color_.reset(CreateDIBSection(nullptr, reinterpret_cast<const BITMAPINFO*>(&header), DIB_RGB_COLORS, &bits,
                                      nullptr, 0));
        if (color_ && bits) {
            pixels_ = std::span<std::uint32_t>(static_cast<std::uint32_t*>(bits), static_cast<size_t>(size) * size);
            std::ranges::fill(pixels_, 0u);
        }
    }

    int Size() const noexcept { return size_; }
    int Grid(int sixteenths) const noexcept { return sixteenths * size_ / 16; }

    void Fill(int left, int top, int right, int bottom, std::uint32_t argb) {
        if (pixels_.empty()) {
            return;
        }
        for (int y = std::max(top, 0); y < std::min(bottom, size_); ++y) {
            for (int x = std::max(left, 0); x < std::min(right, size_); ++x) {
                pixels_[static_cast<size_t>(y) * size_ + x] = argb;
            }
        }
    }

    wil::unique_hicon ToIcon() {
        if (pixels_.empty()) {
            return {};
        }
        std::vector<BYTE> maskBits(static_cast<size_t>((size_ + 15) / 16) * 2 * size_, 0);
        wil::unique_hbitmap mask(CreateBitmap(size_, size_, 1, 1, maskBits.data()));
        if (!mask) {
            return {};
        }
        ICONINFO info{};
        info.fIcon = TRUE;
        info.hbmMask = mask.get();
        info.hbmColor = color_.get();
        return wil::unique_hicon(CreateIconIndirect(&info));
    }

private:
    int size_;
    wil::unique_hbitmap color_;
    std::span<std::uint32_t> pixels_;
};

constexpr std::uint32_t Argb(std::uint32_t alpha, std::uint32_t rgb) noexcept {
    return (alpha << 24) | rgb;
}

// Screen outline with a taskbar strip; coordinates are in sixteenths of the icon size.
void DrawScreenGlyph(Canvas& canvas, std::uint32_t lineArgb, std::uint32_t stripArgb, int left, int top, int right,
                     int bottom, int stripTop) {
    const int stroke = std::max(1, canvas.Size() / 16);
    const int l = canvas.Grid(left);
    const int t = canvas.Grid(top);
    const int r = canvas.Grid(right);
    const int b = canvas.Grid(bottom);
    canvas.Fill(l, t, r, t + stroke, lineArgb);
    canvas.Fill(l, t, l + stroke, b, lineArgb);
    canvas.Fill(r - stroke, t, r, b, lineArgb);
    canvas.Fill(l, canvas.Grid(stripTop), r, b, stripArgb);
}

}  // namespace

wil::unique_hicon CreateStatusIcon(int size, bool lightTaskbar, bool active) {
    Canvas canvas(size);
    const std::uint32_t rgb = lightTaskbar ? 0x1C1C1C : 0xFFFFFF;
    const std::uint32_t lineAlpha = active ? 0xFF : 0x80;
    // Translucent strip = dimming active; whole icon faded = paused or disabled.
    DrawScreenGlyph(canvas, Argb(lineAlpha, rgb), Argb(active ? 0x70 : lineAlpha, rgb), 1, 2, 15, 14, 11);
    return canvas.ToIcon();
}

bool IsTaskbarLight() {
    DWORD value = 0;
    DWORD bytes = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &bytes) == ERROR_SUCCESS &&
           value != 0;
}

}  // namespace dimmer
