// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HWBP_Settings.generated.h"


UCLASS(config = HardwareBreakpoints, notplaceable)
class UHWBP_Settings : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(config, EditAnywhere, Category = HardwareBreakpoints, meta = (DisplayName = "Don't show callstack window if debugger is attached"))
	bool DontShowCallstackWindowIfDebuggerAttached;

	UPROPERTY(config, EditAnywhere, Category = HardwareBreakpoints, meta = (DisplayName = "Don't break even if debugger is attached"))
	bool DontBreakEvenIfDebuggerAttached;
};
