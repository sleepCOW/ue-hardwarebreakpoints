// Copyright Daniel Amthauer. All Rights Reserved.

// Code adapted from USDImporter PropertyHelpers.h, by Epic Games
// Original code was private, therefore unusable by a plugin
// Also modified to make iterative instead of recursive, and to return the actual property address

#pragma once

#include "CoreMinimal.h"

#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MINOR_VERSION >= 25
#include "UObject/DefineUPropertyMacros.h"
#endif

class UProperty;
class UStruct;

namespace PropertyHelpers
{
	struct FPropertyAddress
	{
		UProperty* Property;
		void* Address;

		FPropertyAddress()
			: Property(nullptr)
			, Address(nullptr)
		{}
	};

	struct FPropertyAndIndex
	{
		FPropertyAndIndex() : Property(nullptr), ArrayIndex(INDEX_NONE) {}

		UProperty* Property;
		int32 ArrayIndex;
	};

	HARDWAREBREAKPOINTS_API FPropertyAndIndex FindPropertyAndArrayIndex(UStruct* InStruct, const FString& PropertyName);

	HARDWAREBREAKPOINTS_API FPropertyAddress FindPropertyAddress(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames);

	HARDWAREBREAKPOINTS_API FPropertyAddress FindPropertyAddress(void* BasePointer, UStruct* InStruct, const FString& InPropertyPath);

}