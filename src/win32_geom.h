#pragma once
// Conversions between the Win32-free Rect / Point (common.h) and RECT / POINT. Used at the
// Win32 boundary (overlay, main); layout.* never includes this.
#include <windows.h>

#include "common.h"

inline Rect ToRect(const RECT& r) { return Rect{r.left, r.top, r.right, r.bottom}; }
inline RECT ToRECT(const Rect& r) { return RECT{r.left, r.top, r.right, r.bottom}; }
inline Point ToPoint(const POINT& p) { return Point{p.x, p.y}; }
