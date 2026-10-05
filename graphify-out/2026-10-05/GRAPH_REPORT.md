# Graph Report - Remote_Mouse_Pointer  (2026-10-05)

## Corpus Check
- 4 files · ~8,239 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 94 nodes · 245 edges · 8 communities
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `83c8dabf`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- WindowProc
- WindowProc
- Session log — ScreenPointer teacher title bar won't drag
- ReportCaptionState
- ClickRec
- GetTitleW
- click_diag.cpp
- OnClick

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
  click_diag.cpp →   _Bridges community 4 → community 6_
- `ClickRec` --references--> `POINT`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 4 → community 9_
- `OnClick()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 9_
- `GetTitleW()` --references--> `wstring`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 5_
- `main()` --calls--> `wstring`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 3_

## Import Cycles
- None detected.

## Communities (8 total, 0 thin omitted)

### Community 0 - "WindowProc"
Cohesion: 0.25
Nodes (6): HWND, LPARAM, LRESULT, UINT, WPARAM, WindowProc()

### Community 1 - "WindowProc"
Cohesion: 0.21
Nodes (17): ID2D1RenderTarget, ApplyDonutRegion(), CaptionButtons(), HWND, LPARAM, LRESULT, RECT, UINT (+9 more)

### Community 2 - "Session log — ScreenPointer teacher title bar won't drag"
Cohesion: 0.20
Nodes (9): Bug as reported by user, Definitive results from `build/click_diag_log.txt`, Diagnostic tool (only if a re-run is needed), Fix implemented — Option C, donut region + no colour key (BUILD 1 verified working), Next steps, Option A (client drag strip) — built, tested by user, FAILED, Other open items (unchanged), Project (+1 more)

### Community 3 - "ReportCaptionState"
Cohesion: 0.40
Nodes (10): FindBySub(), FindCtx, out, sub, main(), ReapplyColorKey(), ReportCaptionState(), ToggleLayered() (+2 more)

### Community 4 - "ClickRec"
Cohesion: 0.33
Nodes (6): ClickRec, at, btn, flags, pt, ULONGLONG

### Community 5 - "GetTitleW"
Cohesion: 0.25
Nodes (11): BOOL, HWND, LPARAM, LRESULT, UINT, WPARAM, FindProc(), GetTitleW() (+3 more)

### Community 6 - "click_diag.cpp"
Cohesion: 0.25
Nodes (20): BoolStr(), Clip(), CtrlHandler(), DecodeEx(), DecodeGuiFlags(), DecodeSt(), GetClassW(), Heartbeat() (+12 more)

### Community 9 - "OnClick"
Cohesion: 0.49
Nodes (10): Contains(), RECT, Log(), LogOverlays(), LogStack(), OnClick(), ProbeHitTest(), ProbePoint() (+2 more)

## Knowledge Gaps
- **14 isolated node(s):** `btn`, `pt`, `flags`, `at`, `sub` (+9 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 29 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `ClickRec` connect `ClickRec` to `OnClick`, `click_diag.cpp`?**
  _High betweenness centrality (0.073) - this node is a cross-community bridge._
- **Why does `FindCtx` connect `ReportCaptionState` to `GetTitleW`, `click_diag.cpp`?**
  _High betweenness centrality (0.037) - this node is a cross-community bridge._
- **What connects `btn`, `pt`, `flags` to the rest of the system?**
  _14 weakly-connected nodes found - possible documentation gaps or missing edges._