// Copyright Daniel Amthauer. All Rights Reserved.

#include "HardwareBreakpoints.h"

#include "Modules/ModuleManager.h"

#include "HAL/PlatformHardwareBreakpoints.h"
#include "HardwareBreakpointsLog.h"
#include "Slate/HWBP_Styles.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Settings/HWBP_Settings.h"
#include "Misc/HWBP_Build.h"
#endif

#define LOCTEXT_NAMESPACE "FHardwareBreakpointsModule"

DEFINE_LOG_CATEGORY(LogHardwareBreakpoints);

void FHardwareBreakpointsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FPlatformHardwareBreakpoints::AddStructuredExceptionHandler();
	
	FHWBP_Styles::Initialize();

#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Hardware Breakpoints", "Settings",
			LOCTEXT("SettingsName", "Settings"),
			LOCTEXT("SettingsNameDesc", "Hardware Breakpoints Settings"),
			GetMutableDefault<UHWBP_Settings>()
		);
	}
#endif
}

void FHardwareBreakpointsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FPlatformHardwareBreakpoints::RemoveStructuredExceptionHandler();

	FHWBP_Styles::Shutdown();

#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Hardware Breakpoints", "Settings");
	}
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHardwareBreakpointsModule, HardwareBreakpoints)