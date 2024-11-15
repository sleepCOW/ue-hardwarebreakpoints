// Copyright Daniel Amthauer. All Rights Reserved.

#include "HardwareBreakpointsBPLibrary.h"

#include "HAL/PlatformMisc.h"

#include "HardwareBreakpoints.h"
#include "PropertyHelpers.h"
#include "HAL/PlatformHardwareBreakpoints.h"
#include "CallStackViewer.h"
#include "HWBP_Dialogs.h"
#include "HardwareBreakpointsLog.h"

using namespace PropertyHelpers;


static void ShowPropertyNotFoundMessageDialog(FString PropertyPath, UObject* Object)
{
#define LOCTEXT_NAMESPACE "HardwareBreakpoints_PropertyNotFound"
	OpenMsgDlgInt(EAppMsgType::Ok, FText::Format(LOCTEXT("DialogText", "Data breakpoint not set: Couldn't find property {0} for object {1}"), FText::FromString(PropertyPath), FText::FromString(Object->GetName())), LOCTEXT("DialogTitle", "Property not found"));
#undef LOCTEXT_NAMESPACE
}

void UHardwareBreakpointsBPLibrary::SetDataBreakpoint(UObject* Object, FString PropertyPath, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle)
{
	if (Object == nullptr)
	{
		UE_LOG(LogHardwareBreakpoints, Error, TEXT("Tried to set a data breakpoint on an invalid object"));
		bSuccess = false;
		return;
	}
	FPropertyAddress PropertyAddress = FindPropertyAddress(Object, Object->GetClass(), PropertyPath);
	if (PropertyAddress.Address == nullptr)
	{
		ShowPropertyNotFoundMessageDialog(PropertyPath, Object);
		bSuccess = false;
		return;
	}
	DebugRegisterIndex Index = FPlatformHardwareBreakpoints::SetDataBreakpoint(PropertyAddress.Address, PropertyAddress.Property->GetSize(), Object);
	BreakpointHandle.SetIndex(Index);
	bSuccess = Index >= 0;
}

void UHardwareBreakpointsBPLibrary::SetFloatDataBreakpointWithCondition(UObject* Object, FString PropertyPath,
	FHWBP_FloatCondition Condition, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle)
{
	if (Object == nullptr)
	{
		UE_LOG(LogHardwareBreakpoints, Error, TEXT("Tried to set a data breakpoint on an invalid object"));
		bSuccess = false;
		return;
	}
	FPropertyAddress PropertyAddress = FindPropertyAddress(Object, Object->GetClass(), PropertyPath);
	if (PropertyAddress.Address == nullptr)
	{
		ShowPropertyNotFoundMessageDialog(PropertyPath, Object);
		bSuccess = false;
		return;
	}
	//Only works on float properties
	if (Cast<UFloatProperty>(PropertyAddress.Property) == nullptr)
	{
		UE_LOG(LogHardwareBreakpoints, Error, TEXT("Set Float Data Breakpoint With Condition called on a property that doesn't point to a float value %s"), *PropertyPath);
		bSuccess = false;
		return;
	}
	DebugRegisterIndex Index = FPlatformHardwareBreakpoints::SetDataBreakpointWithCondition((float*)PropertyAddress.Address, [=](const float LastValue, const float Value)
	{
		return Condition.IsBound() ? Condition.Execute(LastValue, Value) : false;
	}, Object);
	BreakpointHandle.SetIndex(Index);
	bSuccess = Index >= 0;
}

void UHardwareBreakpointsBPLibrary::SetIntDataBreakpointWithCondition(UObject* Object, FString PropertyPath,
	FHWBP_IntCondition Condition, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle)
{
	if (Object == nullptr)
	{
		UE_LOG(LogHardwareBreakpoints, Error, TEXT("Tried to set a data breakpoint on an invalid object"));
		bSuccess = false;
		return;
	}
	FPropertyAddress PropertyAddress = FindPropertyAddress(Object, Object->GetClass(), PropertyPath);
	if (PropertyAddress.Address == nullptr)
	{
		ShowPropertyNotFoundMessageDialog(PropertyPath, Object);
		bSuccess = false;
		return;
	}
	//Only works on float properties
	if (Cast<UIntProperty>(PropertyAddress.Property) == nullptr)
	{
		UE_LOG(LogHardwareBreakpoints, Error, TEXT("Set Int Data Breakpoint With Condition called on a property that doesn't point to an int value %s"), *PropertyPath);
		bSuccess = false;
		return;
	}
	DebugRegisterIndex Index = FPlatformHardwareBreakpoints::SetDataBreakpointWithCondition((int*)PropertyAddress.Address, [=](const int LastValue, const int Value)
	{
		return Condition.IsBound() ? Condition.Execute(LastValue, Value) : false;
	}, Object);
	BreakpointHandle.SetIndex(Index);
	bSuccess = Index >= 0;
}

void UHardwareBreakpointsBPLibrary::SetNaNDataBreakpoint(UObject* Object, FString PropertyPath, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle)
{
	if (Object == nullptr)
	{
		UE_LOG(LogHardwareBreakpoints, Error, TEXT("Tried to set a data breakpoint on an invalid object"));
		bSuccess = false;
		return;
	}
	FPropertyAddress PropertyAddress = FindPropertyAddress(Object, Object->GetClass(), PropertyPath);
	if (PropertyAddress.Address == nullptr)
	{
		ShowPropertyNotFoundMessageDialog(PropertyPath, Object);
		bSuccess = false;
		return;
	}
	//Only works on float properties
	if (Cast<UFloatProperty>(PropertyAddress.Property) == nullptr)
	{
		UE_LOG(LogHardwareBreakpoints, Error, TEXT("Set NaN Data Breakpoint called on a property that doesn't point to a float value %s"), *PropertyPath);
		bSuccess = false;
		return;
	}
	DebugRegisterIndex Index = FPlatformHardwareBreakpoints::SetDataBreakpointWithCondition((float*)PropertyAddress.Address, [](const float LastValue, const float Value)
	{
		return FMath::IsNaN(Value);
	}, Object);
	BreakpointHandle.SetIndex(Index);
	bSuccess = Index >= 0;
}

void UHardwareBreakpointsBPLibrary::SetFunctionBreakpoint(UClass* Class, FName FunctionName, bool& bSuccess, FHardwareBreakpointHandle& BreakpointHandle)
{
	if (Class == nullptr)
	{
		bSuccess = false;
		return;
	}
	UFunction* Function = Class->FindFunctionByName(FunctionName);
	bSuccess = false;
	void* Address;
	EHardwareBreakpointType Type;
	if (Function->HasAnyFunctionFlags(FUNC_Native))
	{ 
		Address = (void*)Function->GetNativeFunc();
		Type = EHardwareBreakpointType::Execute;
	}
	else
	{
		Address = Function->Script.GetData();
		Type = EHardwareBreakpointType::ReadWrite;
	}
	DebugRegisterIndex Index = FPlatformHardwareBreakpoints::SetHardwareBreakpoint(Type, EHardwareBreakpointSize::Size_1, Address);
	BreakpointHandle.SetIndex(Index);
	bSuccess = Index >= 0;
}

namespace HardwareBreakpointsUtils
{
	//Used to invalidate all breakpoint handles
	static int GlobalHandleSalt = 0;

	static int PerSlotHandleSalt[MAX_HARDWARE_BREAKPOINTS] = { 0 };

	FDelegateHandle SlotSaltDelegateHandle;
}

bool UHardwareBreakpointsBPLibrary::K2_IsBreakpointHandleValid(FHardwareBreakpointHandle BreakpointHandle)
{
	if (BreakpointHandle.IsCurrent())
	{
		return FPlatformHardwareBreakpoints::IsBreakpointSet(BreakpointHandle.GetIndex());
	}
	return false;
}

bool UHardwareBreakpointsBPLibrary::AnyHardwareBreakpointSet()
{
	return FPlatformHardwareBreakpoints::AnyBreakpointSet();
}

void UHardwareBreakpointsBPLibrary::ClearHardwareBreakpoint(FHardwareBreakpointHandle& BreakpointHandle)
{
	if (BreakpointHandle.IsCurrent())
	{
		FPlatformHardwareBreakpoints::RemoveHardwareBreakpoint(BreakpointHandle.GetIndex());
	}
	BreakpointHandle.Clear();
}

void UHardwareBreakpointsBPLibrary::ClearAllHardwareBreakpoints()
{
	FPlatformHardwareBreakpoints::RemoveAllHardwareBreakpoints();
	//Invalidate all handles so they can't be used to clear a breakpoint they shouldn't be pointing to
	++HardwareBreakpointsUtils::GlobalHandleSalt;
}

void FHardwareBreakpointHandle::SetIndex(DebugRegisterIndex Index)
{
	RegisterIndex = Index;
	if (Index >= 0 && Index < MAX_HARDWARE_BREAKPOINTS)
	{
		if (!HardwareBreakpointsUtils::SlotSaltDelegateHandle.IsValid())
		{
			HardwareBreakpointsUtils::SlotSaltDelegateHandle = CallStackViewer::OnRemoveBreakpoint.AddLambda([](DebugRegisterIndex Index) {
				++HardwareBreakpointsUtils::PerSlotHandleSalt[Index];
			});
		}
		GlobalSalt = HardwareBreakpointsUtils::GlobalHandleSalt;
		SlotSalt = HardwareBreakpointsUtils::PerSlotHandleSalt[Index];
	}
}
bool FHardwareBreakpointHandle::IsCurrent()
{
	using namespace HardwareBreakpointsUtils;
	return RegisterIndex >= 0 && GlobalSalt == GlobalHandleSalt && SlotSalt == PerSlotHandleSalt[RegisterIndex];
}

void FHardwareBreakpointHandle::Clear()
{
	RegisterIndex = -1;
}
