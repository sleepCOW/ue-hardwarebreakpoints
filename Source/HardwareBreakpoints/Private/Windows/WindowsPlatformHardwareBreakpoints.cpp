// Copyright Daniel Amthauer. All Rights Reserved.

#include "Windows/WindowsPlatformHardwareBreakpoints.h"

#include "HAL/PlatformStackWalk.h"
#include "Windows/AllowWindowsPlatformTypes.h"
	#include <DbgHelp.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Internationalization/Text.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"

#include "WindowsPlatformHardwareBreakpointsUser.h"
#include "Misc/HWBP_Build.h"

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 20
#include "HardwareBreakpointsLog.h"
#include "HWBP_Dialogs.h"
#include "CallStackViewer.h"
#include "GenericPlatform/ExtendedProgramCounterSymbolInfo.h"
#else
#include "Private/HardwareBreakpointsLog.h"
#include "Private/HWBP_Dialogs.h"
#include "Private/CallStackViewer.h"
#include "Private/GenericPlatform/ExtendedProgramCounterSymbolInfo.h"
#endif
#include "../Settings/HWBP_Settings.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
PRAGMA_DISABLE_OPTIMIZATION
#endif

enum EDebugRegisterOperation
{
	Set,
	Remove,
	RemoveAll
};

struct FHardwareBreakpointData
{
	void* Address = { nullptr };
	HANDLE ThreadHandle = { nullptr };
	EHardwareBreakpointType Type;
	EHardwareBreakpointSize Size;
	int RegisterIndex = { 0 };
	EDebugRegisterOperation OperationToPerform = { EDebugRegisterOperation::Set };
	bool Success = { false };
	bool RegistersChanged = { false };
};

template <typename T>
static void SetBits(T& Register, int LowBit, int BitCount, int NewBits)
{
	T Mask = (1 << BitCount) - 1; 
	Register = (Register & ~(Mask << LowBit)) | (NewBits << LowBit);
}

static DWORD WINAPI ApplyDebugRegisterChanges(LPVOID Parameter)
{
	FHardwareBreakpointData* Data = (FHardwareBreakpointData*)Parameter;
	BOOL LastCallResult = 0;
	DWORD LastError = 0;

	LastCallResult = SuspendThread(Data->ThreadHandle);
    LastError = GetLastError();

	CONTEXT Context = {0};
	Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	LastCallResult = GetThreadContext(Data->ThreadHandle, &Context);
    LastError = GetLastError();

	bool Busy[4];

	Busy[0] = (Context.Dr7 & 1) != 0;
	Busy[1] = (Context.Dr7 & 4) != 0;
	Busy[2] = (Context.Dr7 & 16) != 0;
	Busy[3] = (Context.Dr7 & 64) != 0;

	auto DebugRegisters = &Context.Dr0;

	switch (Data->OperationToPerform)
	{
	case EDebugRegisterOperation::Set:
		{
			for (; Data->RegisterIndex < 4; ++Data->RegisterIndex)
			{
				if (!Busy[Data->RegisterIndex])
					break;
			}
			if (Data->RegisterIndex < 4)
			{
				DebugRegisters[Data->RegisterIndex] = (DWORD_PTR)Data->Address;
			}
			else
			{
				Data->Success = false;
				LastCallResult = ResumeThread(Data->ThreadHandle);
				LastError = GetLastError();
				return 0;
			}
			Context.Dr6 = 0;
			int TypeBits;
			switch (Data->Type)
			{
			case EHardwareBreakpointType::Execute:		TypeBits = 0; break;
			case EHardwareBreakpointType::ReadWrite:	TypeBits = 3; break;
			case EHardwareBreakpointType::Write:		TypeBits = 1; break;
			default: TypeBits = 0; break;
			}
			int SizeBits;
			switch (Data->Size)
			{
			case EHardwareBreakpointSize::Size_1: SizeBits = 0; break;
			case EHardwareBreakpointSize::Size_2: SizeBits = 1; break;
			case EHardwareBreakpointSize::Size_4: SizeBits = 3; break;
			case EHardwareBreakpointSize::Size_8: SizeBits = 2; break;
			default: SizeBits = 0; break;;
			}

			SetBits(Context.Dr7, 16 + Data->RegisterIndex * 4, 2, TypeBits);
			SetBits(Context.Dr7, 18 + Data->RegisterIndex * 4, 2, SizeBits);
			SetBits(Context.Dr7, Data->RegisterIndex * 2, 1, 1);
		}
		break;

	case EDebugRegisterOperation::Remove:
		DebugRegisters[Data->RegisterIndex] = 0;
		Data->RegistersChanged = Busy[Data->RegisterIndex];
		Context.Dr7 &= ~(1 << (Data->RegisterIndex * 2));
		break;

	case EDebugRegisterOperation::RemoveAll:
		for (int i = 0; i < 4; ++i)
		{
			DebugRegisters[i] = 0;
			Data->RegistersChanged = Data->RegistersChanged || Busy[i];
			Context.Dr7 &= ~(1 << (i * 2));
		}
		break;
	}

	Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	LastCallResult = SetThreadContext(Data->ThreadHandle,&Context);
    LastError = GetLastError();

	LastCallResult = ResumeThread(Data->ThreadHandle);
    LastError = GetLastError();

	Data->Success = true;
	return 0;
}

DebugRegisterIndex FWindowsPlatformHardwareBreakpoints::SetHardwareBreakpoint(EHardwareBreakpointType Type,EHardwareBreakpointSize Size,void* Address)
{
	HANDLE hThread = GetCurrentThread();
	FHardwareBreakpointData Data;
	Data.Address = Address;
	Data.Size = Size;
	Data.Type = Type;

	DWORD pid = GetCurrentThreadId();
	Data.ThreadHandle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, 0, pid);

	Data.OperationToPerform = EDebugRegisterOperation::Set;
	HANDLE OperationThread = CreateThread(0,0,ApplyDebugRegisterChanges,(LPVOID)&Data,0,0);
	WaitForSingleObject(OperationThread,INFINITE);

	CloseHandle(OperationThread);
	CloseHandle(Data.ThreadHandle);

	if (!Data.Success)
	{
		return -1;
	}
	return Data.RegisterIndex;
}

namespace HardwareBreakpointsUtils
{
	struct FFetchContextData
	{
		HANDLE ThreadHandle;
		CONTEXT Context;
	};

	static DWORD WINAPI FetchContextThread(LPVOID lpParameter)
	{
		using namespace HardwareBreakpointsUtils;
		FFetchContextData* Data = (FFetchContextData*)lpParameter;
		BOOL LastCallResult = 0;
		DWORD LastError = 0;

		LastCallResult = SuspendThread(Data->ThreadHandle);
		LastError = GetLastError();

		Data->Context = { 0 };
		Data->Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
		LastCallResult = GetThreadContext(Data->ThreadHandle, &Data->Context);
		LastError = GetLastError();

		LastCallResult = ResumeThread(Data->ThreadHandle);
		LastError = GetLastError();
		return 0;
	}
	
	static FFetchContextData FetchContext()
	{
		FFetchContextData Data;

		Data.ThreadHandle = GetCurrentThread();

		DWORD pid = GetCurrentThreadId();
		Data.ThreadHandle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, 0, pid);

		HANDLE OperationThread = CreateThread(0, 0, FetchContextThread, (LPVOID)&Data, 0, 0);
		WaitForSingleObject(OperationThread, INFINITE);

		CloseHandle(OperationThread);
		CloseHandle(Data.ThreadHandle);
		return Data;
	}
}

bool FWindowsPlatformHardwareBreakpoints::IsBreakpointSet(DebugRegisterIndex Index)
{
	using namespace HardwareBreakpointsUtils;
	if (Index < 0 || Index > 3)
	{
		return false;
	}
	FFetchContextData Data = FetchContext();

	auto DebugRegisters = &Data.Context.Dr0;
	return DebugRegisters[Index] != 0;
}

bool FWindowsPlatformHardwareBreakpoints::IsAnyRegistersContainOurBreakpointAddress(const void* BreakpointAddress, struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	DWORD64* IntegerRegisters = &ExceptionInfo->ContextRecord->Rax;

	for (int32 i = 0; i < 16; ++i)
	{
		if (BreakpointAddress == (void*)IntegerRegisters[i])
		{
			return true;
		}
	}
				
	return false;
}

bool FWindowsPlatformHardwareBreakpoints::AnyBreakpointSet()
{
	using namespace HardwareBreakpointsUtils;
	FFetchContextData Data = FetchContext();

	auto DebugRegisters = &Data.Context.Dr0;
	for (int i = 0; i < 4; ++i)
	{
		if (DebugRegisters[i] != 0)
			return true;
	}
	return false;
}

bool FWindowsPlatformHardwareBreakpoints::RemoveHardwareBreakpoint(DebugRegisterIndex Index)
{
	static_assert(MAX_HARDWARE_BREAKPOINTS == 4, "Windows hardware breakpoints should be 4, is some other value. Check that the plugin is being compiled without unity build");
	if (Index < 0 || Index > MAX_HARDWARE_BREAKPOINTS-1)
	{
		return false;
	}
	RemoveBreakpointAssociatedData(Index);

	FHardwareBreakpointData Data;

	Data.ThreadHandle = GetCurrentThread();
	Data.RegisterIndex = Index;

	DWORD pid = GetCurrentThreadId();
	Data.ThreadHandle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, 0, pid);

	Data.OperationToPerform = EDebugRegisterOperation::Remove;
	HANDLE OperationThread = CreateThread(0, 0, ApplyDebugRegisterChanges, (LPVOID)&Data, 0, 0);
	WaitForSingleObject(OperationThread, INFINITE);
	
	CloseHandle(OperationThread);
	CloseHandle(Data.ThreadHandle);

	return Data.RegistersChanged;
}

bool FWindowsPlatformHardwareBreakpoints::RemoveAllHardwareBreakpoints()
{
	RemoveAllBreakpointAssociatedData();

	FHardwareBreakpointData Data;
	Data.ThreadHandle = GetCurrentThread();

	DWORD pid = GetCurrentThreadId();
	Data.ThreadHandle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, 0, pid);

	Data.OperationToPerform = EDebugRegisterOperation::RemoveAll;
	HANDLE OperationThread = CreateThread(0, 0, ApplyDebugRegisterChanges, (LPVOID)&Data, 0, 0);
	WaitForSingleObject(OperationThread, INFINITE);

	CloseHandle(OperationThread);
	CloseHandle(Data.ThreadHandle);

	return Data.RegistersChanged;
}

extern COREUOBJECT_API FNativeFuncPtr GNatives[];

namespace HardwareBreakpointsUtils
{
	inline bool AddressIsWatchedNativeFunctionCall(PVOID Address, PCONTEXT ContextRecord, int& OutRegisterIndex)
	{
		auto DebugRegisters = &ContextRecord->Dr0;
		for (int i = 0; i < 4; ++i)
		{
			if (Address == (PVOID*)DebugRegisters[i])
			{
				OutRegisterIndex = i;
				return true;
			}
		}
		OutRegisterIndex = -1;
		return false;
	}

	inline void ClearBreakpointFromContextRecord(PCONTEXT ContextRecord, int Index)
	{
		auto DebugRegisters = &ContextRecord->Dr0;
		DebugRegisters[Index] = 0;
		ContextRecord->Dr7 &= ~(1 << (Index * 2));
		FPlatformHardwareBreakpoints::RemoveBreakpointAssociatedData(Index);
	}

	inline void ShiftBreakpointAddressToNextByte(PCONTEXT ContextRecord, int Index)
	{
		auto DebugRegisters = &ContextRecord->Dr0;
		++DebugRegisters[Index];
	}

	inline int CountActiveDebugRegisters(PCONTEXT ContextRecord, int& LastActiveRegister)
	{
		int Count = 0;
		auto* DebugRegisters = &ContextRecord->Dr0;
		for (int i = 0; i < 4; ++i)
		{
			if (DebugRegisters[i])
			{
				LastActiveRegister = i;
				++Count;
			}
		}
		return Count;
	}

	// Returns debug register index or INDEX_NONE if not found
	int32 FindRegisterForBPFunction(PCONTEXT ContextRecord)
	{
		const FFrame* ScriptStack = FFrame::GetThreadLocalTopStackFrame();

		auto* DebugRegisters = &ContextRecord->Dr0;

		for (int i = 0; i < 4; ++i)
		{
			if ((uint8*)DebugRegisters[i] == ScriptStack->Code)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	static bool WaitingForSingleStep = false;

	static void ProcessBreakpointClearing(struct _EXCEPTION_POINTERS *ExceptionInfo)
	{
		WindowsPlatformHardwareBreakpoints::FBreakpointClearData ClearData;
		WindowsPlatformHardwareBreakpoints::ClearBreakpoints(ClearData);

		for (int i = 0; i < 4; ++i)
		{
			if (ClearData.ClearBreakpoint[i] || ClearData.ClearAllBreakpoints)
			{
				ClearBreakpointFromContextRecord(ExceptionInfo->ContextRecord, i);
				FPlatformHardwareBreakpoints::RemoveBreakpointAssociatedData(i);
			}
		}
	}

	bool IsBlueprintInternalFunctionName(ANSICHAR* FunctionName, bool& IsProcessInternal)
	{
		static const FName InternalNames[] = {
			FName("UObject::ProcessInternal()"),
			FName("UFunction::Invoke()"),
			FName("UObject::CallFunction()"),
			FName("FFrame::Step()"),
			FName("UArrayProperty::CopyValuesInternal()"),
			FName("UObject::ProcessContextOpcode()"),
			FName("UObject::ProcessEvent()"),
			FName("AActor::ProcessEvent()"),
			FName("ProcessLocalScriptFunction()"),
			FName("ProcessScriptFunction<void (__cdecl*)(UObject *,FFrame &,void *)>()"),
			FName("ProcessLocalFunction()")
		};
		const FName SearchName = FName(FunctionName, FNAME_Find);
		if (SearchName == NAME_None)
		{
			IsProcessInternal = false;
			if (FCStringAnsi::Strstr(FunctionName, "::exec"))
			{
				return true;
			}
			return false;
		}
		const int N = sizeof(InternalNames) / sizeof(InternalNames[0]);
		for (int i = 0; i < N; ++i)
		{
			if (SearchName == InternalNames[i])
			{
				IsProcessInternal = i == 0;
				return true;
			}
		}
		IsProcessInternal = false;
		return false;
	}

	void CustomStackTraceToLog(CONTEXT* ContextRecord, void* ContextWrapper, DebugRegisterIndex BreakpointIndex)
	{
		// Temporary memory holding the stack trace.
		static const int MAX_DEPTH = 100;
		uint64 StackTrace[MAX_DEPTH];
		FMemory::Memzero(StackTrace);

		// Capture stack backtrace
		// Using optional ContextWrapper. Without it, the stack trace was incomplete for native function breakpoints
		uint32 Depth = FPlatformStackWalk::CaptureStackBackTrace(StackTrace, MAX_DEPTH, ContextWrapper);

		TArray<FExtendedProgramCounterSymbolInfo> StackTraceSymbolInfo;
		StackTraceSymbolInfo.AddDefaulted(Depth);

		for (uint32 i = 0; i < Depth; ++i)
		{
			FPlatformStackWalk::ProgramCounterToSymbolInfo(StackTrace[i], StackTraceSymbolInfo[i].SymbolInfo);
		}
#if DO_BLUEPRINT_GUARD
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25
		const TArray<const FFrame*>& ScriptStack = FBlueprintContextTracker::Get().GetScriptStack();
#else
		TArray<const FFrame*>& ScriptStack = FBlueprintExceptionTracker::Get().ScriptStack;
#endif
		int32 BlueprintFrameIdx = ScriptStack.Num() - 1;
		for (uint32 i = 0; i < Depth; ++i)
		{
			bool IsProcessInternal;
			StackTraceSymbolInfo[i].bIsBlueprintInternalToBeRemoved = IsBlueprintInternalFunctionName(StackTraceSymbolInfo[i].SymbolInfo.FunctionName, IsProcessInternal);
			if (IsProcessInternal)
			{
				StackTraceSymbolInfo[i].bIsBlueprintInternalToBeRemoved = false;
				StackTraceSymbolInfo[i].BlueprintFrame = ScriptStack[BlueprintFrameIdx--];
			}
		}
#endif
		StackTraceSymbolInfo.RemoveAll([](FExtendedProgramCounterSymbolInfo& Info) { return Info.bIsBlueprintInternalToBeRemoved; });

		// Walk the remaining stack and dump it to the allocated memory.
		const SIZE_T StackTraceReadableStringSize = 65535;
		ANSICHAR* StackTraceReadableString = (ANSICHAR*)FMemory::SystemMalloc(StackTraceReadableStringSize);
		{
			StackTraceReadableString[0] = 0;
			bool bPreviousWasBlueprint = false;
			for (auto& Info : StackTraceSymbolInfo)
			{
				if (Info.BlueprintFrame != nullptr)
				{
					if (!bPreviousWasBlueprint)
					{
						FCStringAnsi::Strcat(StackTraceReadableString, StackTraceReadableStringSize, "==== Blueprint End ===" LINE_TERMINATOR_ANSI);
					}
					FCStringAnsi::Strcat(StackTraceReadableString, StackTraceReadableStringSize, TCHAR_TO_ANSI(*Info.BlueprintFrame->GetStackDescription()));
				}
				else
				{
					if (bPreviousWasBlueprint)
					{
						FCStringAnsi::Strcat(StackTraceReadableString, StackTraceReadableStringSize, "==== Blueprint Start ===" LINE_TERMINATOR_ANSI);
					}
					FPlatformStackWalk::SymbolInfoToHumanReadableString(Info.SymbolInfo, StackTraceReadableString, StackTraceReadableStringSize);
				}
				bPreviousWasBlueprint = Info.BlueprintFrame != nullptr;
				FCStringAnsi::Strncat(StackTraceReadableString, LINE_TERMINATOR_ANSI, StackTraceReadableStringSize);
			}
		}
		if (IsInGameThread() && FSlateApplication::IsInitialized() && FSlateApplication::Get().CanAddModalWindow())
		{
			{
				FDelegateHandle CallstackHandle = CallStackViewer::OnRemoveBreakpoint.AddLambda([ContextRecord](DebugRegisterIndex Index) {
					ClearBreakpointFromContextRecord(ContextRecord, Index);
				});
				OpenModalCallstackWindow(BreakpointIndex, StackTraceSymbolInfo);
				CallStackViewer::OnRemoveBreakpoint.Remove(CallstackHandle);
			}
		}
		
		{
			UE_LOG(LogHardwareBreakpoints, Log, TEXT("_\n============= HARDWARE BREAKPOINT STACK ===========\nStack:\n%s"), ANSI_TO_TCHAR(StackTraceReadableString));
			GLog->Flush();
		}
		FMemory::SystemFree(StackTraceReadableString);
	}

	void DumpStackIfEnabled(CONTEXT* ContextRecord, void* ContextWrapper, DebugRegisterIndex BreakpointIndex)
	{
		if (!GetDefault<UHWBP_Settings>()->DontShowCallstackWindowIfDebuggerAttached || !FPlatformMisc::IsDebuggerPresent())
		{
			bool DoLog = true;
			if (!FPlatformHardwareBreakpoints::IsStackWalkingInitialized())
			{
				FText OptTitle = FText::FromString(TEXT("Hardware Breakpoint hit"));
				EAppReturnType::Type Return = OpenMsgDlgInt(EAppMsgType::YesNo, FText::FromString(TEXT("Need to load debug symbols to display callstack, but this will freeze the application for a while. Do you want symbols to be loaded?")), OptTitle);
				if (Return != EAppReturnType::Yes)
				{
					DoLog = false;
				}
			}
			if (DoLog)
			{
				FPlatformStackWalk::InitStackWalking();
#if !NO_LOGGING
				CustomStackTraceToLog(ContextRecord, ContextWrapper, BreakpointIndex);
#endif
			}
		}
	}

	static PVOID GExceptionHandlerHandle = nullptr;
	static CONTEXT StoredContext;

	static void StoreContext(struct _EXCEPTION_POINTERS *ExceptionInfo)
	{
		FMemory::Memcpy(StoredContext, *ExceptionInfo->ContextRecord);
	}
}
#define CALL_FIRST 1  
#define CALL_LAST 0


LONG WINAPI HardwareBreakpointsExceptionHandler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	using namespace HardwareBreakpointsUtils;
	if (ExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	//If we just single stepped after a native or blueprint function breakpoint restore breakpoint state
	if (WaitingForSingleStep)
	{
		WaitingForSingleStep = false;
		auto CurrentDebugRegisters = &ExceptionInfo->ContextRecord->Dr0;
		auto OldDebugRegisters = &StoredContext.Dr0;
		FMemory::Memcpy(CurrentDebugRegisters, OldDebugRegisters, sizeof(*CurrentDebugRegisters) * 6);
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	HANDLE DumpThreadHandle = GetCurrentThread();
	void* ContextWrapper = FWindowsPlatformStackWalk::MakeThreadContextWrapper(ExceptionInfo->ContextRecord, DumpThreadHandle);
	CONTEXT* ContextRecord = ExceptionInfo->ContextRecord;

	int32 OutRegisterIndex = FindRegisterForBPFunction(ContextRecord);
	if (const bool bBlueprintFunctionCall = OutRegisterIndex != INDEX_NONE)
	{
		DumpStackIfEnabled(ContextRecord, ContextWrapper, OutRegisterIndex);
		WindowsPlatformHardwareBreakpoints::CaughtBlueprintFunctionBreakpoint();
		ProcessBreakpointClearing(ExceptionInfo);
		
		OutRegisterIndex = FindRegisterForBPFunction(ContextRecord); // #TODO: Do I need to check a new context?
		const bool BreakpointIsStillActive = OutRegisterIndex != INDEX_NONE;
		if (BreakpointIsStillActive)
		{
			if (OutRegisterIndex != -1)
			{
				StoreContext(ExceptionInfo);
				//We shift the bytecode read breakpoint to the next byte, and mark that we're waiting
				//On next exception we won't break, but we'll reset the breakpoint to its original state
				ShiftBreakpointAddressToNextByte(ContextRecord, OutRegisterIndex);
				WaitingForSingleStep = true;
			}
		}
	}
	else if (AddressIsWatchedNativeFunctionCall(ExceptionInfo->ExceptionRecord->ExceptionAddress, ContextRecord, OutRegisterIndex))
	{
		DumpStackIfEnabled(ContextRecord, ContextWrapper, OutRegisterIndex);
		WindowsPlatformHardwareBreakpoints::CaughtNativeFunctionBreakpoint();
		ProcessBreakpointClearing(ExceptionInfo);

		bool BreakpointIsStillActive = AddressIsWatchedNativeFunctionCall(ExceptionInfo->ExceptionRecord->ExceptionAddress, ContextRecord, OutRegisterIndex);

		if (BreakpointIsStillActive)
		{
			StoreContext(ExceptionInfo);
			//Native function breakpoints need to be cleared so execution can continue
			ClearBreakpointFromContextRecord(ContextRecord, OutRegisterIndex);
			//We mark the single step trap flag so we can restore it after the instruction pointer has moved to the next instruction
			ContextRecord->EFlags |= 0x0100;
			WaitingForSingleStep = true;
		}
	}
	else if (FPlatformHardwareBreakpoints::CheckDataBreakpointConditions(OutRegisterIndex, ExceptionInfo))
	{
		DumpStackIfEnabled(ContextRecord, ContextWrapper, OutRegisterIndex);
		WindowsPlatformHardwareBreakpoints::CaughtDataBreakpoint();
		ProcessBreakpointClearing(ExceptionInfo);
	}

	FWindowsPlatformStackWalk::ReleaseThreadContextWrapper(ContextWrapper);
	return EXCEPTION_CONTINUE_EXECUTION;
}

void FWindowsPlatformHardwareBreakpoints::AddStructuredExceptionHandler()
{
	HardwareBreakpointsUtils::GExceptionHandlerHandle = AddVectoredExceptionHandler(CALL_FIRST, HardwareBreakpointsExceptionHandler);
}

void FWindowsPlatformHardwareBreakpoints::RemoveStructuredExceptionHandler()
{
	using namespace HardwareBreakpointsUtils;
	RemoveVectoredExceptionHandler(GExceptionHandlerHandle);
	GExceptionHandlerHandle = nullptr;
}

int32 FWindowsPlatformHardwareBreakpoints::GetSymbolDisplacementForProgramCounter(uint64 ProgramCounter)
{
	// Initialize stack walking as it loads up symbol information which we require.
	FPlatformStackWalk::InitStackWalking();

	uint32 LastError = 0;
	HANDLE ProcessHandle = GetCurrentProcess();

	// Initialize symbol.
	ANSICHAR SymbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + FProgramCounterSymbolInfo::MAX_NAME_LENGTH] = { 0 };
	IMAGEHLP_SYMBOL64* Symbol = (IMAGEHLP_SYMBOL64*)SymbolBuffer;
	Symbol->SizeOfStruct = sizeof(SymbolBuffer);
	Symbol->MaxNameLength = FProgramCounterSymbolInfo::MAX_NAME_LENGTH;

	DWORD64 dwSymbolDisplacement;

	// Get function name.
	if (SymGetSymFromAddr64(ProcessHandle, ProgramCounter, &dwSymbolDisplacement, Symbol))
	{
	}
	else
	{
		// No symbol found for this address.
		LastError = GetLastError();
	}
	return (int32)dwSymbolDisplacement;
}

uint64 FWindowsPlatformHardwareBreakpoints::GetAddressFromSymbolName(const CHAR* SymbolName)
{
	// Initialize stack walking as it loads up symbol information which we require.
	FPlatformStackWalk::InitStackWalking();

	uint32 LastError = 0;
	HANDLE ProcessHandle = GetCurrentProcess();

	// Initialize symbol.
	ANSICHAR SymbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + FProgramCounterSymbolInfo::MAX_NAME_LENGTH] = { 0 };
	IMAGEHLP_SYMBOL64* Symbol = (IMAGEHLP_SYMBOL64*)SymbolBuffer;
	Symbol->SizeOfStruct = sizeof(SymbolBuffer);
	Symbol->MaxNameLength = FProgramCounterSymbolInfo::MAX_NAME_LENGTH;

	// Get function name.
	if (SymGetSymFromName64(ProcessHandle, SymbolName, Symbol))
	{
	}
	else
	{
		// No symbol found for this address.
		LastError = GetLastError();
	}
	return Symbol->Address;
}

//Horrible hack because GStackWalkingInitialized is static to WindowsPlatformStackWalk.cpp (which is engine side)
bool FWindowsPlatformHardwareBreakpoints::IsStackWalkingInitialized()
{
	uint32 LastError = 0;
	HANDLE ProcessHandle = GetCurrentProcess();

	// Initialize symbol.
	ANSICHAR SymbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + FProgramCounterSymbolInfo::MAX_NAME_LENGTH] = { 0 };
	IMAGEHLP_SYMBOL64* Symbol = (IMAGEHLP_SYMBOL64*)SymbolBuffer;
	Symbol->SizeOfStruct = sizeof(SymbolBuffer);
	Symbol->MaxNameLength = FProgramCounterSymbolInfo::MAX_NAME_LENGTH;
	DWORD64 Displacement;

	if (SymGetSymFromAddr64(ProcessHandle, (DWORD64)&UObject::StaticClass, &Displacement, Symbol))
	{
		return true;
	}
	else
	{
		// If we couldn't get the symbol because of an invalid handle error, stack walking is not initialized
		LastError = GetLastError();
		return false;
	}
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
PRAGMA_ENABLE_OPTIMIZATION
#endif