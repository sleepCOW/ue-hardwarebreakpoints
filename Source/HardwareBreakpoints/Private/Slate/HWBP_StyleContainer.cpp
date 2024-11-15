// Copyright Daniel Amthauer. All Rights Reserved.

#include "HWBP_StyleContainer.h"


void FHWBP_Style::GetResources(TArray<const FSlateBrush *> & OutBrushes) const
{
	OutBrushes.Add(&GroupBorder);
}

const FName FHWBP_Style::GetTypeName() const
{
	return TypeName;
}

const FName FHWBP_Style::TypeName("FHWBP_Style");

const FHWBP_Style& FHWBP_Style::GetDefault()
{
	static FHWBP_Style Default;
	return Default;
}
