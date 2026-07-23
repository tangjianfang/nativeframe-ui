// Screenshot helper — launch one sample exe, find its top-level window, dump it as a BMP.
// Minimal, no framework dependencies. Built as a standalone console app.

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <filesystem>
#include <vector>
#include <cstdio>

namespace fs = std::filesystem;

static BOOL CALLBACK find_window_by_pid(HWND hwnd, LPARAM lparam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    auto* ctx = reinterpret_cast<std::pair<DWORD, HWND>*>(lparam);
    if (ctx->first == pid && IsWindowVisible(hwnd) != FALSE) {
        wchar_t cls[256]{};
        GetClassNameW(hwnd, cls, 256);
        // Only top-level non-owned windows; main app windows.
        if (GetWindow(hwnd, GW_OWNER) == nullptr) {
            ctx->second = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

static bool save_window_bmp(HWND hwnd, const fs::path& path) {
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    HDC wnd_dc = GetDC(hwnd);
    HDC mem_dc = CreateCompatibleDC(wnd_dc);
    HBITMAP bmp = CreateCompatibleBitmap(wnd_dc, w, h);
    HGDIOBJ old = SelectObject(mem_dc, bmp);

    // PrintWindow with PW_RENDERFULLCONTENT captures even layered windows.
    BOOL ok = PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT);
    SelectObject(mem_dc, old);
    DeleteDC(mem_dc);
    ReleaseDC(hwnd, wnd_dc);

    if (!ok) {
        DeleteObject(bmp);
        return false;
    }

    // Save as BMP (no GDI+ dependency, just write BITMAPFILEHEADER + bits).
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<BYTE> pixels(static_cast<size_t>(w) * h * 4);
    HDC screen_dc = GetDC(nullptr);
    GetDIBits(screen_dc, bmp, 0, h, pixels.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, screen_dc);

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"wb") != 0 || fp == nullptr) {
        DeleteObject(bmp);
        return false;
    }

    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + static_cast<DWORD>(pixels.size());

    fwrite(&bfh, sizeof(bfh), 1, fp);
    fwrite(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
    fwrite(pixels.data(), 1, pixels.size(), fp);
    fclose(fp);
    DeleteObject(bmp);
    return true;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        wprintf(L"usage: screenshot.exe <sample.exe> <output.bmp> [wait_ms=2000]\n");
        return 1;
    }
    std::wstring exe = argv[1];
    fs::path out = argv[2];
    int wait_ms = argc >= 4 ? _wtoi(argv[3]) : 2000;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exe + L"\"";
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        wprintf(L"CreateProcessW failed: %lu\n", GetLastError());
        return 2;
    }

    Sleep(static_cast<DWORD>(wait_ms));

    std::pair<DWORD, HWND> ctx{pi.dwProcessId, nullptr};
    EnumWindows(find_window_by_pid, reinterpret_cast<LPARAM>(&ctx));

    if (ctx.second == nullptr) {
        wprintf(L"no top-level window found for pid %lu\n", pi.dwProcessId);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 3;
    }

    bool ok = save_window_bmp(ctx.second, out);
    wprintf(L"hwnd=%p saved=%s -> %s\n", ctx.second, ok ? "ok" : "FAIL", out.c_str());

    // Gracefully shut down: send WM_CLOSE, wait, then force.
    SendMessageW(ctx.second, WM_CLOSE, 0, 0);
    Sleep(500);
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return ok ? 0 : 4;
}