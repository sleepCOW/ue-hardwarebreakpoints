// Copyright Daniel Amthauer. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"

#include "HWBP_StyleContainer.generated.h"

USTRUCT()
struct FHWBP_Style : public FSlateWidgetStyle
{
	GENERATED_BODY()

public:
	virtual void GetResources(TArray<const FSlateBrush *> & OutBrushes) const override;

	virtual const FName GetTypeName() const override;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateFontInfo LargeFont;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush GroupBorder;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin SlotPadding;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin ContentPadding;

	UPROPERTY(EditAnywhere, Category = Appearance)
	float MinDesiredSlotWidth; 

	UPROPERTY(EditAnywhere, Category = Appearance)
	float MinDesiredSlotHeight;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush CallstackViewerCurrentStackFrame;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor CallstackViewerCurrentStackFrameColor;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor CallstackViewerLastStackFrameNavigatedToColor;

	static const FName TypeName;

	static const FHWBP_Style& GetDefault();
};

/**
 * 
 */
UCLASS()
class UHWBP_StyleContainer : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:
	
	// This is our actual Style object. 
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (ShowOnlyInnerProperties))
	FHWBP_Style HWBP_Style;

	// Retrievs the style that this container manages. 
	virtual const struct FSlateWidgetStyle* const GetStyle() const override { return static_cast<const struct FSlateWidgetStyle*>(&HWBP_Style); }
	
};
