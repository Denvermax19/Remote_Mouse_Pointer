#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>

// windows.h defines DrawText as a macro to DrawTextW, which would rewrite both the
// ID2D1RenderTarget::DrawText declaration and our call to it. Must go before d2d1.h.
#undef DrawText

#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
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
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "bcrypt.lib")

// Direct2D Globals
ID2D1Factory* d2dFactory = nullptr;
ID2D1HwndRenderTarget* renderTarget = nullptr;
ID2D1SolidColorBrush* redBrush = nullptr;
ID2D1SolidColorBrush* blueBrush = nullptr;
ID2D1SolidColorBrush* chromeBrush = nullptr;
ID2D1SolidColorBrush* titleBgBrush = nullptr;
ID2D1SolidColorBrush* glyphBrush = nullptr;
ID2D1SolidColorBrush* textBrush = nullptr;

// DirectWrite, for the caption text of our own title bar
IDWriteFactory* dwriteFactory = nullptr;
IDWriteTextFormat* titleFormat = nullptr;

// Client-side handles, in client pixels. They sit exactly on the edge of the donut
// hole, so these are the only client pixels that still belong to the window.
const int GRIP = 6;        // ring thickness -> HTCAPTION / HTLEFT / ... / HTBOTTOM
const int TITLE_H = 36;    // our own title bar (the system caption is removed)
const int BTN_W = 40;      // caption button width
const int DOT_SIZE = 24;   // red-dot preview popup, region-clipped to a circle
const int DOT_R = DOT_SIZE / 2;

// State Variables
std::atomic<float> dotX(-100.0f);
std::atomic<float> dotY(-100.0f);
std::atomic<bool> isVisible(false);
bool isTeacher = false;

// Caption buttons. Static terminal style: the three glyphs are always drawn,
// white strokes on the title band, no hover background (user's reference:
// Windows Terminal - Screenshot 3.png). Geometry enum doubles as the button id
// for hit-testing.
enum { BTN_MIN = 0, BTN_MAX = 1, BTN_CLOSE = 2, BTN_NONE = -1 };

ix::WebSocket webSocket;

HWND g_dotWnd = nullptr; // small popup that shows the teacher's Ctrl red-dot preview

// Keyed on class name, not on g_dotWnd: during CreateWindowExW the global is still
// null, so WM_SIZE / WM_PAINT arriving mid-creation would otherwise be treated as
// belonging to the teacher window.
bool IsDotWnd(HWND hwnd) {
    wchar_t cls[32] = { 0 };
    return GetClassNameW(hwnd, cls, 32) > 0 && lstrcmpW(cls, L"ScreenPointerDot") == 0;
}

// Band sizes for the client-side handles, shared by WM_PAINT, WM_NCHITTEST and the
// donut region so the painted pixels, the hit-test logic and the hole in the window
// can never disagree. `strip` is now our own title bar; `grip` is the resize ring.
void HandleBands(int w, int h, int& strip, int& grip) {
    grip = GRIP;
    if (w < grip * 6) grip = w / 12;
    if (h < grip * 6) grip = h / 12;
    if (grip < 2) grip = 2;
    strip = TITLE_H;
    if (strip > h / 3) strip = h / 3;
    if (strip < grip * 2) strip = grip * 2;
}

const int BTN_H = 24;

// Our own min/max/close buttons, right-aligned inside the title bar. The rects are
// derived from exactly the same strip/grip numbers WM_NCHITTEST and WM_PAINT use, so
// the hit area, the click area and the drawn area are always identical.
void CaptionButtons(int w, int strip, int grip, RECT r[3]) {
    int top = (strip - BTN_H) / 2;
    if (top < grip) top = grip;
    if (top + BTN_H > strip) top = strip - BTN_H;
    if (top < 0) top = 0;
    int b = top + BTN_H;
    int off = grip;                     // keep clear of the blue border stroke
    r[BTN_MIN]   = { w - 3 * BTN_W - off, top, w - 2 * BTN_W - off, b };
    r[BTN_MAX]   = { w - 2 * BTN_W - off, top, w - BTN_W - off,     b };
    r[BTN_CLOSE] = { w - BTN_W - off,     top, w - off,             b };
}

int HitCaptionButton(int x, int y, int w, int strip, int grip) {
    if (strip <= 0) return BTN_NONE;
    RECT r[3];
    CaptionButtons(w, strip, grip, r);
    POINT p = { x, y };
    for (int i = 0; i < 3; i++) if (PtInRect(&r[i], p)) return i;
    return BTN_NONE;
}

// Min / max / restore / close glyphs, drawn as strokes inside the button rect.
void DrawGlyph(ID2D1RenderTarget* rt, const RECT& b, int idx, bool zoomed) {
    float l = (float)b.left, r = (float)b.right;
    float t = (float)b.top, bo = (float)b.bottom;
    float cx = (l + r) * 0.5f, cy = (t + bo) * 0.5f;
    float s = 6.0f;
    ID2D1SolidColorBrush* br = glyphBrush;

    if (idx == BTN_MIN) {
        rt->DrawLine(D2D1::Point2F(cx - s, cy), D2D1::Point2F(cx + s, cy), br, 2.0f);
    } else if (idx == BTN_MAX) {
        if (zoomed) {
            // restore: two offset squares
            rt->DrawRectangle(D2D1::RectF(cx - s + 2.0f, cy - s, cx + s, cy + s - 2.0f), br, 2.0f);
            rt->DrawRectangle(D2D1::RectF(cx - s, cy - s + 2.0f, cx + s - 2.0f, cy + s), br, 2.0f);
        } else {
            rt->DrawRectangle(D2D1::RectF(cx - s, cy - s, cx + s, cy + s), br, 2.0f);
        }
    } else {
        float d = 5.4f;
        rt->DrawLine(D2D1::Point2F(cx - d, cy - d), D2D1::Point2F(cx + d, cy + d), br, 2.0f);
        rt->DrawLine(D2D1::Point2F(cx + d, cy - d), D2D1::Point2F(cx - d, cy + d), br, 2.0f);
    }
}

// The teacher window no longer uses WS_EX_LAYERED / LWA_COLORKEY. That colour-key
// hit-test shape is what went stale after a move/resize/maximize, killing the native
// title bar, the frame and every client-side handle at the same time (SESSION_LOG.md).
// A window region is a different mechanism: pixels outside it are simply not part of
// the window, so they never hit-test and never go stale. Region = full window rect
// minus a hole over the interior, which keeps the non-client caption + frame (always
// hittestable now) and the painted drag strip / resize grips.
void ApplyDonutRegion(HWND hwnd) {
    RECT wr, cr;
    if (!GetWindowRect(hwnd, &wr)) return;
    if (!GetClientRect(hwnd, &cr)) return;
    int cw = cr.right - cr.left;
    int ch = cr.bottom - cr.top;
    if (cw <= 0 || ch <= 0) return;

    POINT tl = { cr.left, cr.top };
    if (!ClientToScreen(hwnd, &tl)) return;
    int ox = tl.x - wr.left;   // client origin, in window coordinates
    int oy = tl.y - wr.top;

    int strip, grip;
    HandleBands(cw, ch, strip, grip);
    if (IsZoomed(hwnd)) grip = 0;   // maximized: no inert ring eating the screen edges
    int l = ox + grip;
    int t = oy + strip;
    int r = ox + cw - grip;
    int b = oy + ch - grip;
    if (r - l < 1 || b - t < 1) return;   // too small: keep the previous region

    HRGN outer = CreateRectRgn(0, 0, wr.right - wr.left, wr.bottom - wr.top);
    HRGN hole = CreateRectRgn(l, t, r, b);
    if (outer && hole) {
        CombineRgn(outer, outer, hole, RGN_DIFF);
        if (!SetWindowRgn(hwnd, outer, TRUE)) DeleteObject(outer);   // system owns it on success
    } else if (outer) {
        DeleteObject(outer);
    }
    if (hole) DeleteObject(hole);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Red-dot preview popup: GDI only, shares nothing with the Direct2D target.
    if (IsDotWnd(hwnd)) {
        if (uMsg == WM_ERASEBKGND) return 1;
        if (uMsg == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(RGB(220, 40, 40));
            if (brush) {
                FillRect(hdc, &rc, brush);
                DeleteObject(brush);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_ERASEBKGND) return 1;

    // Custom chrome: the system caption is removed, the client fills the whole
    // window, and we draw our own title bar (WM_PAINT) with our own buttons.
    // Without this the native min/max/close stay permanently lit and can only be
    // un-lit by hovering them, which is exactly what the user complained about.
    // wParam TRUE: rgrc[0] is left untouched == the window rect, so the client
    // fills the whole window (no border, no caption).
    if (uMsg == WM_NCCALCSIZE && isTeacher && !IsDotWnd(hwnd)) return 0;

    // Maximized windows with no non-client area would cover the taskbar; pin them
    // to the monitor work area instead.
    if (uMsg == WM_GETMINMAXINFO && isTeacher && !IsDotWnd(hwnd)) {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (mon && GetMonitorInfoW(mon, &mi)) {
            // Client == window, so the maximized rect must be exactly the work area
            // or we would paint over the taskbar (the system frame used to shrink it).
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
            mmi->ptMinTrackSize.x = 260;
            mmi->ptMinTrackSize.y = 160;
            return 0;
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    // Caption button click. The buttons live in the title bar, which is HTCAPTION,
    // so the press arrives as WM_LBUTTONDOWN while the pointer is over them.
    if (uMsg == WM_LBUTTONDOWN && isTeacher && !IsDotWnd(hwnd)) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w > 0 && h > 0) {
            int strip, grip;
            HandleBands(w, h, strip, grip);
            int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
            int btn = (y < strip) ? HitCaptionButton(x, y, w, strip, grip) : BTN_NONE;
            if (btn != BTN_NONE) {
                if (btn == BTN_MIN) {
                    SendMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                } else if (btn == BTN_MAX) {
                    SendMessageW(hwnd, WM_SYSCOMMAND,
                                 IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
                } else {
                    SendMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                }
                return 0;
            }
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    // Direct2D Surface Resizing when you resize the window
    if (uMsg == WM_SIZE && isTeacher && (!IsDotWnd(hwnd))) ApplyDonutRegion(hwnd);

    if (uMsg == WM_SIZE && renderTarget && (!IsDotWnd(hwnd))) {
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
            // 1. The donut hole already removed the interior from the window, so
            //    filling the whole client leaves exactly the parts that belong to
            //    us: the title bar band plus a thin ring on the other three sides.
            renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));

            RECT rc;
            GetClientRect(hwnd, &rc);
            float w = (float)rc.right;
            float h = (float)rc.bottom;
            renderTarget->FillRectangle(D2D1::RectF(0.0f, 0.0f, w, h), chromeBrush);

            int strip, grip;
            HandleBands(rc.right, rc.bottom, strip, grip);

            // 2. Our own title bar: dark band, caption text, and the three caption
            //    glyphs, always drawn (terminal style - Screenshot 3.png).
            renderTarget->FillRectangle(D2D1::RectF(0.0f, 0.0f, w, (float)strip), titleBgBrush);

            if (titleFormat && textBrush) {
                wchar_t title[128] = { 0 };
                GetWindowTextW(hwnd, title, 128);
                int n = lstrlenW(title);
                RECT r3[3];
                CaptionButtons(rc.right, strip, grip, r3);
                float right = (float)(r3[BTN_MIN].left - 8);
                if (n > 0 && right > 8.0f) {
                    D2D1_RECT_F tr = D2D1::RectF(8.0f, 0.0f, right, (float)strip);
                    renderTarget->DrawText(title, n, titleFormat, tr, textBrush);
                }
            }

            // Terminal-style caption buttons: always visible, plain white
            // strokes, no hover state. Nothing here depends on the pointer.
            {
                RECT r3[3];
                CaptionButtons(rc.right, strip, grip, r3);
                static bool s_uiReported = false;
                if (!s_uiReported) {
                    s_uiReported = true;
                    float dpiX = 0.0f, dpiY = 0.0f;
                    renderTarget->GetDpi(&dpiX, &dpiY);
                    std::cout << "[UI] dpi=" << dpiX << " client=" << (int)w << "x" << (int)h
                              << " strip=" << strip << " grip=" << grip
                              << " btns=" << r3[BTN_MIN].left << ".." << r3[BTN_CLOSE].right << "\n";
                }
                bool zoomed = IsZoomed(hwnd) != 0;
                for (int i = 0; i < 3; i++)
                    DrawGlyph(renderTarget, r3[i], i, zoomed);
            }

            // 3. Boundary line. Its top edge sits flush against the bottom of our
            //    title bar, so it frames the overlay hole instead of cutting across
            //    the caption band (the middle of that top edge is over the hole and
            //    is clipped away by the region anyway).
            D2D1_RECT_F border = D2D1::RectF(1.0f, (float)strip - 1.0f, w - 1.0f, h - 1.0f);
            renderTarget->DrawRectangle(border, blueBrush, 2.0f);

            // 4. The Ctrl red-dot preview is drawn by g_dotWnd, not here: this window
            //    has a hole through the middle, so it would be clipped away.
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
        // Red-dot preview: a tiny region-clipped popup that follows the cursor while
        // Ctrl is held over the teacher's client area. Separate window, because the
        // donut hole would clip anything drawn on the teacher window itself.
        if (isTeacher && g_dotWnd) {
            bool held = false;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                POINT pt;
                GetCursorPos(&pt);
                RECT cr;
                GetClientRect(hwnd, &cr);
                POINT tl = { cr.left, cr.top }, br = { cr.right, cr.bottom };
                ClientToScreen(hwnd, &tl);
                ClientToScreen(hwnd, &br);
                if (pt.x >= tl.x && pt.x < br.x && pt.y >= tl.y && pt.y < br.y) {
                    int strip, grip;
                    HandleBands(cr.right - cr.left, cr.bottom - cr.top, strip, grip);
                    if (pt.y >= tl.y + strip) {          // over our title bar: not overlay area
                        held = true;
                        SetWindowPos(g_dotWnd, HWND_TOPMOST, pt.x - DOT_R, pt.y - DOT_R,
                                     DOT_SIZE, DOT_SIZE, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                    }
                }
            }
            if (!held && IsWindowVisible(g_dotWnd)) ShowWindow(g_dotWnd, SW_HIDE);
        }


        InvalidateRect(hwnd, NULL, FALSE);   // repaints the static title bar too
        return 0;
    }

    if (uMsg == WM_NCHITTEST) {
        if (!isTeacher) return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);

        if (pt.x < rc.left || pt.x >= rc.right || pt.y < rc.top || pt.y >= rc.bottom)
            return HTCLIENT; // no non-client area left

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) return HTCLIENT;
        int strip, grip;
        HandleBands(w, h, strip, grip);

        int x = pt.x - rc.left;
        int y = pt.y - rc.top;
        bool zoomed = IsZoomed(hwnd) != 0;

        if (y < strip) {
            // Our caption buttons take the press themselves (client messages);
            // the rest of the title bar drags the window.
            if (HitCaptionButton(x, y, w, strip, grip) != BTN_NONE) return HTCLIENT;
            if (!zoomed) {
                if (y < grip) {
                    if (x < grip) return HTTOPLEFT;
                    if (x >= w - grip) return HTTOPRIGHT;
                    return HTTOP;
                }
            }
            return HTCAPTION;
        }
        if (zoomed) return HTCLIENT;   // maximized: no resizing

        if (y >= h - grip) {                   // bottom edge / bottom corners
            if (x < grip)       return HTBOTTOMLEFT;
            if (x >= w - grip)  return HTBOTTOMRIGHT;
            return HTBOTTOM;
        }
        if (x < grip)         return HTLEFT;
        if (x >= w - grip)    return HTRIGHT;
        return HTCLIENT;
    }

    if (uMsg == WM_DESTROY) {
        if (g_dotWnd && IsWindow(g_dotWnd)) DestroyWindow(g_dotWnd);
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
    wc.style = CS_DBLCLKS;   // double-click on our title bar -> WM_NCLBUTTONDBLCLK -> maximize
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
        // Teacher: standard resizable window. NOT layered any more - a window region
        // does the click-through instead of a colour-key hit mask (ApplyDonutRegion).
        hwnd = CreateWindowExW(
            WS_EX_TOPMOST,
            wc.lpszClassName, L"Align Over Google Meet Screen Share", WS_OVERLAPPEDWINDOW,
            100, 100, 850, 600,
            NULL, NULL, hInstance, NULL
        );

        // Red-dot preview popup: tiny, region-clipped to a circle, click-through
        // (WS_EX_TRANSPARENT) and non-activating, so it never steals the drag.
        WNDCLASSW wcd = { 0 };
        wcd.lpfnWndProc = WindowProc;
        wcd.hInstance = hInstance;
        wcd.lpszClassName = L"ScreenPointerDot";
        RegisterClassW(&wcd);
        g_dotWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wcd.lpszClassName, L"", WS_POPUP,
            0, 0, DOT_SIZE, DOT_SIZE, NULL, NULL, hInstance, NULL
        );
        HRGN dotRgn = CreateEllipticRgn(0, 0, DOT_SIZE, DOT_SIZE);
        if (dotRgn) {
            if (!SetWindowRgn(g_dotWnd, dotRgn, TRUE)) DeleteObject(dotRgn);
        }
    }

    // Direct2D Setup
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
    d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(screenW, screenH)),
        &renderTarget
    );
    // The process is Per-Monitor V2 DPI aware, so GetClientRect / SetWindowRgn /
    // cursor maths all speak PHYSICAL pixels, but an HWND target defaults to the
    // monitor DPI (120 on a 125% display) and scales every drawing coordinate
    // by 1.25: the caption button rects (computed at x~697..817) were drawn at
    // 871..1021 px - past the 817 px client edge - which is why the glyphs were
    // never visible in ANY build while the click hit-test (raw pixels) worked.
    // Force 96 DPI so 1 DIP == 1 client pixel everywhere.
    if (renderTarget) renderTarget->SetDpi(96.0f, 96.0f);

    renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f), &redBrush);
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.6f, 1.0f, 1.0f), &blueBrush);
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.16f, 0.18f, 0.22f, 1.0f), &chromeBrush);
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.13f, 0.16f, 1.0f), &titleBgBrush);
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &glyphBrush);
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.78f, 0.80f, 0.83f, 1.0f), &textBrush);

    if (isTeacher) {
        if (SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                          (IUnknown**)&dwriteFactory)) && dwriteFactory) {
            dwriteFactory->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &titleFormat);
            if (titleFormat) {
                titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
        }
    }

    if (isTeacher) ApplyDonutRegion(hwnd);
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

                // Client area now starts at the top of the window (no system caption),
                // so our own title bar has to be trimmed off before mapping to the
                // student's screen - otherwise a Ctrl hover there sends a dot at the
                // very top of the screen instead of where the share really starts.
                RECT clientRc;
                GetClientRect(hwnd, &clientRc);
                POINT topLeft = { clientRc.left, clientRc.top };
                ClientToScreen(hwnd, &topLeft);

                int w = clientRc.right - clientRc.left;
                int h = clientRc.bottom - clientRc.top;
                int strip, grip;
                HandleBands(w, h, strip, grip);
                int top = strip;
                int mapH = h - strip;

                bool inside = (pt.x >= topLeft.x && pt.x <= (topLeft.x + w) &&
                               pt.y >= (topLeft.y + top) && pt.y <= (topLeft.y + h));

                bool shouldSend = isCtrl && inside && (w > 10) && (mapH > 10);

                if (shouldSend) {
                    float relX = (float)(pt.x - topLeft.x) / (float)w;
                    float relY = (float)(pt.y - topLeft.y - top) / (float)mapH;

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