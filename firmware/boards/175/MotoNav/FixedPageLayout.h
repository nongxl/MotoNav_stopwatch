#pragma once

// Screen positions: music(-1,0), navigation(0,0), weather(1,0),
// rain(1,-1), alerts(1,1). sign is the corrected finger movement.
namespace FixedPageLayout {
struct Target { int page; int detail; bool allowed; };
inline Target move(int page, int detail, bool horizontal, int sign) {
  Target none{page, detail, false};
  if (sign != -1 && sign != 1) return none;
  if (horizontal) {
    if (page == 2 && sign < 0) return {0, 0, true};
    if (page == 0) return sign < 0 ? Target{1, 0, true} : Target{2, 0, true};
    if (page == 1 && detail == 0 && sign > 0) return {0, 0, true};
    return none;
  }
  if (page != 1) return none;
  if (detail == 0) return {1, sign > 0 ? 1 : 2, true};
  if (detail == 1 && sign < 0) return {1, 0, true};
  if (detail == 2 && sign > 0) return {1, 0, true};
  return none;
}
inline int weatherRow(int detail) { return detail == 1 ? 0 : detail == 2 ? 2 : 1; }
}
