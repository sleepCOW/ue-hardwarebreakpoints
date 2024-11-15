// Copyright Daniel Amthauer. All Rights Reserved.

#include "WindowsPlatformHardwareBreakpointsUser.h"

#include "HAL/PlatformMisc.h"

#include "../Settings/HWBP_Settings.h"

//We disable optimization here, so we can ensure an accurate callstack for the user
PRAGMA_DISABLE_OPTIMIZATION

void WindowsPlatformHardwareBreakpoints::CaughtBlueprintFunctionBreakpoint()
{
	if (GetDefault<UHWBP_Settings>()->DontBreakEvenIfDebuggerAttached)
		return;
	//If your debugger breaks here, the blueprint function you were watching was called
	//Check up the callstack to see where it was being called from
	UE_DEBUG_BREAK();
}

void WindowsPlatformHardwareBreakpoints::CaughtNativeFunctionBreakpoint()
{
	if (GetDefault<UHWBP_Settings>()->DontBreakEvenIfDebuggerAttached)
		return;
	//If your debugger breaks here, the native function you were watching was called
	//Check up the callstack to see where it was being called from
	UE_DEBUG_BREAK();
}

void WindowsPlatformHardwareBreakpoints::CaughtDataBreakpoint()
{
	if (GetDefault<UHWBP_Settings>()->DontBreakEvenIfDebuggerAttached)
		return;
	//If your debugger breaks here, it must have been a data breakpoint you set
	//Check up the callstack to see where the data was modified
	UE_DEBUG_BREAK();
}

void WindowsPlatformHardwareBreakpoints::ClearBreakpoints(FBreakpointClearData& OutData)
{
	//Place a breakpoint in this function to get a chance to disable a breakpoint after it's been triggered
	//Setting OutData.ClearBreakpoint[i] will clear breakpoint i
	//Setting OutData.ClearAllBreakpoints to true will clear all breakpoints
}

PRAGMA_ENABLE_OPTIMIZATION
