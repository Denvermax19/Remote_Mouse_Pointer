# Graph Report - Remote_Mouse_Pointer  (2026-10-04)

## Corpus Check
- 3 files · ~4,289 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 73 nodes · 186 edges · 11 communities (10 shown, 1 thin omitted)
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- WindowProc
- WindowProc
- click_diag.cpp
- wstring
- LogWindow
- MsgWndProc
- Heartbeat
- HWND
- ClickRec
- OnClick
- RectStr

## God Nodes (most connected - your core abstractions)
1. `OnClick()` - 18 edges
2. `LogWindow()` - 17 edges
3. `Heartbeat()` - 14 edges
4. `Log()` - 12 edges
5. `LogStack()` - 10 edges
6. `LogOverlays()` - 10 edges
7. `ProbePoint()` - 10 edges
8. `GetTitleW()` - 9 edges
9. `ClickRec` - 8 edges
10. `PtrStr()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `ClickRec` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 8 → community 6_
- `ClickRec` --references--> `POINT`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 8 → community 9_
- `CtrlHandler()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 7_
- `OnClick()` --references--> `DWORD`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 6 → community 9_
- `LogOverlays()` --references--> `POINT`  [EXTRACTED]
  click_diag.cpp →   _Bridges community 9 → community 3_

## Import Cycles
- None detected.

## Communities (11 total, 1 thin omitted)

### Community 0 - "WindowProc"
Cohesion: 0.25
Nodes (6): HWND, LPARAM, LRESULT, UINT, WPARAM, WindowProc()

### Community 1 - "WindowProc"
Cohesion: 0.24
Nodes (9): HWND, LPARAM, LRESULT, UINT, WPARAM, main(), StartWebSocket(), WindowProc() (+1 more)

### Community 2 - "click_diag.cpp"
Cohesion: 0.36
Nodes (7): FindBySub(), FindCtx, out, sub, HtName(), vector, wchar_t

### Community 3 - "wstring"
Cohesion: 0.67
Nodes (6): BoolStr(), Log(), LogOverlays(), main(), Stamp(), wstring

### Community 4 - "LogWindow"
Cohesion: 0.67
Nodes (4): DecodeEx(), DecodeSt(), LogWindow(), LONG_PTR

### Community 5 - "MsgWndProc"
Cohesion: 0.47
Nodes (6): LPARAM, LRESULT, UINT, WPARAM, HookProc(), MsgWndProc()

### Community 6 - "Heartbeat"
Cohesion: 0.32
Nodes (8): Clip(), DecodeGuiFlags(), Heartbeat(), Hex32(), ProcName(), WorkerProc(), DWORD, LPVOID

### Community 7 - "HWND"
Cohesion: 0.40
Nodes (5): BOOL, HWND, CtrlHandler(), FindProc(), GetClassW()

### Community 8 - "ClickRec"
Cohesion: 0.33
Nodes (6): ClickRec, at, btn, flags, pt, ULONGLONG

### Community 9 - "OnClick"
Cohesion: 0.40
Nodes (10): Contains(), GetTitleW(), LogStack(), OnClick(), ProbeHitTest(), ProbePoint(), PtrStr(), RgbStr() (+2 more)

## Knowledge Gaps
- **6 isolated node(s):** `btn`, `pt`, `flags`, `at`, `sub` (+1 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 20 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `ClickRec` connect `ClickRec` to `OnClick`, `click_diag.cpp`, `Heartbeat`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._
- **Why does `FindCtx` connect `click_diag.cpp` to `HWND`?**
  _High betweenness centrality (0.055) - this node is a cross-community bridge._
- **What connects `btn`, `pt`, `flags` to the rest of the system?**
  _6 weakly-connected nodes found - possible documentation gaps or missing edges._