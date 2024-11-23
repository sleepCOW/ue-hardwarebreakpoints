// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "HardwareBreakpointsBPLibrary.generated.h"

USTRUCT(BlueprintType)
struct FHardwareBreakpointHandle
{
	GENERATED_BODY()

private:
	UPROPERTY()
	int RegisterIndex = { -1 };

	UPROPERTY()
	int GlobalSalt = { -1 };

	UPROPERTY()
	int SlotSalt = { -1 };

public:
	void SetIndex(int Index);
	int GetIndex() const { return RegisterIndex; }
	bool IsCurrent();
	void Clear();
};

DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FHWBP_FloatCondition, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FHWBP_IntCondition, int, OldValue, int, NewValue);

/**
 * Exposed API for setting breakpoints in the debugger (accessible via the Immediate Window in Visual Studio).
 *
 * @note The breakpoint set is delayed until the next frame.
 * #TODO: Investigate ways to eliminate or minimize this delay (e.g., detecting when the debugger resumes the thread).
 */
extern "C" HARDWAREBREAKPOINTS_API bool SetDataBreakpoint(UObject* Object, TCHAR* PropertyPath);
extern "C" HARDWAREBREAKPOINTS_API bool SetFunctionBreakpoint(UClass* Class, TCHAR* FunctionName);
extern "C" HARDWAREBREAKPOINTS_API bool AnyHardwareBreakpointSet();
extern "C" HARDWAREBREAKPOINTS_API void ClearAllHardwareBreakpoints();

// Aliases for convenience

// Alias for SetDataBreakpoint
extern "C" inline HARDWAREBREAKPOINTS_API bool BP(UObject* Object, TCHAR* PropertyPath) { return SetDataBreakpoint(Object, PropertyPath); };
// Alias for SetFunctionBreakpoint
extern "C" inline HARDWAREBREAKPOINTS_API bool BPFunc(UClass* Class, TCHAR* FunctionName) { return SetFunctionBreakpoint(Class, FunctionName); };
// Alias for ClearAllHardwareBreakpoints
extern "C" inline HARDWAREBREAKPOINTS_API void ClearBP() { ClearAllHardwareBreakpoints(); }

/*
	This library provides functions to easily control hardware breakpoints programmatically from Blueprints or C++.
	
	The most useful type of breakpoint that has no equivalent on the software side is the Data Breakpoint.
	It can help you to find out what part of the code is causing a variable to change on a specific object.

	The library also allows you to set hardware breakpoints on Blueprint or native functions which will be triggered upon their invocation.
	
	The fact that you can control the breakpoints programmatically allows you to narrow down the cases that you want to inspect, 
	and the fact that they're implemented in hardware means that they will work 100%, as opposed to blueprint breakpoints which can be somewhat unreliable.

	Even if you're experienced in setting breakpoints by hand, using the debugger, this library will save you a lot of time navigating UE4's property system to find
	the right address to set.

	The limitations of hardware breakpoints are as follows:
	- They are platform specific. This plugin currently only supports the Windows platform (only tested on 64 bit Windows)
	- There can only be a limited number of breakpoints set at any one time (4 is the usual limit on modern desktop CPUs)

	For best results be sure to install engine source and debug symbols, so you have a more complete callstack on print outs, or your debugger
*/
UCLASS()
class HARDWAREBREAKPOINTS_API UHardwareBreakpointsBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//A data breakpoint allows you to see what part of the code is modifying a specific variable on any given object
	//If you have a debugger attached, you can look up the callstack to see the specific part of the code that is changing the data
	//This works best if you have the engine source and debug symbols
	UFUNCTION(BlueprintCallable, Category = "Hardware Breakpoints", meta = (DevelopmentOnly))
	static void SetDataBreakpoint(UObject* Object, FString PropertyPath, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle);

	//A data breakpoint allows you to see what part of the code is modifying a specific float variable on any given object that will trigger only when the specific condition is met
	//The condition is evaluated only once each time the variable changes value
	//If you have a debugger attached, you can look up the callstack to see the specific part of the code that is changing the data
	//This works best if you have the engine source and debug symbols
	UFUNCTION(BlueprintCallable, Category = "Hardware Breakpoints", meta = (DevelopmentOnly))
	static void SetFloatDataBreakpointWithCondition(UObject* Object, FString PropertyPath, FHWBP_FloatCondition Condition, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle);

	//A data breakpoint allows you to see what part of the code is modifying a specific int variable on any given object that will trigger only when the specific condition is met
	//The condition is evaluated only once each time the variable changes value
	//If you have a debugger attached, you can look up the callstack to see the specific part of the code that is changing the data
	//This works best if you have the engine source and debug symbols
	UFUNCTION(BlueprintCallable, Category = "Hardware Breakpoints", meta = (DevelopmentOnly))
	static void SetIntDataBreakpointWithCondition(UObject* Object, FString PropertyPath, FHWBP_IntCondition Condition, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle);

	//Detects when a NaN value is set to a float variable
	UFUNCTION(BlueprintCallable, Category = "Hardware Breakpoints", meta = (DevelopmentOnly, DisplayName = "Set NaN Data Breakpoint"))
	static void SetNaNDataBreakpoint(UObject* Object, FString PropertyPath, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle);

	//A function breakpoint can be set on BP or native functions. It will be hit upon any invocation of the function (with some exceptions in the case of native functions, see below)
	//If you have a debugger attached, you can look up the callstack to see the specific part of the code that is invoking the function
	//This works best if you have the engine source and debug symbols
	//
	//Note that in the case of native functions, the breakpoint will only be hit when the function is invoked from blueprints, or through a dynamic delegate
	//A direct invocation will not trigger the breakpoint
	//See FPlatformHardwareBreakpoints::SetNativeFunctionHardwareBreakpoint for an alternative that will work on other invocations (C++ only)
	UFUNCTION(BlueprintCallable, Category = "Hardware Breakpoints", meta = (DevelopmentOnly))
	static void SetFunctionBreakpoint(UClass* Class, FName FunctionName, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle);

	//Checks whether a specific hardware breakpoint is set
	UFUNCTION(BlueprintPure, Category = "Hardware Breakpoints", meta = (DevelopmentOnly, DisplayName="Is Valid"))
	static bool K2_IsBreakpointHandleValid(FHardwareBreakpointHandle BreakpointHandle);

	//Checks whether any hardware breakpoints are set
	//This function could be used as an anti debugging utility
	UFUNCTION(BlueprintPure, Category = "Hardware Breakpoints")
	static bool AnyHardwareBreakpointSet();

	//Clears a specific hardware breakpoint, does nothing if the breakpoint handle has already been cleared
	UFUNCTION(BlueprintCallable, Category = "Hardware Breakpoints", meta = (DevelopmentOnly))
	static void ClearHardwareBreakpoint(UPARAM(Ref) FHardwareBreakpointHandle& BreakpointHandle);

	//Clears all active hardware breakpoints
	UFUNCTION(BlueprintCallable, Category = "Hardware Breakpoints")
	static void ClearAllHardwareBreakpoints();
};
