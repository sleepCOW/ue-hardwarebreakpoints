// Copyright Daniel Amthauer. All Rights Reserved.

#include "PropertyHelpers.h"
#include "UObject/UnrealType.h"
#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/MetaData.h"
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 20
#include "UObject/UnrealTypePrivate.h"
#endif

namespace PropertyHelpers
{
	UMetaData* GetPackageMetadata(UUserDefinedStruct* InStruct)
	{
		UPackage* Package = InStruct->GetOutermost();
		check(Package);

		UMetaData* MetaData = Package->GetMetaData();
		check(MetaData);
		return MetaData;
	}

	PRAGMA_DISABLE_OPTIMIZATION
	//Names for UserDefinedStruct properties are of the form DisplayName_<digit>_<32 digit number>
	//So we assume that finding the second underscore from the end gives us the display name length
	//We could just strip 35 characters from the right but if the name format was changed by Epic we wouldn't find out
	bool ParsedDisplayNameMatches(PropertyType* UserDefinedStructProp, const ANSICHAR* FieldName, int FieldNameLen)
	{
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 21
		ANSICHAR FullName [NAME_SIZE];
		UserDefinedStructProp->GetFName().GetPlainANSIString(FullName);
#else
		const ANSICHAR* FullName = UserDefinedStructProp->GetFName().GetPlainANSIString();
#endif
		int FullNameLen = FCStringAnsi::Strlen(FullName);
		if (FPlatformString::Strncmp(FullName, FieldName, FieldNameLen) != 0)
		{
			//If the property name doesn't at least start with the name we're searching for, we're done
			return false;
		}
		//Actually extract the display name from the full name to verify they are actually equal, and not just matching a subset
		int UnderscoreCount = 0;
		int i = FullNameLen - 1;
		for (; i >= 0; --i)
		{
			if (FullName[i] == TEXT('_'))
			{
				++UnderscoreCount;
				if (UnderscoreCount == 2)
				{
					break;
				}
			}
		}
		//If this is not true, the name format for UserDefinedStruct properties has changed
		check(UnderscoreCount >= 2);

		//If the lengths match, they are equal, because we already compared this segment at the start
		//Otherwise this property's name contains the name that we're looking for, but it's not equal
		return i == FieldNameLen;
	}

	PropertyType* FindBPStructField(UUserDefinedStruct* Owner, const FString& FieldName)
	{
		if (FieldName.Len() == 0)
		{
			return nullptr;
		}

		static const FName NAME_DisplayName(TEXT("DisplayName"));

		//Accessing property display names (which are the correct names for properties on UserDefinedStructs) is only possible in the editor (because metadata is not available in standalone)
		//so we can't depend on this method. Therefore, we parse the display name from the full property name and compare to that.
#if 0 && WITH_EDITORONLY_DATA
		FString NativeDisplayName;
		UMetaData* StructMetadata = GetPackageMetadata(Owner);

		// Search by comparing display names
		for (TFieldIterator<UProperty>It(Owner); It; ++It)
		{
			if (StructMetadata->HasValue(*It, NAME_DisplayName))
			{
				NativeDisplayName = StructMetadata->GetValue(*It, NAME_DisplayName);
				if (NativeDisplayName == FieldName)
				{
					return *It;
				}
			}
		}
#else
		ANSICHAR* AnsiFieldName = TCHAR_TO_ANSI(*FieldName);
		// Search by comparing display names
		for (TFieldIterator<PropertyType>It(Owner); It; ++It)
		{
			if (ParsedDisplayNameMatches(*It, AnsiFieldName, FieldName.Len()))
			{
				return *It;
			}
		}
#endif

		// If we didn't find it, return no field
		return nullptr;
	}
	PRAGMA_ENABLE_OPTIMIZATION

	FPropertyAndIndex FindPropertyAndArrayIndex(UStruct* InStruct, const FString& PropertyName)
	{
		FPropertyAndIndex PropertyAndIndex;

		// Calculate the array index if possible
		int32 ArrayIndex = -1;
		if (PropertyName.Len() > 0 && PropertyName.GetCharArray()[PropertyName.Len() - 1] == ']')
		{
			int32 OpenIndex = 0;
			if (PropertyName.FindLastChar('[', OpenIndex))
			{
				FString TruncatedPropertyName(OpenIndex, *PropertyName);
				if (auto UserDefinedStruct = Cast<UUserDefinedStruct>(InStruct))
				{
					PropertyAndIndex.Property = FindBPStructField(UserDefinedStruct, TruncatedPropertyName);
				}
				else
				{
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25
					PropertyAndIndex.Property = FindFProperty<FProperty>(InStruct, *TruncatedPropertyName);
#else
					PropertyAndIndex.Property = FindField<UProperty>(InStruct, *TruncatedPropertyName);
#endif
				}
				
				if (PropertyAndIndex.Property)
				{
					const int32 NumberLength = PropertyName.Len() - OpenIndex - 2;
					if (NumberLength > 0 && NumberLength <= 10)
					{
						TCHAR NumberBuffer[11];
						FMemory::Memcpy(NumberBuffer, &PropertyName[OpenIndex + 1], sizeof(TCHAR) * NumberLength);
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 20
						LexFromString(PropertyAndIndex.ArrayIndex, NumberBuffer);
#else
						Lex::FromString(PropertyAndIndex.ArrayIndex, NumberBuffer);
#endif
					}
				}

				return PropertyAndIndex;
			}
		}

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25
		PropertyAndIndex.Property = FindFProperty<FProperty>(InStruct, *PropertyName);
#else
		PropertyAndIndex.Property = FindField<UProperty>(InStruct, *PropertyName);
#endif
		return PropertyAndIndex;
	}

	FPropertyAddress FindPropertyAddress(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames)
	{
		FPropertyAddress NewAddress;

		for (int32 Index = 0; Index < InPropertyNames.Num(); ++Index)
		{
			FPropertyAndIndex PropertyAndIndex = FindPropertyAndArrayIndex(InStruct, InPropertyNames[Index]);

			if (PropertyAndIndex.ArrayIndex != INDEX_NONE)
			{
				const ArrayPropertyType* ArrayProp = CAST_PROPERTY<ArrayPropertyType>(PropertyAndIndex.Property);

				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(BasePointer));

				if (ArrayHelper.IsValidIndex(PropertyAndIndex.ArrayIndex))
				{
					StructPropertyType* InnerStructProp = Cast<StructPropertyType>(ArrayProp->Inner);
					if (InnerStructProp && InPropertyNames.IsValidIndex(Index + 1))
					{
						BasePointer = ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex);
						InStruct = InnerStructProp->Struct;
						continue;
					}
					else
					{
						NewAddress.Property = ArrayProp->Inner;
						NewAddress.Address = ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex);
						break;
					}
				}
			}
			else if (StructPropertyType* StructProp = CAST_PROPERTY<StructPropertyType>(PropertyAndIndex.Property))
			{
				NewAddress.Property = StructProp;
				NewAddress.Address = BasePointer;

				if (InPropertyNames.IsValidIndex(Index + 1))
				{
					void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(BasePointer);
					BasePointer = StructContainer;
					InStruct = StructProp->Struct;
					continue;
				}
				else
				{
					check(StructProp->GetName() == InPropertyNames[Index]);
					break;
				}
			}
			else if (UObjectPropertyType* ObjectProp = CAST_PROPERTY<UObjectPropertyType>(PropertyAndIndex.Property))
			{
				NewAddress.Property = ObjectProp;
				NewAddress.Address = BasePointer;

				if (InPropertyNames.IsValidIndex(Index + 1))
				{
					void* ObjectContainer = ObjectProp->ContainerPtrToValuePtr<void>(BasePointer);
					UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectContainer);
					if (Object)
					{
						BasePointer = Object;
						InStruct = Object->GetClass();
						continue;
					}
					break;
				}
				else
				{
					check(ObjectProp->GetName() == InPropertyNames[Index]);
					break;
				}

			}
			else if (PropertyAndIndex.Property)
			{
				NewAddress.Property = PropertyAndIndex.Property;
				NewAddress.Address = PropertyAndIndex.Property->ContainerPtrToValuePtr<void>(BasePointer);
				break;
			}
		}

		return NewAddress;
	}

	FPropertyAddress FindPropertyAddress(void* BasePointer, UStruct* InStruct, const FString& InPropertyPath)
	{
		TArray<FString> PropertyNames;

		InPropertyPath.ParseIntoArray(PropertyNames, TEXT("."), true);

		if (PropertyNames.Num() > 0)
		{
			return FindPropertyAddress(BasePointer, InStruct, PropertyNames);
		}
		else
		{
			return FPropertyAddress();
		}
	}
}
