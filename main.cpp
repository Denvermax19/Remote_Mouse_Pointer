#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwmapi.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <random>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "bcrypt.lib")

// Direct2D Globals
ID2D1Factory* d2dFactory = nullptr;
ID2D1HwndRenderTarget* renderTarget = nullptr;
ID2D1SolidColorBrush* redBrush = nullptr;
ID2D1SolidColorBrush* blueBrush = nullptr;

// State Variables
std::atomic<float> dotX(-100.0f);
std::atomic<float> dotY(-100.0f);
std::atomic<bool> isVisible(false);
bool isTeacher = false;
bool wasMinimized = false;

ix::WebSocket webSocket;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_ERASEBKGND) return 1;

    // Windows rebuilds a layered window's colour-key hit-test shape lazily. After a
    // size or activation change the shape goes stale: the title bar still paints, but
    // mouse input falls through to whatever is behind the window, so the window can no
    // longer be grabbed to move or resize. Re-applying the attributes rebuilds it.
    if (isTeacher) {
        bool refresh = (uMsg == WM_EXITSIZEMOVE) ||
                       (uMsg == WM_NCACTIVATE) ||
                       (uMsg == WM_DISPLAYCHANGE) ||
                       (uMsg == WM_SIZE && wParam == SIZE_MAXIMIZED);
        if (uMsg == WM_SIZE && wParam == SIZE_MINIMIZED) wasMinimized = true;
        else if (uMsg == WM_SIZE && wasMinimized) { wasMinimized = false; refresh = true; }
        if (refresh) SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    }

    // Direct2D Surface Resizing when you resize the window
    if (uMsg == WM_SIZE && renderTarget) {
        UINT w = LOWORD(lParam);
        UINT h = HIWORD(lParam);
        if (w > 0 && h > 0) {
            renderTarget->Resize(D2D1::SizeU(w, h));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    if (uMsg == WM_PAINT) {
        if (!renderTarget) { ValidateRect(hwnd, NULL); return 0; }
        renderTarget->BeginDraw();

        if (isTeacher) {
            // 1. Clear interior to 100% transparent glass
            renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));

            // 2. Draw clean 2px boundary line
            RECT rc;
            GetClientRect(hwnd, &rc);
            D2D1_RECT_F border = D2D1::RectF(1.0f, 1.0f, (float)rc.right - 1.0f, (float)rc.bottom - 1.0f);
            renderTarget->DrawRectangle(border, blueBrush, 2.0f);

            // 3. Local Red Dot Preview for Teacher when holding Ctrl
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F((float)pt.x, (float)pt.y), 12.0f, 12.0f), redBrush);
            }
        } else {
            // Student: Clear background and render red laser dot
            renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
            if (isVisible) {
                renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 15.0f, 15.0f), redBrush);
            }
        }

        renderTarget->EndDraw();
        ValidateRect(hwnd, NULL);
        return 0;
    }

    if (uMsg == WM_TIMER) {
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void StartWebSocket(const std::string& url, const std::string& code, const std::string& role, int screenW, int screenH) {
    webSocket.setUrl(url);

    webSocket.setOnMessageCallback([&, role, screenW, screenH](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "[NET] Connected to server!\n";
            json regMsg;
            regMsg["type"] = (role == "student") ? "create_room" : "join_room";
            regMsg["code"] = code;
            webSocket.send(regMsg.dump());
        }
        else if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto data = json::parse(msg->str);
                std::string type = data.value("type", "");

                if (type == "peer_connected") {
                    std::cout << "\n[SUCCESS] Partner paired and connected!\n";
                } 
                else if (type == "signal" && role == "student") {
                    bool vis = data.value("visible", false);
                    isVisible = vis;
                    if (vis) {
                        float relX = data.value("x", 0.0f);
                        float relY = data.value("y", 0.0f);
                        dotX = relX * screenW;
                        dotY = relY * screenH;
                    }
                }
            } catch (...) {}
        }
    });

    webSocket.start();
}

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    std::cout << "========================================\n";
    std::cout << "    ScreenPointer C++ Native Client     \n";
    std::cout << "========================================\n\n";

    std::cout << "Select Role:\n1. Student (Generate Code)\n2. Teacher (Enter Code)\nChoice: ";
    int choice = 1;
    std::cin >> choice;

    std::string role = (choice == 1) ? "student" : "teacher";
    isTeacher = (role == "teacher");

    std::string code;
    if (role == "student") {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        code = std::to_string(dis(gen));
        std::cout << "\n>>> YOUR 6-DIGIT CODE: [ " << code << " ] <<<\n";
    } else {
        std::cout << "\nEnter Student's 6-Digit Code: ";
        std::cin >> code;
    }

    std::string serverUrl;
    std::cout << "Enter Render WebSocket URL: ";
    std::cin >> serverUrl;

    if (serverUrl.rfind("https://", 0) == 0) serverUrl.replace(0, 8, "wss://");
    else if (serverUrl.rfind("wss://", 0) != 0) serverUrl = "wss://" + serverUrl;
    if (!serverUrl.empty() && serverUrl.back() == '/') serverUrl.pop_back();

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ScreenPointerNative";
    RegisterClassW(&wc);

    HWND hwnd = nullptr;

    if (!isTeacher) {
        // Student: Fullscreen click-through overlay
        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"StudentOverlay", WS_POPUP,
            0, 0, screenW, screenH,
            NULL, NULL, hInstance, NULL
        );
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    } else {
        // Teacher: Standard resizable window with title bar
        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED,
            wc.lpszClassName, L"Align Over Google Meet Screen Share", WS_OVERLAPPEDWINDOW,
            100, 100, 850, 600,
            NULL, NULL, hInstance, NULL
        );
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    }

    // Direct2D Setup
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
    d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(screenW, screenH)),
        &renderTarget
    );

    renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f), &redBrush);
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.6f, 1.0f, 1.0f), &blueBrush);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetTimer(hwnd, 1, 16, NULL); // 60 FPS

    StartWebSocket(serverUrl, code, role, screenW, screenH);

    // Teacher Input Transmitter Loop
    std::thread inputThread([&, hwnd, role]() {
        bool lastState = false;
        while (true) {
            if (isTeacher) {
                bool isCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

                POINT pt;
                GetCursorPos(&pt);

                // Get exact client viewing area (excludes title bar and borders!)
                RECT clientRc;
                GetClientRect(hwnd, &clientRc);
                POINT topLeft = { clientRc.left, clientRc.top };
                ClientToScreen(hwnd, &topLeft);

                int w = clientRc.right - clientRc.left;
                int h = clientRc.bottom - clientRc.top;

                bool inside = (pt.x >= topLeft.x && pt.x <= (topLeft.x + w) &&
                               pt.y >= topLeft.y && pt.y <= (topLeft.y + h));

                bool shouldSend = isCtrl && inside && (w > 10) && (h > 10);

                if (shouldSend) {
                    float relX = (float)(pt.x - topLeft.x) / (float)w;
                    float relY = (float)(pt.y - topLeft.y) / (float)h;

                    json sig;
                    sig["type"] = "signal";
                    sig["x"] = relX;
                    sig["y"] = relY;
                    sig["visible"] = true;
                    webSocket.send(sig.dump());
                    lastState = true;
                } else if (lastState) {
                    json sig;
                    sig["type"] = "signal";
                    sig["visible"] = false;
                    webSocket.send(sig.dump());
                    lastState = false;
                }
            }
            Sleep(16); // 60 Hz transmission
        }
    });
    inputThread.detach();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    webSocket.stop();
    WSACleanup();
    return 0;
}