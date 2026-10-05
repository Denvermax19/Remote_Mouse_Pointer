# Session log — ScreenPointer title-bar / move / resize bug

Read this first. It replaces re-reading the whole codebase.

## The bug (user report)

On the **teacher** overlay window of `ScreenPointer`:

1. The window can be moved and resized normally the first time.
2. After the first move + **resize**, the title bar stops reacting to mouse input —
   the window can no longer be grabbed to move or resize.
3. Separately, after a **right-click** on the title bar the caption text greys out.

Symptom persists for the rest of the session.

## Project facts

- Root: `C:\Users\lette\C_Projects\Remote_Mouse_Pointer`
- Targets: `ScreenPointer` (main.cpp, line 9 of CMakeLists) and `ClickDiag` (click_diag.cpp, line 22)
- Build: `cmake --build build` — **the user builds manually; the agent must NOT run any build/compile command** (AGENTS.md).
- `graphify-out/` knowledge graph exists; run `graphify update .` after code edits.

### Window creation (main.cpp)

```cpp
CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, ..., WS_OVERLAPPEDWINDOW, ...)
SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);   // teacher
SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);             // student
```

- Teacher: interior `Clear()` to opaque black = fully colour-key transparent,
  so clicks pass through to the app underneath. Standard title bar drawn by
  `DefWindowProc` (no custom `WM_NCHITTEST` in the app).
- Student (reference impl): `LWA_ALPHA` + `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})`,
  D2D clears to alpha 0. Clicks do **not** pass through it.
- `WindowProc` (main.cpp:38) handles only `WM_ERASEBKGND`, `WM_SIZE`, `WM_PAINT`,
  `WM_TIMER`, `WM_DESTROY`, else `DefWindowProcW`.
- `inputThread` polls `GetClientRect`/`ClientToScreen` cross-thread every 16 ms.

## Diagnostic tool

`click_diag.cpp` → `build\ClickDiag.exe`, writes **`build\click_diag_log.txt`**
(user calls it "click_log.txt" — same file, only written by ClickDiag).

Run order: start `ClickDiag.exe` first → then `ScreenPointer.exe` → reproduce →
**F9** quits ClickDiag → user sends the log.

It logs, for every mouse-down: injection flag, foreground before/after,
`WindowFromPoint`, full z-order stack at the point, overlay rects, then decisive
probes on the teacher window:

- `WM_NCHITTEST` sent **to the teacher itself** (`SendMessageTimeoutW`) — what the
  window's own answer is for that screen point.
- `WindowFromPoint` at caption-left / caption-right / client-centre / just-above.
- Screen RGB (`GetPixel`) at each probe.
- Overlay thread `GetGUIThreadInfo` flags/focus/capture/active at the click instant.
- Per-second heartbeat: `ui_responds` (`SendMessageTimeout WM_NULL`), `gui` flags,
  enabled/visible/rect/style/exstyle, `-- state CHANGED --` dumps.
- Hook→log latency (always 0 ms, so snapshots are at the click instant).

Experiment hotkeys added this session (all leave the window in its normal
colour-keyed state): **F10** = report caption hittability, **F11** = re-apply
`SetLayeredWindowAttributes(LWA_COLORKEY)`, **F12** = drop/re-add `WS_EX_LAYERED`
+ re-apply colour key.

`GetCapture()` is per-thread and always NULL in ClickDiag — use
`gi.hwndCapture` instead.

## What the logs proved (runs 5 and 6)

Latest run occupies lines **1936–3928** of `build\click_diag_log.txt` (3928 total,
lines 1–1935 are stale runs). Run 6: teacher hwnd `001F0686`, Terminal `001407E6`.

**Ruled out:**

| Hypothesis | Evidence |
|---|---|
| Hung UI thread | `ui_responds=YES` on every heartbeat |
| Stuck menu / move-size loop | `gui=(none)` after the drag ends |
| Capture leak | `hwndCapture=0` throughout |
| Disabled / hidden | `en=YES vis=YES` |
| Style or exstyle change | `style=0x14CF0000`, `ex=0x00080108` identical before and after |
| Window region set | `GetWindowRgn` = `ERROR/none` |
| DPI mismatch | same process/monitor |
| Worker latency | hook→log always 0 ms |

**Confirmed mechanism (the contradiction that pins it):**

The teacher window is at z-order **#1** (`GetTopWindow(NULL)`), visible, enabled,
has no region, and **its own `WM_NCHITTEST` correctly answers `HTCAPTION` /
`HTTOPLEFT` / `HTCLIENT`** for points inside it — yet `WindowFromPoint` at those
same points returns the window *behind* it (WindowsTerminal, or Chrome in run 5),
and the real mouse click goes there too (foreground switches to Terminal).

The teacher **is visibly painting its own caption**: probe at caption-left gives
screen RGB `(242,243,245)` light gray, while a probe 40 px *above* the window
top gives `(15,17,26)` = Terminal. So the surface content is correct and the
caption is drawn, but Windows' layered colour-key hit-test shape no longer covers
the non-client area.

**Break point in run 6:**

- Click #13 at `+66109ms`, point `(347,310)` → `HTTOPLEFT`, `gui=INMOVESIZE` — **works**.
- `caption-hit-test NO at (250,288)` at **`+66797ms`, still `gui=INMOVESIZE`** — breaks
  *during* the first **resize** (a plain move before it did not break it).
- Resize ends `+68875ms`, rect `(80,213)-(1185,907)`.
- From click #14 (`+71984ms`) onward every click lands on the Terminal; teacher
  never re-activates (`focus=active=0`) until the app is closed `~+138609ms`.

## Conclusion

Windows keeps a layered window's colour-key hit-test shape stale after a size /
activation change: the window still paints, still reports `HTCAPTION` for its own
title bar, but the input system treats the non-client area as transparent and
passes clicks through to the window underneath. This matches a documented
`SetLayeredWindowAttributes` defect (MS Q&A: *"the non-client area remains
unaffected initially but passes mouse clicks after [a state change]"*).
`WindowFromPoint`'s normal `HTTRANSPARENT` rule does **not** explain it, because
Raymond Chen notes it does not even send `WM_NCHITTEST` cross-process — and our
own `WM_NCHITTEST` answer was correct anyway.

## Changes made this session (NOT yet built or verified)

### main.cpp

- `main.cpp:41-53` — new block at the top of `WindowProc`: for the **teacher only**,
  re-apply `SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY)` on
  `WM_EXITSIZEMOVE`, `WM_NCACTIVATE`, `WM_DISPLAYCHANGE`, `WM_SIZE` with
  `SIZE_MAXIMIZED`, and restore-from-minimize (flag `wasMinimized`, main.cpp:34).
  This forces Windows to rebuild the stale hit-test shape.
- `main.cpp:67` — null-guard `renderTarget` at the top of `WM_PAINT`.

### click_diag.cpp (707 → 767 lines)

- `ReportCaptionState(tag)` — `WindowFromPoint` on the teacher caption, right now.
- `ReapplyColorKey(tag)` / `ToggleLayered(tag)` — the F11 / F12 experiments.
- Hotkey dispatch in the main loop + banner help lines.
- Already present from earlier rounds: `ClickRec.at`, `HtName`, `RgbStr`,
  `ProbeHitTest`, `ProbePoint`, `GetTopWindow` identity, `GetWindowRgn`, four
  probe points, per-second caption-hit-test flip line, `#pragma comment(lib,"gdi32.lib")`.

Both files verified balanced (main.cpp 36/36 braces, 164/164 parens;
click_diag.cpp 137/137, 619/619). `graphify update .` has been run.

## Next steps for this session

1. Have the user run `cmake --build build` (both targets).
2. Kill the stray `StudentOverlay` process first (a second ScreenPointer instance
   is always running and adds noise to the log).
3. Reproduce: move the teacher, **resize** it, then click its title bar.
   - If it still breaks → press **F11**, then **F12**, then **F9** and read the log.
     F11 succeeding confirms re-applying is the fix; only F12 working means the
     `WS_EX_LAYERED` toggle is required.
4. If the fix works, verify the right-click caption-grey symptom also cleared
   (it should — `WM_NCACTIVATE` re-applies).

### Still open / not done

- Right-click on `HTCAPTION` still opens the system menu —
  candidate: handle `WM_NCRBUTTONDOWN/UP` with `wParam==HTCAPTION`, return 0.
- `renderTarget->EndDraw()` return value is still ignored; if it ever returns
  `D2DERR_RECREATE_TARGET` after a resize the overlay freezes silently.
  Candidate: on failure release and recreate the render target.
- If re-applying does **not** fix it, the fallback is to stop using `LWA_COLORKEY`
  for the teacher and switch to the student's approach (`LWA_ALPHA` + 
  `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})` + D2D `Clear(alpha 0)`) — but note
  that makes the client click-through=false, i.e. clicks would no longer pass
  through the overlay to the app underneath. Discuss with the user first.

## Reference material

- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes
- https://github.com/MicrosoftEdge/WebView2Feedback/issues/5668 — a `WS_EX_LAYERED`
  alpha-0 window *without* `WS_EX_TRANSPARENT` still hit-tests.
