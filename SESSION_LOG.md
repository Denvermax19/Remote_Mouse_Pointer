# Session log — ScreenPointer (teacher overlay / custom title bar)

**Read this file instead of re-deriving history or re-reading code comments.**
Old eras (colour-key bug, ClickDiag runs, hover attempts) are collapsed to a
summary at the bottom — only the current design matters.
Current = **BUILD 8, user-verified working** (UI + all functionality, Release
single-exe build).

## Project rules

- Root `C:\Users\lette\C_Projects\Remote_Mouse_Pointer`; targets `ScreenPointer`
  (`main.cpp`) and `ClickDiag` (`click_diag.cpp`). `test.cpp` (old red-dot
  prototype) deleted — never in CMake. Old `build\click_diag_log.txt` deleted.
- User runs `cmake --build build` — **agent must NOT build** (AGENTS.md).
  After edits: balance check + `graphify update .` + append to this log.
- Console app (`main()`, `std::cout` diagnostics work).
- `CMakeLists.txt` forces **static CRT + `x86-windows-static` triplet**.
  **Proven build sequence** (developer/x86 Native Tools prompt — NMake needs
  `cl` on PATH; the `rmdir` is mandatory after any triplet change):

  ```bat
  C:\vcpkg\vcpkg install ixwebsocket:x86-windows-static nlohmann-json:x86-windows-static
  cd /d C:\Users\lette\C_Projects\Remote_Mouse_Pointer
  rmdir /s /q build
  cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
  cmake --build build
  ```

  Why the fresh dir: classic vcpkg (no `vcpkg.json`) never auto-installs on a
  triplet change, and a stale cache keeps `ixwebsocket_DIR` pinned to old
  dynamic libs → LNK2038 `RuntimeLibrary` mismatch (/MT vs /MD). Ran once,
  **verified**: `build\ScreenPointer.exe` ≈ 1.0 MB, imports only Windows
  system DLLs (d2d1/dwrite/ws2_32/bcrypt/…) — copy that ONE file to any
  Windows PC; no redist, no DLLs. First run: SmartScreen "More info → Run
  anyway" + firewall allow.

## Current architecture (main.cpp, BUILD 8)

### Window

- Teacher: `WS_EX_TOPMOST` + `WS_OVERLAPPEDWINDOW`, **no `WS_EX_LAYERED`**;
  `WM_NCCALCSIZE → 0` (client == window rect, no system caption/frame).
  `WM_GETMINMAXINFO` clamps to monitor work area (else maximized covers the
  taskbar), min track 260×160. `wc.style = CS_DBLCLKS` → double-click title
  bar maximizes.
- Click-through interior via donut region: `ApplyDonutRegion()` =
  `SetWindowRgn(full rect minus hole)`; hole = client inset by `strip` on top,
  `grip` elsewhere; `IsZoomed → grip = 0` (no inert ring when maximized).
  Re-applied on creation and every `WM_SIZE`. Region pixels are simply not
  part of the window, so they never hit-test — nothing to go stale (this
  killed the original colour-key bug class; the re-apply sweep is gone too).
- `HandleBands(w,h,strip,grip)` = single source of truth shared by WM_PAINT,
  WM_NCHITTEST and the region, so they can never disagree:
  `grip = GRIP = 6` (min 2; `w/12`/`h/12` if tiny),
  `strip = TITLE_H = 36` (≤ `h/3`, ≥ `2*grip`).
  Other constants: `BTN_W=40`, `BTN_H=24`, `DOT_SIZE=24`.

### Custom title bar

- Paint order: chrome fill (whole client; donut clips it to the ring) → title
  band (`titleBgBrush`, `strip` px) → DirectWrite caption text (Segoe UI 13 pt,
  rect `8 … BTN_MIN.left-8`) → 3 static glyphs → 2 px blue border
  `RectF(1, strip-1, w-1, h-1)` — top edge flush under the band; its middle is
  over the hole and clipped away by design.
- Buttons: `CaptionButtons()` right-aligned with `off = grip` (clears the
  border). Glyphs always drawn by `DrawGlyph`: **white** `glyphBrush` (1,1,1),
  2 px strokes, `s=6`; max shows restore double-square while zoomed.
  **No hover — abandoned by user's choice** (Windows Terminal reference:
  static glyphs; see History).
- Click path (works): `WM_NCHITTEST` → button rect = `HTCLIENT`; `y < grip`
  (not zoomed) = `HTTOP*`; rest of title = `HTCAPTION` (drag); edges/corners =
  resize; zoomed = `HTCLIENT`. `WM_LBUTTONDOWN` → `HitCaptionButton` →
  `WM_SYSCOMMAND` (SC_MINIMIZE / SC_MAXIMIZE|SC_RESTORE / SC_CLOSE).
- **DPI (BUILD 8 — do not regress):** process is
  `PER_MONITOR_AWARE_V2`, so `GetClientRect`/`SetWindowRgn`/cursor maths are
  PHYSICAL pixels (display at 125%, `AppliedDPI=120`), but
  `CreateHwndRenderTarget` defaults to monitor DPI and scales every D2D
  coordinate ×1.25 before rasterising. **`renderTarget->SetDpi(96.0f, 96.0f)`
  right after creation makes 1 DIP = 1 client pixel** — paint, region and
  hit-test then share one coordinate space (WM_SIZE already resizes the RT to
  the client, so the present mapping is identity). Without it the button rects
  (x 697..817) render at 871..1021 px, past the 817 px client edge → glyphs
  and hover highlights were invisible in ALL builds 3–7 while raw-pixel clicks
  kept working. That was the whole hover mystery. First paint prints once:
  `[UI] dpi=96 client=… strip=… grip=… btns=…`.
- Gotcha: `#undef DrawText` **before** `#include <d2d1.h>` — the `windows.h`
  macro would rewrite `ID2D1RenderTarget::DrawText` into `DrawTextW`.

### Other windows / threads

- Student: `WS_EX_TRANSPARENT|WS_EX_LAYERED` + `LWA_ALPHA` +
  `DwmExtendFrameIntoClientArea({-1,-1,-1,-1})`, not click-through; red dot =
  `FillEllipse(dotX, dotY, 15)` at network coords (pixel-exact thanks to
  `SetDpi(96)`).
- Ctrl red-dot preview (teacher): separate GDI popup `g_dotWnd`, class
  `ScreenPointerDot`, 24 px, elliptic region, solid red `FillRect` — the donut
  hole would clip anything drawn on the teacher itself. `IsDotWnd()` keys on
  the class name, not the global (null mid-creation). Shown/hidden/positioned
  from the 16 ms `WM_TIMER`; destroyed from the teacher's `WM_DESTROY`.
- Input transmitter thread and the dot logic trim `strip` off the client rect
  before mapping to the student's screen (title bar ≠ overlay area).

## Status & next steps

- **All app issues resolved and user-verified (this session):** title bar UI,
  glyphs, clicks, drag/resize, donut region, red-dot preview, plus the
  BUILD 8 DPI fix. First paint still prints the one-shot
  `[UI] dpi=96 client=… strip=… grip=… btns=…` line — useful to ask for from
  a student's console screenshot when debugging remotely.
- **Remaining (no code work pending):** classroom rollout of the single
  `ScreenPointer.exe`. WebSocket URL must be reachable from every laptop
  (same LAN; if the server runs on the teacher's machine, allow its port
  inbound in Windows Firewall and make sure the AP doesn't isolate clients).
  ClickDiag is dev-only — never ships to students.
- Known code TODOs are the Open items below; nothing else.

## Diagnostic tool (only if a re-run is needed)

`click_diag.cpp` → `build\ClickDiag.exe` writes `build\click_diag_log.txt`.
Run ClickDiag first → ScreenPointer → reproduce → **F9** quits.
**F10** = caption hittability report. F11/F12 answered the old colour-key
questions — the colour key is gone, don't repeat them.

## History (collapsed — do not re-derive)

- **Original bug (closed):** `WS_EX_LAYERED` + `LWA_COLORKEY` hit-test shape
  went stale after the first move/resize/maximize → caption/resize/buttons all
  fell through to the window behind; the `g_reapplyPending` sweep and manual
  F11/F12 provably never repaired it (9 ClickDiag runs, 284 clicks). Fix =
  BUILD 1 "Option C": drop the colour key entirely, use the donut region.
  All ClickDiag findings are historical only.
- **Option A (client drag strip):** failed — same colour-key class of
  problem; removed.
- **Hover attempts (BUILD 3→6):** message-driven → client-sync → paint-time →
  `ScreenToClient` — none ever lit the buttons. The real cause was the DPI
  clipping described above, but per the user's decision (BUILD 7) hover was
  dropped entirely: glyphs are static and always visible.

## Open items

- Right-click on `HTCAPTION` opens the system menu — candidate: handle
  `WM_NCRBUTTONDOWN/UP` with `wParam == HTCAPTION`, return 0.
- `renderTarget->EndDraw()` return ignored; on `D2DERR_RECREATE_TARGET` after
  a resize the overlay freezes silently → release and recreate the target.
- Kill any stray ScreenPointer / StudentOverlay process before a test run
  (a second instance adds noise).
