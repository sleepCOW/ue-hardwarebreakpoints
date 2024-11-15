// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#ifndef MAX_HARDWARE_BREAKPOINTS
#define MAX_HARDWARE_BREAKPOINTS 4
#endif

#include "GenericPlatform/GenericPlatformHardwareBreakpoints.h"

struct HARDWAREBREAKPOINTS_API FWindowsPlatformHardwareBreakpoints : public FGenericPlatformHardwareBreakpoints
{
	template <typename R, typename T, typename... Args>
	static DebugRegisterIndex SetNativeFunctionHardwareBreakpoint(R(T::*Func)(Args...))
	{
		//This is not allowed by the compiler, and is implementation dependent, but it works in practice
		void* Address = reinterpret_cast<void*&>(Func);
		return SetHardwareBreakpoint(EHardwareBreakpointType::Execute, EHardwareBreakpointSize::Size_1, Address);
	}
	static DebugRegisterIndex SetHardwareBreakpoint(EHardwareBreakpointType Type, EHardwareBreakpointSize Size, void* Address);
	static bool IsBreakpointSet(DebugRegisterIndex Index);
	static bool AnyBreakpointSet();
	static bool RemoveHardwareBreakpoint(DebugRegisterIndex Index);
	static bool RemoveAllHardwareBreakpoints();
	static void AddStructuredExceptionHandler();
	static void RemoveStructuredExceptionHandler();

	static int32 GetSymbolDisplacementForProgramCounter(uint64 ProgramCounter);
	static uint64 GetAddressFromSymbolName(const CHAR* SymbolName);

	static bool IsStackWalkingInitialized();
};

typedef FWindowsPlatformHardwareBreakpoints FPlatformHardwareBreakpoints;