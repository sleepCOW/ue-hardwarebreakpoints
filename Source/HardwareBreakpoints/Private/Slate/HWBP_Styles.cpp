// Copyright Daniel Amthauer. All Rights Reserved.

#include "HWBP_Styles.h"

#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleRegistry.h"


TSharedPtr<FSlateStyleSet> FHWBP_Styles::MenuStyleInstance = NULL;

void FHWBP_Styles::Initialize() 
{
	if (!MenuStyleInstance.IsValid()) 
	{
		MenuStyleInstance = Create(); 
		FSlateStyleRegistry::RegisterSlateStyle(*MenuStyleInstance); 
	} 
}

void FHWBP_Styles::Shutdown() 
{ 
	FSlateStyleRegistry::UnRegisterSlateStyle(*MenuStyleInstance); 
	ensure(MenuStyleInstance.IsUnique()); 
	MenuStyleInstance.Reset(); 
}

FName FHWBP_Styles::GetStyleSetName()
{ 
	static FName StyleSetName(TEXT("HWBP_StyleSet")); 
	return StyleSetName; 
}

TSharedRef<FSlateStyleSet> FHWBP_Styles::Create() 
{ 
	TSharedRef<FSlateStyleSet> StyleRef = FSlateGameResources::New(FHWBP_Styles::GetStyleSetName(), "/HardwareBreakpoints/Styles", "/HardwareBreakpoints/Styles");
	return StyleRef; 
}

const ISlateStyle& FHWBP_Styles::Get()
{ 
	return *MenuStyleInstance;
}