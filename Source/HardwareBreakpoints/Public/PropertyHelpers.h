// Copyright Daniel Amthauer. All Rights Reserved.

// Code adapted from USDImporter PropertyHelpers.h, by Epic Games
// Original code was private, therefore unusable by a plugin
// Also modified to make iterative instead of recursive, and to return the actual property address

#pragma once

#include "CoreMinimal.h"

#include "Runtime/Launch/Resources/Version.h"


#if ENGINE_MINOR_VERSION >= 25
	#include "UObject/DefineUPropertyMacros.h"
#else
	#include "UObject/UnrealTypePrivate.h"
#endif

class UProperty;
class UStruct;

namespace PropertyHelpers
{
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25
	using PropertyType			= FProperty;
	using ArrayPropertyType		= FArrayProperty;
	using StructPropertyType	= FStructProperty;
	using UObjectPropertyType	= FObjectProperty;
	using FloatPropertyType		= FFloatProperty;
	using IntPropertyType		= FIntProperty;

	#define CAST_PROPERTY CastField
#else
	using PropertyType			= UProperty;
	using ArrayPropertyType		= UArrayProperty;
	using StructPropertyType	= UStructProperty;
	using UObjectPropertyType	= UObjectProperty;
	using FloatPropertyType		= UFloatProperty;
	using IntPropertyType		= UIntProperty;
	
	#define CAST_PROPERTY Cast
#endif
	
	
	struct FPropertyAddress
	{
		PropertyType* Property;
		void* Address;

		FPropertyAddress()
			: Property(nullptr)
			, Address(nullptr)
		{}
	};

	struct FPropertyAndIndex
	{
		FPropertyAndIndex() : Property(nullptr), ArrayIndex(INDEX_NONE) {}

		PropertyType* Property;
		int32 ArrayIndex;
	};

	HARDWAREBREAKPOINTS_API FPropertyAndIndex FindPropertyAndArrayIndex(UStruct* InStruct, const FString& PropertyName);

	HARDWAREBREAKPOINTS_API FPropertyAddress FindPropertyAddress(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames);

	HARDWAREBREAKPOINTS_API FPropertyAddress FindPropertyAddress(void* BasePointer, UStruct* InStruct, const FString& InPropertyPath);

}