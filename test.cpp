#include <windows.h>
#include <d2d1.h>
#include <dwmapi.h>
#include <stdio.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")

ID2D1Factory* d2dFactory = nullptr;
ID2D1HwndRenderTarget* renderTarget = nullptr;
ID2D1SolidColorBrush* redBrush = nullptr;

float dotX = -100.0f;
float dotY = -100.0f;
bool isVisible = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        renderTarget->BeginDraw();
        // Clear background to 100% transparent
        renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        
        // Draw dot ONLY if Ctrl is pressed
        if (isVisible) {
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 15.0f, 15.0f);
            renderTarget->FillEllipse(ellipse, redBrush);
        }
        
        renderTarget->EndDraw();
        ValidateRect(hwnd, NULL);
        return 0;
    }
    if (uMsg == WM_TIMER) {
        // 1. Check if Ctrl key (0x11 = VK_CONTROL) is held down globally
        isVisible = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

        // 2. Track mouse position
        POINT pt;
        GetCursorPos(&pt);
        dotX = (float)pt.x;
        dotY = (float)pt.y;

        // 3. Trigger redraw
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int main() {
    printf("[INFO] Running Ctrl-Tracking Pointer...\n");

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TestOverlayClass";
    RegisterClassW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"TestOverlay", WS_POPUP,
        0, 0, screenW, screenH,
        NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
    d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT, 
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        ),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(screenW, screenH)),
        &renderTarget
    );
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f), &redBrush);

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // 60 Hz smooth refresh rate (~16ms)
    SetTimer(hwnd, 1, 16, NULL);

    printf("[INFO] Ready! Hold the Ctrl key and move your mouse anywhere on your screen.\n");

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}