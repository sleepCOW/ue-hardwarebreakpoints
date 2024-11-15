// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#include "GenericPlatform/ExtendedProgramCounterSymbolInfo.h"
#include "GenericPlatform/GenericPlatformHardwareBreakpoints.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRemoveBreakpointFromCallstackViewer, DebugRegisterIndex);

namespace CallStackViewer
{
	extern FOnRemoveBreakpointFromCallstackViewer OnRemoveBreakpoint;
}

void OpenModalCallstackWindow(DebugRegisterIndex Index, const TArray<FExtendedProgramCounterSymbolInfo>& CallstackData);