// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/WeakObjectPtr.h"

#ifndef MAX_HARDWARE_BREAKPOINTS
#define MAX_HARDWARE_BREAKPOINTS 1
#endif

enum class EHardwareBreakpointType
{
	Execute,
	ReadWrite,
	Write,
};

enum class EHardwareBreakpointSize
{
	Size_1,
	Size_2,
	Size_4,
	Size_8,
};
typedef int DebugRegisterIndex;

class IHardwareBreakpointCondition
{
public:
	virtual bool ShouldBreak(uint8 (&LastValue)[8], void* BreakpointAddress) = 0;
	virtual ~IHardwareBreakpointCondition() {};
};

template <typename T, typename L>
class TDataBreakpointConditionAdapter : public IHardwareBreakpointCondition
{
	L Lambda;
public:
	TDataBreakpointConditionAdapter(const L& InLambda) 
		: Lambda(InLambda)
	{
	}

	virtual bool ShouldBreak(uint8 (&LastValue)[8], void* BreakpointAddress) override
	{
		const T& TypedData = *reinterpret_cast<const T*>(BreakpointAddress);
		const T& TypedLastValue = *reinterpret_cast<const T*>(LastValue);
		return Lambda(TypedLastValue, TypedData);
	}
};

struct HARDWAREBREAKPOINTS_API FGenericPlatformHardwareBreakpoints
{
	template <typename R, typename T, typename... Args>
	static DebugRegisterIndex SetNativeFunctionHardwareBreakpoint(R(T::*Func)(Args...)) { return -1; }

	template <typename T>
	static DebugRegisterIndex SetDataBreakpoint(T* TypedAddress, UObject* Owner = nullptr)
	{
		return SetDataBreakpoint(TypedAddress, sizeof(T), Owner);
	}

	template <typename T, typename L>
	static DebugRegisterIndex SetDataBreakpointWithCondition(T* TypedAddress, const L& TypedCondition, UObject* Owner = nullptr)
	{
		if (!TypedAddress)
		{
			return -1;
		}
		DebugRegisterIndex Index = SetDataBreakpoint(TypedAddress, Owner);
		if (Index >= 0)
		{
			DataBreakpointInfo[Index].Condition = new TDataBreakpointConditionAdapter<T, L>(TypedCondition);
		}
		return Index;
	}

	static DebugRegisterIndex SetDataBreakpoint(void* Address, int DataSize, UObject* Owner = nullptr);

	static DebugRegisterIndex SetHardwareBreakpoint(EHardwareBreakpointType Type, EHardwareBreakpointSize Size, void* Address) { return -1; }
	static bool IsBreakpointSet(DebugRegisterIndex Index) { return false; }
	static bool AnyBreakpointSet() { return false; }
	static bool RemoveHardwareBreakpoint(DebugRegisterIndex Index) { return false; }
	static bool RemoveAllHardwareBreakpoints() { return false; }
	static void AddStructuredExceptionHandler() {}
	static void RemoveStructuredExceptionHandler() {}
	static int32 GetSymbolDisplacementForProgramCounter(uint64 ProgramCounter) { return 0; }


	//Internal
	static void RemoveBreakpointAssociatedData(DebugRegisterIndex Index);
	static void RemoveAllBreakpointAssociatedData();
	static bool CheckDataBreakpointConditions(DebugRegisterIndex& OutRegisterIndex);

protected:

	struct FDataBreakpointInfo
	{
		FWeakObjectPtr Owner;
		bool bHasOwner;
		void* Address = { nullptr };
		uint8 LastValue[8] = {0};
		int Size = 0;
		IHardwareBreakpointCondition* Condition = { nullptr };
	};
	static FDataBreakpointInfo DataBreakpointInfo[MAX_HARDWARE_BREAKPOINTS];
};
