// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformStackWalk.h"

struct FFrame;

struct FExtendedProgramCounterSymbolInfo
{
	FProgramCounterSymbolInfo SymbolInfo;
	const FFrame* BlueprintFrame;

	//internal, used for callstack construction
	uint32 bIsBlueprintInternalToBeRemoved : 1;

	FExtendedProgramCounterSymbolInfo()
		: BlueprintFrame(nullptr)
		, bIsBlueprintInternalToBeRemoved(false)
	{}
};