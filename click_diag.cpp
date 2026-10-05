#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <atomic>
#include <iostream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00020000L
#endif
#ifndef GUI_CARETBLINKING
#define GUI_CARETBLINKING 0x00000001
#endif
#ifndef GUI_INMENUMODE
#define GUI_INMENUMODE 0x00000002
#endif
#ifndef GUI_INMOVESIZE
#define GUI_INMOVESIZE 0x00000004
#endif
#ifndef GUI_INMENULOOP
#define GUI_INMENULOOP 0x00000008
#endif
#ifndef GUI_POPUPMENUMODE
#define GUI_POPUPMENUMODE 0x00000010
#endif
#ifndef GUI_SYSTEMMENUMODE
#define GUI_SYSTEMMENUMODE 0x00000020
#endif
#ifndef GUI_FOCUSABLE
#define GUI_FOCUSABLE 0x00000080
#endif
#ifndef GUI_INCONTRLMODE
#define GUI_INCONTRLMODE 0x00000100
#endif

static HHOOK              g_hook   = NULL;
static HWND               g_msgWnd = NULL;
static HANDLE             g_log    = INVALID_HANDLE_VALUE;
static HANDLE             g_wake   = NULL;
static HANDLE             g_worker = NULL;
static std::atomic<bool>  g_quit(false);
static unsigned long long g_start  = 0;
static int                g_clickCount = 0;

static CRITICAL_SECTION g_logCs;
static CRITICAL_SECTION g_qCs;

struct ClickRec { DWORD btn; POINT pt; DWORD flags; ULONGLONG at; };
static std::vector<ClickRec> g_pending;

// ---------------------------------------------------------------- logging
static std::wstring Stamp() {
    return L"[+" + std::to_wstring(GetTickCount64() - g_start) + L"ms] ";
}

static void Log(const std::wstring& s) {
    std::wstring line = Stamp() + s + L"\r\n";
    EnterCriticalSection(&g_logCs);
    std::wcout << line;
    std::wcout.flush();
    if (g_log != INVALID_HANDLE_VALUE) {
        int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), (int)line.size(), NULL, 0, NULL, NULL);
        if (n > 0) {
            std::string u((size_t)n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, line.c_str(), (int)line.size(), &u[0], n, NULL, NULL);
            DWORD wr = 0;
            WriteFile(g_log, u.data(), (DWORD)u.size(), &wr, NULL);
            FlushFileBuffers(g_log);
        }
    }
    OutputDebugStringW(line.c_str());
    LeaveCriticalSection(&g_logCs);
}

// ---------------------------------------------------------------- helpers
static std::wstring PtrStr(const void* p) {
    static const wchar_t* d = L"0123456789ABCDEF";
    size_t n = sizeof(void*) * 2;
    unsigned long long v = (unsigned long long)(ULONG_PTR)p;
    std::wstring s(n, L'0');
    for (size_t i = n; i-- > 0;) { s[i] = d[v & 0xF]; v >>= 4; }
    return s;
}

static std::wstring Hex32(DWORD v) {
    static const wchar_t* d = L"0123456789ABCDEF";
    std::wstring s(8, L'0');
    for (int i = 7; i >= 0; --i) { s[i] = d[v & 0xF]; v >>= 4; }
    return s;
}

static std::wstring RectStr(const RECT& r) {
    return L"(" + std::to_wstring(r.left) + L"," + std::to_wstring(r.top) + L")-(" +
           std::to_wstring(r.right) + L"," + std::to_wstring(r.bottom) + L") " +
           std::to_wstring(r.right - r.left) + L"x" + std::to_wstring(r.bottom - r.top);
}

static std::wstring BoolStr(bool v) { return v ? L"YES" : L"NO"; }

static std::wstring Clip(const std::wstring& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen) + L"...";
}

static std::wstring GetTitleW(HWND h) {
    int len = GetWindowTextLengthW(h);
    if (len <= 0) return L"(no title)";
    std::wstring s(len + 1, L'\0');
    int n = GetWindowTextW(h, &s[0], len + 1);
    s.resize(n > 0 ? n : 0);
    if (s.empty()) s = L"(no title)";
    return s;
}

static std::wstring GetClassW(HWND h) {
    wchar_t buf[256] = L"";
    GetClassNameW(h, buf, 256);
    return buf[0] ? buf : L"(?)";
}

static std::wstring ProcName(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"?";
    wchar_t buf[MAX_PATH] = L"";
    DWORD size = MAX_PATH;
    std::wstring name = L"?";
    if (QueryFullProcessImageNameW(h, 0, buf, &size)) {
        const wchar_t* p = buf;
        for (const wchar_t* q = buf; *q; ++q)
            if (*q == L'\\' || *q == L'/') p = q + 1;
        name = p;
    }
    CloseHandle(h);
    return name;
}

static std::wstring DecodeEx(LONG_PTR ex) {
    std::wstring s;
    struct { LONG_PTR bit; const wchar_t* name; } bits[] = {
        { WS_EX_TOPMOST,             L"TOPMOST" },
        { WS_EX_LAYERED,             L"LAYERED" },
        { WS_EX_TRANSPARENT,         L"TRANSPARENT" },
        { WS_EX_NOACTIVATE,          L"NOACTIVATE" },
        { WS_EX_TOOLWINDOW,          L"TOOLWINDOW" },
        { WS_EX_APPWINDOW,           L"APPWINDOW" },
        { WS_EX_NOREDIRECTIONBITMAP, L"NOREDIRECTIONBITMAP" },
        { WS_EX_WINDOWEDGE,          L"WINDOWEDGE" },
        { WS_EX_DLGMODALFRAME,       L"DLGMODALFRAME" },
        { WS_EX_STATICEDGE,          L"STATICEDGE" },
    };
    for (auto& b : bits)
        if (ex & b.bit) { if (!s.empty()) s += L" "; s += b.name; }
    if (s.empty()) s = L"(none)";
    return s;
}

static std::wstring DecodeSt(LONG_PTR st) {
    std::wstring s;
    struct { LONG_PTR bit; const wchar_t* name; } bits[] = {
        { WS_DISABLED,   L"DISABLED" },
        { WS_CHILD,      L"CHILD" },
        { (LONG_PTR)WS_POPUP, L"POPUP" },
        { WS_CAPTION,    L"CAPTION" },
        { WS_THICKFRAME, L"THICKFRAME" },
        { WS_VISIBLE,    L"VISIBLE" },
        { WS_SYSMENU,    L"SYSMENU" },
        { WS_MINIMIZEBOX,L"MINBOX" },
        { WS_MAXIMIZEBOX,L"MAXBOX" },
    };
    for (auto& b : bits)
        if (st & b.bit) { if (!s.empty()) s += L" "; s += b.name; }
    if (s.empty()) s = L"(none)";
    return s;
}

static std::wstring DecodeGuiFlags(DWORD f) {
    std::wstring s;
    struct { DWORD bit; const wchar_t* name; } bits[] = {
        { GUI_CARETBLINKING,  L"CARETBLINKING" },
        { GUI_INMENUMODE,     L"INMENUMODE" },
        { GUI_INMOVESIZE,     L"INMOVESIZE" },
        { GUI_INMENULOOP,     L"INMENULOOP" },
        { GUI_POPUPMENUMODE,  L"POPUPMENUMODE" },
        { GUI_SYSTEMMENUMODE, L"SYSTEMMENUMODE" },
        { GUI_FOCUSABLE,      L"FOCUSABLE" },
        { GUI_INCONTRLMODE,   L"INCONTRLMODE" },
    };
    for (auto& b : bits)
        if (f & b.bit) { if (!s.empty()) s += L"+"; s += b.name; }
    if (s.empty()) s = L"(none)";
    return s;
}

// ---------------------------------------------------------------- window dump
static void LogWindow(const std::wstring& label, HWND h) {
    if (!h) { Log(label + L": (null)"); return; }
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    LONG_PTR st = GetWindowLongPtrW(h, GWL_STYLE);
    RECT rc = {};
    GetWindowRect(h, &rc);

    Log(label + L" hwnd=" + PtrStr(h) + L" \"" + GetTitleW(h) + L"\" class=\"" + GetClassW(h)
        + L"\" pid=" + std::to_wstring(pid) + L" (" + ProcName(pid) + L")");
    Log(L"      GetWindowRect=" + RectStr(rc)
        + L"  visible=" + BoolStr(IsWindowVisible(h) != 0)
        + L" enabled=" + BoolStr(IsWindowEnabled(h) != 0)
        + L" iconic=" + BoolStr(IsIconic(h) != 0));

    RECT drc;
    if (SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &drc, sizeof(drc))))
        Log(L"      DwmFrameBounds=" + RectStr(drc));
    else
        Log(L"      DwmFrameBounds=(unavailable)");

    Log(L"      exstyle=0x" + Hex32((DWORD)ex) + L" [" + DecodeEx(ex)
        + L"]  style=0x" + Hex32((DWORD)st) + L" [" + DecodeSt(st) + L"]");
}

static bool Contains(const RECT& r, POINT p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

static void LogStack(POINT pt) {
    Log(L"    --- z-order stack at point, TOP first (first entry = what is visually on top):");
    int idx = 0;
    for (HWND h = GetTopWindow(NULL); h != NULL; h = GetWindow(h, GW_HWNDNEXT)) {
        if (!IsWindowVisible(h) || IsIconic(h)) continue;
        RECT rc;
        if (!GetWindowRect(h, &rc)) continue;
        if (!Contains(rc, pt)) continue;

        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);

        std::wstring num = std::to_wstring(idx + 1);
        if (num.size() < 2) num = L" " + num;
        Log(L"      " + num + L". hwnd=" + PtrStr(h)
            + L" \"" + GetTitleW(h) + L"\" class=\"" + GetClassW(h)
            + L"\" pid=" + std::to_wstring(pid)
            + L" [" + DecodeEx(ex) + L"] rect=" + RectStr(rc));

        if (++idx >= 12) { Log(L"      ... (truncated)"); break; }
    }
    if (idx == 0) Log(L"      (no visible window contains the point)");
}

struct FindCtx { const wchar_t* sub; std::vector<HWND>* out; };

static BOOL CALLBACK FindProc(HWND h, LPARAM lp) {
    FindCtx* c = (FindCtx*)lp;
    if (GetTitleW(h).find(c->sub) != std::wstring::npos)
        c->out->push_back(h);
    return TRUE;
}

static std::vector<HWND> FindBySub(const wchar_t* sub) {
    std::vector<HWND> out;
    FindCtx c; c.sub = sub; c.out = &out;
    EnumWindows(FindProc, (LPARAM)&c);
    return out;
}

static void LogOverlays(POINT pt, bool withPoint) {
    const wchar_t* tags[] = { L"Align Over", L"StudentOverlay" };
    for (auto tag : tags) {
        auto wins = FindBySub(tag);
        if (wins.empty()) {
            Log(L"    overlay search \"" + std::wstring(tag) + L"\": NOT FOUND");
            continue;
        }
        for (HWND h : wins) {
            LogWindow(L"    OVERLAY[\"" + std::wstring(tag) + L"\"]", h);
            if (!withPoint) continue;
            RECT rc = {};
            GetWindowRect(h, &rc);
            Log(L"      point rel. to overlay: dx=" + std::to_wstring((int)(pt.x - rc.left))
                + L" dy=" + std::to_wstring((int)(pt.y - rc.top))
                + L" (dy<0 => point ABOVE window top)  inside GetWindowRect="
                + BoolStr(Contains(rc, pt)));
            RECT drc;
            if (SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &drc, sizeof(drc))))
                Log(L"      inside DwmFrameBounds=" + BoolStr(Contains(drc, pt)));
        }
    }
}

// ---------------------------------------------------------------- click capture
static std::wstring HtName(LRESULT r) {
    switch (r) {
    case HTERROR:           return L"HTERROR";
    case HTTRANSPARENT:     return L"HTTRANSPARENT (deliberately passes through)";
    case HTNOWHERE:         return L"HTNOWHERE";
    case HTCLIENT:          return L"HTCLIENT";
    case HTCAPTION:         return L"HTCAPTION";
    case HTSYSMENU:         return L"HTSYSMENU";
    case HTGROWBOX:         return L"HTGROWBOX/HTSIZE";
    case HTMENU:            return L"HTMENU";
    case HTHSCROLL:         return L"HTHSCROLL";
    case HTVSCROLL:         return L"HTVSCROLL";
    case HTMINBUTTON:       return L"HTMINBUTTON";
    case HTMAXBUTTON:       return L"HTMAXBUTTON";
    case HTLEFT:            return L"HTLEFT";
    case HTRIGHT:           return L"HTRIGHT";
    case HTTOP:             return L"HTTOP";
    case HTBOTTOM:          return L"HTBOTTOM";
    case HTTOPLEFT:         return L"HTTOPLEFT";
    case HTTOPRIGHT:        return L"HTTOPRIGHT";
    case HTBOTTOMLEFT:      return L"HTBOTTOMLEFT";
    case HTBOTTOMRIGHT:     return L"HTBOTTOMRIGHT";
    case HTBORDER:          return L"HTBORDER";
    case HTCLOSE:           return L"HTCLOSE";
    case HTHELP:            return L"HTHELP";
    case HTOBJECT:          return L"HTOBJECT";
    default:                return L"HT? value=" + std::to_wstring(r);
    }
}

static std::wstring RgbStr(COLORREF c) {
    return L"(" + std::to_wstring(GetRValue(c)) + L"," +
           std::to_wstring(GetGValue(c)) + L"," +
           std::to_wstring(GetBValue(c)) + L")";
}

// Ask a window what hit-test IT reports for a screen point (DefWindowProc path).
// WindowFromPoint can silently skip a window; this cannot.
static void ProbeHitTest(HWND h, POINT pt) {
    if (!h) return;
    DWORD_PTR res = 0;
    if (SendMessageTimeoutW(h, WM_NCHITTEST, 0, (LPARAM)MAKELPARAM(pt.x, pt.y),
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &res)) {
        Log(L"    WM_NCHITTEST -> " + PtrStr(h) + L" = " + HtName((LRESULT)res)
            + L"   (window's OWN answer for this screen point)");
    } else {
        Log(L"    WM_NCHITTEST -> " + PtrStr(h) + L" = (SendMessageTimeout FAILED, err="
            + std::to_wstring(GetLastError()) + L")");
    }
}

// Sample the composited screen colour and what WindowFromPoint says for a point.
static void ProbePoint(const wchar_t* what, POINT pt) {
    HWND hit = WindowFromPoint(pt);
    HDC hdc = GetDC(NULL);
    COLORREF c = (hdc && pt.x >= 0 && pt.y >= 0) ? GetPixel(hdc, pt.x, pt.y) : 0xFFFFFFFF;
    if (hdc) ReleaseDC(NULL, hdc);

    std::wstring who;
    if (!hit) who = L"(null)";
    else {
        DWORD pid = 0;
        GetWindowThreadProcessId(hit, &pid);
        who = PtrStr(hit) + L" \"" + GetTitleW(hit) + L"\" (" + ProcName(pid) + L")";
    }
    Log(L"    probe[" + std::wstring(what) + L"] (" + std::to_wstring(pt.x) + L"," +
        std::to_wstring(pt.y) +         L")  WindowFromPoint=" + who + L"  screenRGB=" + RgbStr(c));
}

// Can the teacher window still be grabbed at its own caption, right now?
static void ReportCaptionState(const wchar_t* tag) {
    auto t = FindBySub(L"Align Over");
    if (t.empty()) {
        Log(std::wstring(tag) + L": teacher window \"Align Over\" NOT FOUND");
        return;
    }
    HWND h = t[0];
    RECT rc = {};
    if (!GetWindowRect(h, &rc)) return;
    POINT p = { rc.left + 60, rc.top + 15 };
    HWND who = WindowFromPoint(p);
    bool ok = who && (who == h || GetAncestor(who, GA_ROOT) == h);
    DWORD pid = 0;
    if (who) GetWindowThreadProcessId(who, &pid);
    Log(std::wstring(tag) + L": caption probe (" + std::to_wstring(p.x) + L"," +
        std::to_wstring(p.y) + L") on " + PtrStr(h) + L" rect=" + RectStr(rc) +
        L" ex=" + Hex32((DWORD)GetWindowLongPtrW(h, GWL_EXSTYLE)) +
        L"\r\n      WindowFromPoint=" +
        (who ? PtrStr(who) + L" \"" + GetTitleW(who) + L"\" (" + ProcName(pid) + L")"
             : std::wstring(L"(null)")) +
        L"  ==> caption HITTESTABLE=" + BoolStr(ok));
}

// F11: force Windows to rebuild the colour-key hit-test shape.
static void ReapplyColorKey(const wchar_t* tag) {
    auto t = FindBySub(L"Align Over");
    if (t.empty()) { ReportCaptionState(tag); return; }
    SetLastError(0);
    BOOL ok = SetLayeredWindowAttributes(t[0], RGB(0, 0, 0), 0, LWA_COLORKEY);
    Log(std::wstring(tag) + L": SetLayeredWindowAttributes(LWA_COLORKEY, rgb(0,0,0)) ret=" +
        BoolStr(ok != FALSE) + L" err=" + std::to_wstring(GetLastError()));
    ReportCaptionState(tag);
}

// F12: drop and re-add WS_EX_LAYERED, then re-apply the colour key.
static void ToggleLayered(const wchar_t* tag) {
    auto t = FindBySub(L"Align Over");
    if (t.empty()) { ReportCaptionState(tag); return; }
    HWND h = t[0];
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    const UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowLongPtrW(h, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
    SetWindowPos(h, NULL, 0, 0, 0, 0, flags);
    SetWindowLongPtrW(h, GWL_EXSTYLE, ex);
    SetWindowPos(h, NULL, 0, 0, 0, 0, flags);
    Log(std::wstring(tag) + L": toggled WS_EX_LAYERED off/on (ex=" + Hex32((DWORD)ex) + L")");
    ReapplyColorKey(tag);
}


static void OnClick(WPARAM btn, POINT pt, DWORD flags, ULONGLONG hookAt) {
    ++g_clickCount;
    const wchar_t* name = (btn == WM_LBUTTONDOWN) ? L"LBUTTON"
                        : (btn == WM_RBUTTONDOWN) ? L"RBUTTON" : L"MBUTTON";

    Log(std::wstring(L"==== ") + name + L" DOWN #" + std::to_wstring(g_clickCount)
        + L" at screen (" + std::to_wstring(pt.x) + L"," + std::to_wstring(pt.y) + L") ====");
    Log(L"    injected=" + BoolStr((flags & LLKHF_INJECTED) != 0)
        + L"  (YES => a program generated this click)");
    Log(L"    hook->log latency = " + std::to_wstring(GetTickCount64() - hookAt) + L"ms"
        + L"  (large value => this snapshot is NOT the state at click instant)");

    LogWindow(L"    fg-BEFORE", GetForegroundWindow());

    HWND hit = WindowFromPoint(pt);
    LogWindow(L"    WindowFromPoint", hit);
    if (hit) {
        HWND root = GetAncestor(hit, GA_ROOT);
        if (root && root != hit) LogWindow(L"    WindowFromPoint->GA_ROOT", root);
    }

    LogStack(pt);
    LogOverlays(pt, true);

    // ---- decisive probes: is the overlay skipped, or is the point really elsewhere?
    HWND topWin = GetTopWindow(NULL);
    Log(L"    GetTopWindow(NULL) = " +
        (topWin ? PtrStr(topWin) + L" \"" + GetTitleW(topWin) + L"\"" : std::wstring(L"(null)")) +
        L"   (is the teacher window REALLY the topmost window?)");
    auto teachers = FindBySub(L"Align Over");
    for (HWND h : teachers) {
        ProbeHitTest(h, pt);
        RECT rc = {};
        if (!GetWindowRect(h, &rc)) continue;
        HRGN rgn = CreateRectRgn(0, 0, 0, 0);
        int rgnState = GetWindowRgn(h, rgn);
        DeleteObject(rgn);
        Log(L"    window region = " +
            (rgnState == NULLREGION ? std::wstring(L"NULLREGION (empty!)")
             : rgnState == ERROR ? std::wstring(L"ERROR/none")
             : std::wstring(L"SIMPLEREGION (a region IS set)")));

        POINT capL = { rc.left + 60,  rc.top + 15 };
        POINT capR = { rc.right - 140, rc.top + 15 };
        POINT cli  = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 + 40 };
        POINT outA = { rc.left + 60, rc.top - 40 };
        if (outA.y < 0) outA = POINT{ rc.left + 60, rc.bottom + 40 };
        ProbePoint(L"caption-left", capL);
        ProbePoint(L"caption-right", capR);
        ProbePoint(L"client-center", cli);
        ProbePoint(L"just-above-window", outA);
        break; // only the first teacher window
    }

    GUITHREADINFO gi = {};
    gi.cbSize = (UINT)sizeof(gi);
    if (!teachers.empty()) {
        DWORD pid = 0;
        DWORD tid = GetWindowThreadProcessId(teachers[0], &pid);
        if (GetGUIThreadInfo(tid, &gi))
            Log(L"    overlay thread GUI state AT CLICK: flags=" +
                DecodeGuiFlags(gi.flags) + L" focus=" + PtrStr(gi.hwndFocus) +
                L" capture=" + PtrStr(gi.hwndCapture) + L" active=" + PtrStr(gi.hwndActive));
    }
}

// The WH_MOUSE_LL callback must return fast: Windows holds back the mouse
// event until it does. Only queue work here; all logging happens on the worker.
static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION &&
        (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN)) {
        MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)lParam;
        EnterCriticalSection(&g_qCs);
        if (g_pending.size() < 64) {
            ClickRec r; r.btn = (DWORD)wParam; r.pt = m->pt; r.flags = m->flags;
            r.at = GetTickCount64();
            g_pending.push_back(r);
        }
        LeaveCriticalSection(&g_qCs);
        SetEvent(g_wake);
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// ---------------------------------------------------------------- heartbeat
static void Heartbeat(std::wstring& lastSig, ULONGLONG& lastDetail, ULONGLONG now) {
    std::vector<HWND> found = FindBySub(L"Align Over");
    auto stu = FindBySub(L"StudentOverlay");
    found.insert(found.end(), stu.begin(), stu.end());

    static std::vector<HWND> known;
    if (found != known) {
        known = found;
        if (known.empty()) Log(L"    overlay search: NOT FOUND (ScreenPointer not running?)");
        else for (HWND h : known) LogWindow(L"    overlay appeared", h);
    }

    HWND fg = GetForegroundWindow();
    HWND cap = GetCapture();

    if (known.empty()) {
        Log(L"HB fg=" + PtrStr(fg) + L" \"" + Clip(GetTitleW(fg), 70) + L"\" cap="
            + PtrStr(cap) + L" overlay=NOT RUNNING");
        return;
    }

    HWND h = known[0];
    if (!IsWindow(h)) { known.clear(); return; }

    DWORD_PTR ping = 0;
    BOOL alive = SendMessageTimeoutW(h, WM_NULL, 0, 0,
                                     SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &ping);
    std::wstring resp = alive ? L"YES" : L"NO (UI THREAD HUNG)";

    std::wstring gui = L"(n/a)";
    DWORD gflags = 0;
    DWORD ownPid = 0;
    DWORD tid = GetWindowThreadProcessId(h, &ownPid);
    GUITHREADINFO gi = {};
    gi.cbSize = (UINT)sizeof(gi);
    if (GetGUIThreadInfo(tid, &gi)) {
        gflags = gi.flags;
        gui = DecodeGuiFlags(gi.flags)
            + L" focus=" + PtrStr(gi.hwndFocus)
            + L" capture=" + PtrStr(gi.hwndCapture)
            + L" active=" + PtrStr(gi.hwndActive);
    }

    RECT rc = {};
    GetWindowRect(h, &rc);
    DWORD st = (DWORD)GetWindowLongPtrW(h, GWL_STYLE);
    DWORD ex = (DWORD)GetWindowLongPtrW(h, GWL_EXSTYLE);
    int en = IsWindowEnabled(h) != 0;
    int vis = IsWindowVisible(h) != 0;
    Log(L"HB fg=" + PtrStr(fg) + L" \"" + Clip(GetTitleW(fg), 70) + L"\" cap="
        + PtrStr(cap) + L" ui_responds=" + resp
        + L" gui=" + gui
        + L" en=" + BoolStr(en != 0) + L" vis=" + BoolStr(vis != 0)
        + L" rect=" + RectStr(rc)
        + L" st=0x" + Hex32(st) + L" ex=0x" + Hex32(ex));

    // Is the teacher window's CAPTION still reachable by hit-testing?
    // Logged only when it flips, so this line marks the exact second it breaks.
    auto tlist = FindBySub(L"Align Over");
    if (!tlist.empty()) {
        HWND tw = tlist[0];
        RECT trc = {};
        if (GetWindowRect(tw, &trc)) {
            POINT capp = { trc.left + 60, trc.top + 15 };
            HWND who = WindowFromPoint(capp);
            bool reachable = (who == tw) || (who && GetAncestor(who, GA_ROOT) == tw);
            static bool reachInit = false;
            static bool lastReach = true;
            if (!reachInit || reachable != lastReach) {
                reachInit = true;
                lastReach = reachable;
                Log(L"    caption-hit-test " + BoolStr(reachable) + L" at (" +
                    std::to_wstring(capp.x) + L"," + std::to_wstring(capp.y) +
                    L")  WindowFromPoint=" +
                    (who ? PtrStr(who) : std::wstring(L"(null)")) +
                    L"  <-- if this flips to NO, the window became transparent");
            }
        }
    }


    std::wstring sig = resp + L"|" + PtrStr(fg) + L"|" + PtrStr(cap) + L"|" +
                       std::to_wstring(gflags) + L"|" + RectStr(rc) +
                       Hex32(st) + Hex32(ex) + std::to_wstring(en) + std::to_wstring(vis);
    if (sig != lastSig && now - lastDetail >= 500) {
        lastSig = sig;
        lastDetail = now;
        Log(L"    -- state CHANGED --");
        LogWindow(L"    state", h);
    }
}

static DWORD WINAPI WorkerProc(LPVOID) {
    ULONGLONG lastClick = 0;
    bool needAfter = false;
    ULONGLONG lastHb = 0;
    ULONGLONG lastAlive = 0;
    ULONGLONG lastDetail = 0;
    std::wstring lastSig;

    for (;;) {
        if (g_quit) break;
        WaitForSingleObject(g_wake, 250);

        std::vector<ClickRec> local;
        EnterCriticalSection(&g_qCs);
        local.swap(g_pending);
        LeaveCriticalSection(&g_qCs);

        for (auto& c : local) {
            OnClick(c.btn, c.pt, c.flags, c.at);
            lastClick = GetTickCount64();
            needAfter = true;
        }

        ULONGLONG now = GetTickCount64();
        if (needAfter && now - lastClick >= 300) {
            needAfter = false;
            LogWindow(L"    fg-AFTER(300ms)", GetForegroundWindow());
        }
        if (now - lastHb >= 1000) {
            lastHb = now;
            Heartbeat(lastSig, lastDetail, now);
        }
        if (now - lastAlive >= 5000) {
            lastAlive = now;
            Log(L"--- click_diag alive, hook installed, waiting for input ---");
        }
    }
    return 0;
}

// ---------------------------------------------------------------- host window
static LRESULT CALLBACK MsgWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static BOOL WINAPI CtrlHandler(DWORD type) {
    g_quit = true;
    if (g_msgWnd) PostMessageW(g_msgWnd, WM_CLOSE, 0, 0);
    Sleep(300);
    return FALSE;
}

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    InitializeCriticalSection(&g_logCs);
    InitializeCriticalSection(&g_qCs);
    g_start = GetTickCount64();
    g_wake = CreateEventW(NULL, FALSE, FALSE, NULL);

    g_log = CreateFileW(L"click_diag_log.txt", GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_log != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER z; z.QuadPart = 0;
        SetFilePointerEx(g_log, z, NULL, FILE_END);
        LARGE_INTEGER sz; sz.QuadPart = 0;
        GetFileSizeEx(g_log, &sz);
        if (sz.QuadPart == 0) {
            DWORD w = 0;
            const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
            WriteFile(g_log, bom, 3, &w, NULL);
        }
    }

    wchar_t full[MAX_PATH] = L"";
    GetFullPathNameW(L"click_diag_log.txt", MAX_PATH, full, NULL);

    Log(L"======================================================================");
    Log(L" ScreenPointer click diagnostic");
    Log(L"======================================================================");
    if (g_log == INVALID_HANDLE_VALUE)
        Log(L" WARNING: cannot open log file, error=" + std::to_wstring(GetLastError()));
    else
        Log(L" Log file: " + std::wstring(full));
    Log(L" Every left/right/middle click prints:");
    Log(L"   - was the click injected by another program?");
    Log(L"   - foreground window BEFORE the click");
    Log(L"   - what WindowFromPoint() returns for the click point");
    Log(L"   - z-order stack of ALL windows at that point (top first)");
    Log(L"   - where the ScreenPointer overlays REALLY are (rect vs point)");
    Log(L"   - foreground window 300ms AFTER the click");
    Log(L" Then four decisive probes on the teacher window:");
    Log(L"   - WM_NCHITTEST: what the window ITSELF reports for that screen point");
    Log(L"   - WindowFromPoint at caption-left / caption-right / client / outside");
    Log(L"   - screen RGB sampled at each probe point");
    Log(L"   - the overlay thread's focus/capture/active AT the click instant");
    Log(L" If WM_NCHITTEST says HTCAPTION but WindowFromPoint returns a DIFFERENT");
    Log(L" window, the overlay is being skipped by per-pixel transparency (LWA_COLORKEY).");
    Log(L" Once per second a HB line reports the overlay's heartbeat:");
    Log(L"   ui_responds = SendMessageTimeout to the app (NO => UI THREAD HUNG)");
    Log(L"   gui         = INMENUMODE/POPUPMENUMODE/INMOVESIZE of the app thread");
    Log(L"   en/vis/rect/st/ex = enabled, visible, rect, style, exstyle");
    Log(L" KEY LINE for resize trouble: does gui show INMENUMODE/INMOVESIZE");
    Log(L"   stuck ON, or ui_responds=NO, at the moment resize stops working?");
    Log(L" Press F9 or close this console to quit.");
    Log(L" EXPERIMENT KEYS (all leave the overlay in its normal colour-keyed state):");
    Log(L"   F10 = report only: is the teacher caption still hittable?");
    Log(L"   F11 = re-apply SetLayeredWindowAttributes(LWA_COLORKEY), then report");
    Log(L"   F12 = drop/re-add WS_EX_LAYERED + re-apply colour key, then report");
    Log(L"   Run F11/F12 ONLY after the title bar has stopped reacting to clicks.");
    Log(L"----------------------------------------------------------------------");

    HINSTANCE hInst = GetModuleHandleW(NULL);
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc   = MsgWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"ClickDiagMsgWnd";
    RegisterClassW(&wc);

    g_msgWnd = CreateWindowExW(0, wc.lpszClassName, L"ClickDiag", 0,
                               0, 0, 0, 0, HWND_MESSAGE, NULL, hInst, NULL);
    if (!g_msgWnd) {
        Log(L"ERROR: message window creation failed: " + std::to_wstring(GetLastError()));
        return 1;
    }

    Log(L" Overlay snapshot at startup:");
    LogOverlays(POINT{ 0, 0 }, false);

    g_worker = CreateThread(NULL, 0, WorkerProc, NULL, 0, NULL);

    g_hook = SetWindowsHookExW(WH_MOUSE_LL, HookProc, hInst, 0);
    if (!g_hook) {
        Log(L"ERROR: SetWindowsHookEx failed: " + std::to_wstring(GetLastError()));
        g_quit = true;
        return 1;
    }

    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    Log(L" Hook installed. Start/reproduce the ScreenPointer state, then click.");
    Log(L"----------------------------------------------------------------------");

    MSG msg;
    for (;;) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetAsyncKeyState(VK_F9) & 1) {
            Log(L"F9 pressed, quitting.");
            break;
        }
        if (GetAsyncKeyState(VK_F10) & 1) ReportCaptionState(L"F10");
        if (GetAsyncKeyState(VK_F11) & 1) ReapplyColorKey(L"F11");
        if (GetAsyncKeyState(VK_F12) & 1) ToggleLayered(L"F12");
        Sleep(10);
    }

done:
    g_quit = true;
    if (g_wake) SetEvent(g_wake);
    if (g_worker) {
        WaitForSingleObject(g_worker, 3000);
        CloseHandle(g_worker);
        g_worker = NULL;
    }
    if (g_hook) UnhookWindowsHookEx(g_hook);
    if (g_log != INVALID_HANDLE_VALUE) { CloseHandle(g_log); g_log = INVALID_HANDLE_VALUE; }
    if (g_wake) { CloseHandle(g_wake); g_wake = NULL; }
    return 0;
}
