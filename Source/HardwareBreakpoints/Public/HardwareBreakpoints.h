// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "HardwareBreakpointsLog.h"

class FHardwareBreakpointsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
