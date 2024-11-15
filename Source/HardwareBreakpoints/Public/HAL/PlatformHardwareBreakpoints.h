// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformHardwareBreakpoints.h"
#else
#include "GenericPlatform/GenericPlatformHardwareBreakpoints.h"
typedef FGenericPlatformHardwareBreakpoints FPlatformHardwareBreakpoints;
#endif

struct FScopedHardwareBreakpoint
{
	FScopedHardwareBreakpoint(DebugRegisterIndex InRegisterIndex) : RegisterIndex(InRegisterIndex)
	{

	}
	~FScopedHardwareBreakpoint()
	{
		FPlatformHardwareBreakpoints::RemoveHardwareBreakpoint(RegisterIndex);
	}
	operator DebugRegisterIndex()
	{
		return RegisterIndex;
	}
	void Reset()
	{
		FPlatformHardwareBreakpoints::RemoveHardwareBreakpoint(RegisterIndex);
		RegisterIndex = -1;
	}
private:
	DebugRegisterIndex RegisterIndex;
};
