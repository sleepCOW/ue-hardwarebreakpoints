// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

namespace WindowsPlatformHardwareBreakpoints
{
	void CaughtBlueprintFunctionBreakpoint();
	void CaughtNativeFunctionBreakpoint();
	void CaughtDataBreakpoint();

	struct FBreakpointClearData
	{
		bool ClearBreakpoint[4] = { 0 };
		bool ClearAllBreakpoints = { false };
	};

	void ClearBreakpoints(FBreakpointClearData& OutData);
}