# Graph Report - Remote_Mouse_Pointer  (2026-10-04)

## Corpus Check
- 3 files · ~4,770 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 76 nodes · 215 edges · 7 communities
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- WindowProc
- WindowProc
- click_diag.cpp
- wstring
- MsgWndProc
- Heartbeat
- OnClick

## God Nodes (most connected - your core abstractions)
1. `OnClick()` - 18 edges
2. `LogWindow()` - 17 edges
3. `Log()` - 15 edges
4. `ReportCaptionState()` - 14 edges
5. `Heartbeat()` - 14 edges
6. `GetTitleW()` - 10 edges
7. `LogStack()` - 10 edges
8. `FindBySub()` - 10 edges
9. `LogOverlays()` - 10 edges
10. `ProbePoint()` - 10 edges

## Surprising Connections (you probably didn't know these)
- `ClickRec` --references--> `POINT`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 9_
- `CtrlHandler()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 2_
- `Hex32()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 3_
- `ProbePoint()` --references--> `POINT`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 9 → community 3_
- `DecodeEx()` --references--> `wstring`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 3 → community 2_

## Import Cycles
- None detected.

## Communities (7 total, 0 thin omitted)

### Community 0 - "WindowProc"
Cohesion: 0.25
Nodes (6): HWND, LPARAM, LRESULT, UINT, WPARAM, WindowProc()

### Community 1 - "WindowProc"
Cohesion: 0.24
Nodes (9): HWND, LPARAM, LRESULT, UINT, WPARAM, main(), StartWebSocket(), WindowProc() (+1 more)

### Community 2 - "click_diag.cpp"
Cohesion: 0.25
Nodes (14): BOOL, HWND, CtrlHandler(), DecodeEx(), DecodeSt(), FindCtx, out, sub (+6 more)

### Community 3 - "wstring"
Cohesion: 0.40
Nodes (14): FindBySub(), Hex32(), Log(), main(), ProbePoint(), ProcName(), ReapplyColorKey(), ReportCaptionState() (+6 more)

### Community 5 - "MsgWndProc"
Cohesion: 0.38
Nodes (7): LPARAM, LRESULT, UINT, WPARAM, HookProc(), HtName(), MsgWndProc()

### Community 6 - "Heartbeat"
Cohesion: 0.20
Nodes (12): ClickRec, at, btn, flags, pt, Clip(), DecodeGuiFlags(), Heartbeat() (+4 more)

### Community 9 - "OnClick"
Cohesion: 0.40
Nodes (10): BoolStr(), Contains(), LogOverlays(), LogStack(), OnClick(), ProbeHitTest(), PtrStr(), RectStr() (+2 more)

## Knowledge Gaps
- **6 isolated node(s):** `btn`, `pt`, `flags`, `at`, `sub` (+1 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 20 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `ClickRec` connect `Heartbeat` to `OnClick`, `click_diag.cpp`?**
  _High betweenness centrality (0.099) - this node is a cross-community bridge._
- **Why does `FindCtx` connect `click_diag.cpp` to `wstring`?**
  _High betweenness centrality (0.051) - this node is a cross-community bridge._
- **What connects `btn`, `pt`, `flags` to the rest of the system?**
  _6 weakly-connected nodes found - possible documentation gaps or missing edges._