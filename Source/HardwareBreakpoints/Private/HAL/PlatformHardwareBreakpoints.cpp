// Copyright Daniel Amthauer. All Rights Reserved.

#include "HAL/PlatformHardwareBreakpoints.h"

#include "Misc/ScopeExit.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 20
	#include "GenericPlatform/GenericPlatformMath.h"
#else
	#include "GenericPlatformMath.h"
#endif

//This stuff depends on the value of MAX_HARDWARE_BREAKPOINTS which is defined per platform, so this has to be here instead of in GenericPlatformHardwareBreakpoints.cpp

template <typename T>
static void SafeDelete(T*& Ptr)
{
	if (Ptr)
	{
		delete Ptr;
		Ptr = nullptr;
	}
}

void FGenericPlatformHardwareBreakpoints::RemoveBreakpointAssociatedData(DebugRegisterIndex Index)
{
	if (Index >= 0 && Index < MAX_HARDWARE_BREAKPOINTS)
	{
		DataBreakpointInfo[Index].Address = nullptr;
		SafeDelete(DataBreakpointInfo[Index].Condition);
	}
}

void FGenericPlatformHardwareBreakpoints::RemoveAllBreakpointAssociatedData()
{
	for (int i = 0; i < MAX_HARDWARE_BREAKPOINTS; ++i)
	{
		DataBreakpointInfo[i].Address = nullptr;
		SafeDelete(DataBreakpointInfo[i].Condition);
	}
}

PRAGMA_DISABLE_OPTIMIZATION
bool FGenericPlatformHardwareBreakpoints::CheckDataBreakpointConditions(int& OutRegisterIndex, struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	const int maxBreakpoints = MAX_HARDWARE_BREAKPOINTS;
	for (int i = 0; i < maxBreakpoints; ++i)
	{
		if (DataBreakpointInfo[i].Address)
		{
			//We check the current value of the memory for each data breakpoint compared to its last known value
			//(we store it when the breakpoint is set)
			//So if it's different, then we can assume it's because this breakpoint was hit
			//If it has a condition, we evaluate that instead of just returning true
			//We still check other breakpoints if this one's condition fails, because other breakpoints might have been modified at once

			// @sleepCOW: We need to implement a more accurate method to determine the cause of breakpoints. 
			// The current implementation overlooks cases where memory is written with the same value, 
			// which I have found to be problematic.
			//
			// A proper solution would involve analyzing the address where the exception occurred, 
			// disassembling the previous instruction to identify where the address was stored, 
			// and comparing it with the saved address in `DataBreakpointInfo`. 
			// However, this approach is hard to implement, and I'm lazy to go the long way for not so much benefit.
			//
			// As a simpler (but potentially error-prone) alternative, 
			// I use a heuristic that searches through integer registers (where the address of 
			// the variable might have been stored). If a match is found, we can, with a certain 
			// degree of probability, infer that the breakpoint corresponds to the data breakpoint we set.
			//
			// Potential false positives include:
			// 1. Cases where the value matches because a function stores the address into a register 
			//    without actually writing to it.
			const void* BreakpointAddress = DataBreakpointInfo[i].Address;
			if (FPlatformHardwareBreakpoints::IsAnyRegistersContainOurBreakpointAddress(BreakpointAddress, ExceptionInfo) || FMemory::Memcmp(DataBreakpointInfo[i].LastValue, BreakpointAddress, DataBreakpointInfo[i].Size) != 0)
			{
				//If the breakpoint has a weak reference to an owning object, verify that it's still valid
				//This is so we don't fire a data breakpoint on an object only because it's being freed
				//TODO: maybe add a parameter to the BP functions, so they optionally don't set this, in case they want to detect object deletion (not very useful, but still)
				if (!DataBreakpointInfo[i].bHasOwner || DataBreakpointInfo[i].Owner.IsValid())
				{
					//I might extend conditions to receive the last value as well, but for now this is on scope exit so you can still look at the last value with the debugger
					ON_SCOPE_EXIT
					{
						FMemory::Memcpy(DataBreakpointInfo[i].LastValue, DataBreakpointInfo[i].Address, DataBreakpointInfo[i].Size);
					};
					if (DataBreakpointInfo[i].Condition)
					{
						if (DataBreakpointInfo[i].Condition->ShouldBreak(DataBreakpointInfo[i].LastValue, DataBreakpointInfo[i].Address))
						{
							OutRegisterIndex = i;
							return true;
						}
					}
					else
					{
						OutRegisterIndex = i;
						return true;
					}
				}
				//If the owner reference was set, the owner is not valid, and the breakpoint has triggered, we should remove this breakpoint as it's now pointing to
				//freed memory
				else if (DataBreakpointInfo[i].bHasOwner)
				{
					FPlatformHardwareBreakpoints::RemoveHardwareBreakpoint(i);
				}
			}
		}
	}
	return false;
}
PRAGMA_ENABLE_OPTIMIZATION

DebugRegisterIndex FGenericPlatformHardwareBreakpoints::SetDataBreakpoint(void* Address, int DataSize, UObject* Owner)
{
	DataSize = FGenericPlatformMath::Min(8, DataSize);
	EHardwareBreakpointSize BreakpointSize = EHardwareBreakpointSize::Size_8;
	if (DataSize <= 4)
	{
		BreakpointSize = EHardwareBreakpointSize::Size_4;
	}
	if (DataSize <= 2)
	{
		BreakpointSize = EHardwareBreakpointSize::Size_2;
	}
	if (DataSize == 1)
	{
		BreakpointSize = EHardwareBreakpointSize::Size_1;
	}
	DebugRegisterIndex Index = FPlatformHardwareBreakpoints::SetHardwareBreakpoint(EHardwareBreakpointType::Write, BreakpointSize, Address);
	if (Index >= 0)
	{
		DataBreakpointInfo[Index].Owner = Owner;
		DataBreakpointInfo[Index].bHasOwner = Owner != nullptr;
		DataBreakpointInfo[Index].Address = Address;
		DataBreakpointInfo[Index].Size = DataSize;
		FMemory::Memcpy(DataBreakpointInfo[Index].LastValue, Address, DataSize);
	}
	return Index;
}


FGenericPlatformHardwareBreakpoints::FDataBreakpointInfo FGenericPlatformHardwareBreakpoints::DataBreakpointInfo[MAX_HARDWARE_BREAKPOINTS];