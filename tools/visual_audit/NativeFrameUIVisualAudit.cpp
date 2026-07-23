// NativeFrameUI visual-audit capture tool.
// Standalone Win32 console executable: Windows SDK, GDI, DWM, UxTheme, and WIC only.

#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <chrono>
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {

constexpr DWORD kWindowTimeoutMs = 15'000;
constexpr DWORD kExitTimeoutMs = 3'000;

struct HandleCloser {
    void operator()(HANDLE handle) const noexcept {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};
using UniqueHandle = std::unique_ptr<void, HandleCloser>;

struct ProcessGuard {
    UniqueHandle process;
    UniqueHandle thread;
    DWORD processId{};
    HWND window{};

    ~ProcessGuard() {
        if (!process) {
            return;
        }
        if (window != nullptr && IsWindow(window) != FALSE) {
            PostMessageW(window, WM_CLOSE, 0, 0);
        }
        if (WaitForSingleObject(process.get(), kExitTimeoutMs) == WAIT_TIMEOUT) {
            TerminateProcess(process.get(), 0);
            WaitForSingleObject(process.get(), 1'000);
        }
    }
};

std::wstring quoteArgument(std::wstring_view argument) {
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'\"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'\"');
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            backslashes = 0;
            quoted.push_back(ch);
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

std::wstring expectedTitleFragment(const fs::path& executable) {
    const std::wstring stem = executable.stem().wstring();
    constexpr std::wstring_view prefix = L"NativeFrameUI";
    if (stem.starts_with(prefix)) {
        return stem.substr(prefix.size());
    }
    return stem;
}

struct WindowSearch {
    DWORD processId{};
    std::wstring titleFragment;
    HWND fallback{};
    HWND match{};
};

BOOL CALLBACK findTopLevelWindow(HWND window, LPARAM parameter) noexcept {
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != search->processId || IsWindowVisible(window) == FALSE ||
        GetWindow(window, GW_OWNER) != nullptr) {
        return TRUE;
    }

    if (search->fallback == nullptr) {
        search->fallback = window;
    }

    wchar_t title[512]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    if (search->titleFragment.empty() ||
        wcsstr(title, search->titleFragment.c_str()) != nullptr) {
        search->match = window;
        return FALSE;
    }
    return TRUE;
}

HWND waitForWindow(HANDLE process, DWORD processId, const std::wstring& titleFragment) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kWindowTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
            return nullptr;
        }
        WindowSearch search{processId, titleFragment};
        EnumWindows(findTopLevelWindow, reinterpret_cast<LPARAM>(&search));
        if (search.match != nullptr || search.fallback != nullptr) {
            return search.match != nullptr ? search.match : search.fallback;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}

struct CaptionSearch {
    std::wstring_view caption;
    HWND match{};
};

BOOL CALLBACK findChildByCaption(HWND child, LPARAM parameter) noexcept {
    wchar_t className[32]{};
    if (GetClassNameW(child, className, static_cast<int>(std::size(className))) == 0 ||
        _wcsicmp(className, L"Button") != 0) {
        return TRUE;
    }

    auto* search = reinterpret_cast<CaptionSearch*>(parameter);
    wchar_t text[128]{};
    if (GetWindowTextW(child, text, static_cast<int>(std::size(text))) == 0) {
        return TRUE;
    }
    if (std::wstring_view{text} == search->caption) {
        search->match = child;
        return FALSE;
    }
    return TRUE;
}

HWND findButtonByCaption(HWND window, std::wstring_view caption) noexcept {
    CaptionSearch search{caption};
    EnumChildWindows(window, findChildByCaption, reinterpret_cast<LPARAM>(&search));
    return search.match;
}

bool requestInSampleTheme(HWND window, std::wstring_view theme) {
    // Treat captions as an in-sample theme contract only when the complete
    // Light/Dark/High Contrast selector is present. This avoids clicking an
    // unrelated button that happens to be named "Dark".
    const HWND lightButton = findButtonByCaption(window, L"Light");
    const HWND darkButton = findButtonByCaption(window, L"Dark");
    const HWND highContrastButton = findButtonByCaption(window, L"High Contrast");
    if (lightButton == nullptr || darkButton == nullptr || highContrastButton == nullptr) {
        return false;
    }

    const HWND target = theme == L"dark" ? darkButton :
                        theme == L"high_contrast" ? highContrastButton : lightButton;
    DWORD_PTR clickResult = 0;
    if (SendMessageTimeoutW(target, BM_CLICK, 0, 0, SMTO_ABORTIFHUNG,
                            1'000, &clickResult) == 0) {
        return false;
    }
    // BM_CLICK is synchronous: SendMessageTimeoutW returns only after the
    // target window has processed the click and posted any queued
    // InvalidateRect. A 250 ms settle here did no useful work and lengthened
    // every capture where an exact-caption child existed (at least all three
    // ThemeDemo modes, plus any sample with a "Light/Dark/HC" affordance);
    // PrintWindow issues its own WM_PAINT and flushes whatever was queued.
    return true;
}

BOOL CALLBACK applyThemeToChild(HWND child, LPARAM parameter) noexcept {
    const auto theme = static_cast<std::wstring_view*>(reinterpret_cast<void*>(parameter));
    if (*theme == L"dark") {
        SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
    } else if (*theme == L"light") {
        SetWindowTheme(child, L"Explorer", nullptr);
    } else {
        SetWindowTheme(child, L"", L"");
    }
    return TRUE;
}

void applyRequestedTheme(HWND window, std::wstring_view theme) {
    // Samples that understand --theme remain authoritative. These calls also make the
    // requested mode visible to standard non-client/common-control chrome where supported.
    if (theme == L"dark") {
        BOOL enabled = TRUE;
        if (FAILED(DwmSetWindowAttribute(window, 20, &enabled, sizeof(enabled)))) {
            DwmSetWindowAttribute(window, 19, &enabled, sizeof(enabled));
        }
        SetWindowTheme(window, L"DarkMode_Explorer", nullptr);
    } else if (theme == L"light") {
        BOOL enabled = FALSE;
        if (FAILED(DwmSetWindowAttribute(window, 20, &enabled, sizeof(enabled)))) {
            DwmSetWindowAttribute(window, 19, &enabled, sizeof(enabled));
        }
        SetWindowTheme(window, L"Explorer", nullptr);
    } else {
        SetWindowTheme(window, L"", L"");
    }
    auto mutableTheme = theme;
    EnumChildWindows(window, applyThemeToChild,
                     reinterpret_cast<LPARAM>(&mutableTheme));
    SendMessageTimeoutW(window, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 1'000, nullptr);
    RedrawWindow(window, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
}

bool savePng(HWND window, const fs::path& output, std::wstring& error) {
    RECT bounds{};
    if (GetWindowRect(window, &bounds) == FALSE) {
        error = L"GetWindowRect failed";
        return false;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) {
        error = L"window has empty bounds";
        return false;
    }

    HDC windowDc = GetDC(window);
    HDC memoryDc = CreateCompatibleDC(windowDc);
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(windowDc, &bitmapInfo, DIB_RGB_COLORS,
                                      &pixels, nullptr, 0);
    if (windowDc == nullptr || memoryDc == nullptr || bitmap == nullptr || pixels == nullptr) {
        if (bitmap != nullptr) DeleteObject(bitmap);
        if (memoryDc != nullptr) DeleteDC(memoryDc);
        if (windowDc != nullptr) ReleaseDC(window, windowDc);
        error = L"failed to create capture bitmap";
        return false;
    }

    HGDIOBJ previous = SelectObject(memoryDc, bitmap);
    const BOOL printed = PrintWindow(window, memoryDc, PW_RENDERFULLCONTENT);
    SelectObject(memoryDc, previous);
    DeleteDC(memoryDc);
    ReleaseDC(window, windowDc);
    if (printed == FALSE) {
        DeleteObject(bitmap);
        error = L"PrintWindow failed";
        return false;
    }

    // PrintWindow/GDI leave the reserved byte undefined (normally zero). PNG treats
    // it as alpha, so make the captured desktop window fully opaque.
    auto* pixelBytes = static_cast<BYTE*>(pixels);
    const std::size_t pixelCount = static_cast<std::size_t>(width) *
                                   static_cast<std::size_t>(height);
    for (std::size_t index = 0; index < pixelCount; ++index) {
        pixelBytes[index * 4 + 3] = 0xFF;
    }

    std::error_code removeError;
    fs::remove(output, removeError);

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(output.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, &properties);
    if (SUCCEEDED(hr)) hr = frame->Initialize(properties.Get());
    if (SUCCEEDED(hr)) hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
    if (SUCCEEDED(hr) && format != GUID_WICPixelFormat32bppBGRA) {
        hr = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
    }
    const UINT stride = static_cast<UINT>(width) * 4U;
    const UINT imageSize = stride * static_cast<UINT>(height);
    if (SUCCEEDED(hr)) {
        hr = frame->WritePixels(static_cast<UINT>(height), stride, imageSize,
                                static_cast<BYTE*>(pixels));
    }
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();

    DeleteObject(bitmap);
    if (FAILED(hr)) {
        error = L"WIC PNG encoding failed (HRESULT 0x";
        wchar_t value[16]{};
        swprintf_s(value, L"%08X", static_cast<unsigned>(hr));
        error += value;
        error += L")";
        return false;
    }
    return true;
}

void printUsage() {
    std::wcerr << L"Usage: NativeFrameUIVisualAudit.exe <sample.exe> "
                  L"<light|dark|high_contrast> <output.png>\n";
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 4) {
        printUsage();
        return 1;
    }

    const fs::path executable = fs::absolute(argv[1]);
    const std::wstring theme = argv[2];
    const fs::path output = fs::absolute(argv[3]);
    if (!fs::is_regular_file(executable)) {
        std::wcerr << L"Sample executable not found: " << executable << L'\n';
        return 2;
    }
    if (theme != L"light" && theme != L"dark" && theme != L"high_contrast") {
        std::wcerr << L"Invalid theme mode: " << theme << L'\n';
        printUsage();
        return 3;
    }
    if (output.extension() != L".png") {
        std::wcerr << L"Output path must end in .png: " << output << L'\n';
        return 4;
    }

    std::error_code directoryError;
    fs::create_directories(output.parent_path(), directoryError);
    if (directoryError) {
        std::wcerr << L"Cannot create output directory: " << output.parent_path() << L'\n';
        return 5;
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        std::wcerr << L"CoInitializeEx failed\n";
        return 6;
    }

    std::wstring commandLine = quoteArgument(executable.wstring()) + L" --theme " +
                               quoteArgument(theme);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION processInfo{};
    std::wstring workingDirectory = executable.parent_path().wstring();
    if (CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                       CREATE_UNICODE_ENVIRONMENT, nullptr, workingDirectory.c_str(),
                       &startup, &processInfo) == FALSE) {
        std::wcerr << L"CreateProcessW failed with error " << GetLastError() << L'\n';
        CoUninitialize();
        return 7;
    }

    ProcessGuard process{UniqueHandle(processInfo.hProcess), UniqueHandle(processInfo.hThread),
                         processInfo.dwProcessId};
    WaitForInputIdle(process.process.get(), 5'000);
    process.window = waitForWindow(process.process.get(), process.processId,
                                   expectedTitleFragment(executable));
    if (process.window == nullptr) {
        DWORD exitCode = STILL_ACTIVE;
        GetExitCodeProcess(process.process.get(), &exitCode);
        std::wcerr << L"No visible top-level window appeared within " << kWindowTimeoutMs
                   << L" ms; process exit code=" << exitCode << L'\n';
        CoUninitialize();
        return 8;
    }

    ShowWindow(process.window, SW_RESTORE);
    SetForegroundWindow(process.window);
    const bool sampleHandledTheme = requestInSampleTheme(process.window, theme);
    if (!sampleHandledTheme) {
        applyRequestedTheme(process.window, theme);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1'000));

    std::wstring error;
    const bool saved = savePng(process.window, output, error);
    if (!saved) {
        std::wcerr << L"Capture failed: " << error << L'\n';
        CoUninitialize();
        return 9;
    }

    std::wcout << L"Captured " << executable.filename() << L" [" << theme << L"] -> "
               << output << L'\n';
    CoUninitialize();
    return 0;
}
