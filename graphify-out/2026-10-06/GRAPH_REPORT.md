# Graph Report - Remote_Mouse_Pointer  (2026-10-06)

## Corpus Check
- 3 files · ~7,911 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 87 nodes · 239 edges · 10 communities (9 shown, 1 thin omitted)
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `83c8dabf`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- click_diag.cpp
- WindowProc
- Session log — ScreenPointer (teacher overlay / custom title bar)
- OnClick
- ClickRec
- HWND
- Log
- RectStr
- RgbStr
- wstring

## God Nodes (most connected - your core abstractions)
1. `OnClick()` - 18 edges
2. `LogWindow()` - 17 edges
3. `Log()` - 15 edges
4. `ReportCaptionState()` - 14 edges
5. `Heartbeat()` - 14 edges
6. `WindowProc()` - 12 edges
7. `GetTitleW()` - 10 edges
8. `LogStack()` - 10 edges
9. `FindBySub()` - 10 edges
10. `LogOverlays()` - 10 edges

## Surprising Connections (you probably didn't know these)
- `ClickRec` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 4 → community 9_
- `ClickRec` --references--> `POINT`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 4 → community 3_
- `CtrlHandler()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 9 → community 5_
- `OnClick()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 9 → community 3_
- `WorkerProc()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 9 → community 0_

## Import Cycles
- None detected.

## Communities (10 total, 1 thin omitted)

### Community 0 - "click_diag.cpp"
Cohesion: 0.43
Nodes (7): DecodeEx(), DecodeSt(), GetClassW(), LogWindow(), WorkerProc(), LONG_PTR, LPVOID

### Community 1 - "WindowProc"
Cohesion: 0.21
Nodes (17): ID2D1RenderTarget, ApplyDonutRegion(), CaptionButtons(), HWND, LPARAM, LRESULT, RECT, UINT (+9 more)

### Community 2 - "Session log — ScreenPointer (teacher overlay / custom title bar)"
Cohesion: 0.18
Nodes (10): Current architecture (main.cpp, BUILD 8), Custom title bar, Diagnostic tool (only if a re-run is needed), History (collapsed — do not re-derive), Next steps (BUILD 8 verification), Open items, Other windows / threads, Project rules (+2 more)

### Community 3 - "OnClick"
Cohesion: 0.67
Nodes (7): GetTitleW(), LogStack(), OnClick(), ProbeHitTest(), ProbePoint(), PtrStr(), POINT

### Community 4 - "ClickRec"
Cohesion: 0.33
Nodes (6): ClickRec, at, btn, flags, pt, ULONGLONG

### Community 5 - "HWND"
Cohesion: 0.16
Nodes (15): BOOL, HWND, LPARAM, LRESULT, UINT, WPARAM, CtrlHandler(), FindCtx (+7 more)

### Community 6 - "Log"
Cohesion: 0.64
Nodes (9): BoolStr(), FindBySub(), Log(), LogOverlays(), main(), ReapplyColorKey(), ReportCaptionState(), ToggleLayered() (+1 more)

### Community 7 - "RectStr"
Cohesion: 0.67
Nodes (3): Contains(), RECT, RectStr()

### Community 9 - "wstring"
Cohesion: 0.43
Nodes (8): Clip(), DecodeGuiFlags(), Heartbeat(), Hex32(), ProcName(), Stamp(), DWORD, wstring

## Knowledge Gaps
- **14 isolated node(s):** `btn`, `pt`, `flags`, `at`, `sub` (+9 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 23 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `ClickRec` connect `ClickRec` to `click_diag.cpp`, `wstring`, `OnClick`?**
  _High betweenness centrality (0.086) - this node is a cross-community bridge._
- **Why does `FindCtx` connect `HWND` to `click_diag.cpp`, `Log`?**
  _High betweenness centrality (0.043) - this node is a cross-community bridge._
- **What connects `btn`, `pt`, `flags` to the rest of the system?**
  _14 weakly-connected nodes found - possible documentation gaps or missing edges._