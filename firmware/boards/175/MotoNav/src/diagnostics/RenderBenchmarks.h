#pragma once
// 可选绘图基准测试入口。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
#if FPS_SELFTEST
#include "../../RenderBaseline.h"
#define RENDER_BASELINE_POLYLINE mapPolylineBefore
#include "../../RouteRangeBaseline.h"
#include "../../RenderSelfTest.h"
#endif
