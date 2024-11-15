// Copyright Daniel Amthauer. All Rights Reserved.

#include "CallStackViewer.h"

#include "Framework/Commands/GenericCommands.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformStackWalk.h"
#include "Engine/Blueprint.h"
#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "ISourceCodeAccessModule.h"
#include "ISourceCodeAccessor.h"
#include "Modules/ModuleManager.h"
#endif

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"

#include "Slate/HWBP_Styles.h"
#include "Slate/HWBP_StyleContainer.h"
#include "HWBP_Dialogs.h"


#define LOCTEXT_NAMESPACE "HWBP_CallStackViewer"

namespace CallStackViewer
{
	FOnRemoveBreakpointFromCallstackViewer OnRemoveBreakpoint;
}

enum class ECallstackLanguages : uint8
{
	Blueprints,
	NativeCPP,
};

struct FCallStackRow
{
	FCallStackRow(
		UObject* InContextObject,
		const FName& InScopeName, 
		const FName& InFunctionName, 
		int32 InScriptOffset, 
		ECallstackLanguages InLanguage, 
		const FText& InScopeDisplayName,
		const FText& InFunctionDisplayName,
		const FProgramCounterSymbolInfo* InSymbolInfo = nullptr
	)
		: ContextObject(InContextObject)
		, ScopeName(InScopeName)
		, FunctionName(InFunctionName)
		, ScriptOffset(InScriptOffset)
		, Language(InLanguage)
		, ScopeDisplayName(InScopeDisplayName)
		, FunctionDisplayName(InFunctionDisplayName)
	{
		if (InSymbolInfo)
		{
			SymbolInfo = *InSymbolInfo;
		}
	}

	UObject* ContextObject;

	FName ScopeName;
	FName FunctionName;
	int32 ScriptOffset;

	ECallstackLanguages Language;

	FText ScopeDisplayName;
	FText FunctionDisplayName;

	FProgramCounterSymbolInfo SymbolInfo;

	FText GetTextForEntry() const
	{
		switch(Language)
		{
		case ECallstackLanguages::Blueprints:
			return FText::Format(
				LOCTEXT("CallStackEntry", "{0}: {1}"), 
				ScopeDisplayName, 
				FunctionDisplayName
			);
		case ECallstackLanguages::NativeCPP:
			return ScopeDisplayName;
		}

		return FText();
	}
};

typedef STreeView<TSharedRef<FCallStackRow>> SCallStackTree;

class SCallStackViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SCallStackViewer ){}
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(DebugRegisterIndex, BreakpointIndex)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, TArray<TSharedRef<FCallStackRow>>* InCallStackSource);
	void CopySelectedRows() const;
	void JumpToEntry(TSharedRef< FCallStackRow > Entry);
	void JumpToSelectedEntry();

	/** SWidget interface */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

	TSharedPtr<SCallStackTree> CallStackTreeWidget;
	TArray<TSharedRef<FCallStackRow>> CallStackSource;
	TWeakPtr<FCallStackRow> LastFrameNavigatedTo;
	TWeakPtr<FCallStackRow> LastFrameClickedOn;

	TSharedPtr< FUICommandList > CommandList;

	TSharedPtr<SWindow> ParentWindow;
	DebugRegisterIndex BreakpointIndex;

	FReply HandleContinueExecutionButtonClicked()
	{
		ParentWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	FReply HandleRemoveBreakpointButtonClicked()
	{
		OpenMsgDlgInt(EAppMsgType::Ok, LOCTEXT("RemoveBreakpointText", "The breakpoint that triggered this window has been removed"), LOCTEXT("MessageTitle", "Message"));
		if(CallStackViewer::OnRemoveBreakpoint.IsBound())
		{
			CallStackViewer::OnRemoveBreakpoint.Broadcast(BreakpointIndex);
		}
		return FReply::Handled();
	}
};


class SCallStackViewerTableRow : public SMultiColumnTableRow< TSharedRef<FCallStackRow> >
{
public:
	SLATE_BEGIN_ARGS(SCallStackViewerTableRow) { }
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TWeakPtr<FCallStackRow> InEntry, TWeakPtr<SCallStackViewer> InOwner, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		CallStackEntry = InEntry;
		Owner = InOwner;
		SMultiColumnTableRow< TSharedRef<FCallStackRow> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& InColumnName )
	{
		TSharedPtr<FCallStackRow> CallStackEntryPinned = CallStackEntry.Pin();

		const FHWBP_Style& DialogStyle = FHWBP_Styles::Get().GetWidgetStyle<FHWBP_Style>("DialogStyle");

		const FSlateBrush* CurrentStackFrameBrush = &DialogStyle.CallstackViewerCurrentStackFrame;
		const FLinearColor& CurrentStackFrameColor = DialogStyle.CallstackViewerCurrentStackFrameColor;
		const FLinearColor& LastStackFrameNavigatedColor = DialogStyle.CallstackViewerLastStackFrameNavigatedToColor;

		if(InColumnName == TEXT("ProgramCounter") || !CallStackEntryPinned.IsValid())
		{
			TSharedPtr<SCallStackViewer> OwnerPinned = Owner.Pin();
			if(CallStackEntryPinned.IsValid() && OwnerPinned.IsValid())
			{
				TSharedPtr<SImage> Icon;
				if(OwnerPinned->CallStackSource.Num() > 0 && OwnerPinned->CallStackSource[0] == CallStackEntryPinned)
				{
					
					Icon =  SNew(SImage)
						.Image(CurrentStackFrameBrush)
						.ColorAndOpacity(CurrentStackFrameColor);
				}
				else
				{
					const auto NavigationTrackerVisibility = [](TWeakPtr<SCallStackViewer> InOwner, TWeakPtr<FCallStackRow> InCallStackEntry) -> EVisibility
					{
						TSharedPtr<SCallStackViewer> InOwnerPinned = InOwner.Pin();
						TSharedPtr<FCallStackRow> InCallStackEntryPinned = InCallStackEntry.Pin();
						if(InOwnerPinned.IsValid() && InCallStackEntryPinned.IsValid() && InOwnerPinned->LastFrameNavigatedTo == InCallStackEntryPinned)
						{
							return EVisibility::Visible;
						}

						return EVisibility::Hidden;
					};

					
					Icon = SNew(SImage)
						.Image(CurrentStackFrameBrush)
						.ColorAndOpacity(LastStackFrameNavigatedColor)
						.Visibility(
							TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(NavigationTrackerVisibility, Owner, CallStackEntry))
						);
				}

				if(Icon.IsValid())
				{
					return SNew( SHorizontalBox )
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						[
							Icon.ToSharedRef()
						];
				}
			}
			return SNew(SBox);
		}
		else if( InColumnName == TEXT("FunctionName"))
		{
			return 
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2)
				[
					SNew(STextBlock)
					.Text(
						CallStackEntryPinned->GetTextForEntry()
					)
				];
			}
		else if (InColumnName == TEXT("Language"))
		{
			FText Language;
			switch( CallStackEntryPinned->Language)
			{
				case ECallstackLanguages::Blueprints:
					Language = LOCTEXT("BlueprintsLanguageName", "Blueprints");
					break;
				case ECallstackLanguages::NativeCPP:
					Language = LOCTEXT("CPPLanguageName", "C++");
					break;
			}

			return SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(STextBlock)
				.Text(Language)
			];
		}
		else
		{
			ensure(false);
			return SNew(STextBlock)
				.Text(LOCTEXT("UnexpectedColumn", "Unexpected Column"));
		}
	}

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		// Our owner needs to know which row was right clicked in order to provide reliable navigation
		// from the context menu:
		TSharedPtr<SCallStackViewer> OwnerPinned = Owner.Pin();
		if(OwnerPinned.IsValid())
		{
			OwnerPinned->LastFrameClickedOn = CallStackEntry;
		}

		return SMultiColumnTableRow< TSharedRef<FCallStackRow> >::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

private:
	TWeakPtr<FCallStackRow> CallStackEntry;
	TWeakPtr<SCallStackViewer> Owner;
};

void SCallStackViewer::Construct(const FArguments& InArgs, TArray<TSharedRef<FCallStackRow>>* InCallStackSource)
{
	ParentWindow = InArgs._ParentWindow.Get();
	ParentWindow->SetWidgetToFocusOnActivate(SharedThis(this));
	BreakpointIndex = InArgs._BreakpointIndex.Get();

	CommandList = MakeShareable( new FUICommandList );
	CommandList->MapAction( 
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP( this, &SCallStackViewer::CopySelectedRows ),
		// we need to override the default 'can execute' because we want to be available during debugging:
		FCanExecuteAction::CreateStatic( [](){ return true; } )
	);

	CallStackSource = MoveTemp(*InCallStackSource);

	// The table view 'owns' the row, but it's too inflexible to do anything useful, so we pass in a pointer to SCallStackViewer,
	// this is only necessary because we have multiple columns and SMultiColumnTableRow requires deriving:
	const auto RowGenerator = [](TSharedRef< FCallStackRow > Entry, const TSharedRef<STableViewBase>& TableOwner, TWeakPtr<SCallStackViewer> ControlOwner) -> TSharedRef< ITableRow >
	{
		return SNew(SCallStackViewerTableRow, Entry, ControlOwner, TableOwner);
	};

	const auto ContextMenuOpened = [](TWeakPtr<FUICommandList> InCommandList, TWeakPtr<SCallStackViewer> ControlOwnerWeak) -> TSharedPtr<SWidget>
	{
		const bool CloseAfterSelection = true;
		FMenuBuilder MenuBuilder( CloseAfterSelection, InCommandList.Pin() );
		TSharedPtr<SCallStackViewer> ControlOwner = ControlOwnerWeak.Pin();

#if WITH_EDITOR
		FText Text = LOCTEXT("GoToDefinition", "Go to Function Definition");
		auto CanExecute = []() { return true; };
#else
		FText Text = LOCTEXT("GoToDefinitionRuntime", "Go to Function Definition (Editor Only)");
		auto CanExecute = []() { return false; };
#endif
		if(ControlOwner.IsValid())
		{
			MenuBuilder.AddMenuEntry(
				Text,
				LOCTEXT("GoToDefinitionTooltip", "Opens the Blueprint that declares the function"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(ControlOwner.ToSharedRef(), &SCallStackViewer::JumpToSelectedEntry),
					FCanExecuteAction::CreateStatic( CanExecute )
				)
		);
		}
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		return MenuBuilder.MakeWidget();
	};

	// there is no nesting in this list view:
	const auto ChildrenAccessor = [](TSharedRef<FCallStackRow> InTreeItem, TArray< TSharedRef< FCallStackRow > >& OutChildren)
	{
	};

	const auto EmptyWarningVisibility = [](TWeakPtr<SCallStackViewer> ControlOwnerWeak) -> EVisibility
	{
		TSharedPtr<SCallStackViewer> ControlOwner = ControlOwnerWeak.Pin();
		if(ControlOwner.IsValid() &&
			ControlOwner->CallStackSource.Num() > 0)
		{
			return EVisibility::Hidden;
		}
		return EVisibility::Visible;
	};

	const auto CallStackViewIsEnabled = [](TWeakPtr<SCallStackViewer> ControlOwnerWeak) -> bool
	{
		TSharedPtr<SCallStackViewer> ControlOwner = ControlOwnerWeak.Pin();
		if(ControlOwner.IsValid() &&
			ControlOwner->CallStackSource.Num() > 0)
		{
			return true;
		}
		return false;
	};

	// Cast due to TSharedFromThis inheritance issues:
	TSharedRef<SCallStackViewer> SelfTyped = StaticCastSharedRef<SCallStackViewer>(AsShared());
	TWeakPtr<SCallStackViewer> SelfWeak = SelfTyped;
	TWeakPtr<FUICommandList> CommandListWeak = CommandList;

	const FHWBP_Style& DialogStyle = FHWBP_Styles::Get().GetWidgetStyle<FHWBP_Style>("DialogStyle");

	const FSlateBrush* GroupBorderBrush = &DialogStyle.GroupBorder;
	auto& ContentPadding = DialogStyle.ContentPadding;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(4)
		.BorderImage( GroupBorderBrush )
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(CallStackTreeWidget, SCallStackTree)
						.ItemHeight(25.0f)
						.TreeItemsSource(&CallStackSource)
						.OnGenerateRow(SCallStackTree::FOnGenerateRow::CreateStatic(RowGenerator, SelfWeak))
						.OnGetChildren(SCallStackTree::FOnGetChildren::CreateStatic(ChildrenAccessor))
						.OnMouseButtonDoubleClick(SCallStackTree::FOnMouseButtonClick::CreateSP(SelfTyped, &SCallStackViewer::JumpToEntry))
						.OnContextMenuOpening(FOnContextMenuOpening::CreateStatic(ContextMenuOpened, CommandListWeak, SelfWeak))
						.IsEnabled(
							TAttribute<bool>::Create(
								TAttribute<bool>::FGetter::CreateStatic(CallStackViewIsEnabled, SelfWeak)
							)
						)
						.HeaderRow
						(
							SNew(SHeaderRow)
							+SHeaderRow::Column(TEXT("ProgramCounter"))
							.DefaultLabel(LOCTEXT("ProgramCounterLabel", ""))
							.FixedWidth(16.f)
							+SHeaderRow::Column(TEXT("FunctionName"))
							.FillWidth(.8f)
							.DefaultLabel(LOCTEXT("FunctionName", "Function Name"))
							+SHeaderRow::Column(TEXT("Language"))
							.DefaultLabel(LOCTEXT("Language", "Language"))
							.FillWidth(.15f)
						)
					]
				]
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.AutoHeight()
				.Padding(10.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SSpacer)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("RemoveBreakpoint", "Remove Breakpoint"))
						.ToolTipText(LOCTEXT("RemoveBreakpointTooltip", "Removes the breakpoint that triggered this window"))
						.OnClicked(this, &SCallStackViewer::HandleRemoveBreakpointButtonClicked)
						.ContentPadding(ContentPadding)
						.HAlign(HAlign_Right)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ContinueExecution", "Continue Execution"))
						.OnClicked(this, &SCallStackViewer::HandleContinueExecutionButtonClicked)
						.ContentPadding(ContentPadding)
						.HAlign(HAlign_Right)
					]
				]
			]
			+SOverlay::Slot()
			.Padding(32.f)
			[
				SNew(STextBlock)
				.Text(
					LOCTEXT("NoCallStack", "No call stack to display - set a breakpoint and Play in Editor")
				)
				.Justification(ETextJustify::Center)
				.Visibility(
					TAttribute<EVisibility>::Create(
						TAttribute<EVisibility>::FGetter::CreateStatic(EmptyWarningVisibility, SelfWeak)
					)
				)
			]
		]
	];
}

void SCallStackViewer::CopySelectedRows() const
{
	FString StringToCopy;

	// We want to copy in the order displayed, not the order selected, so iterate the list and build up the string:
	for (const TSharedRef<FCallStackRow>& Item : CallStackSource)
	{
		if (CallStackTreeWidget->IsItemSelected(Item))
		{
			StringToCopy.Append(Item->GetTextForEntry().ToString());
			StringToCopy.Append(TEXT("\r\n"));
		}
	}

	if( !StringToCopy.IsEmpty() )
	{
		FPlatformApplicationMisc::ClipboardCopy(*StringToCopy);
	}
}

void SCallStackViewer::JumpToEntry(TSharedRef< FCallStackRow > Entry)
{
	LastFrameNavigatedTo = Entry;

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (Entry->Language == ECallstackLanguages::Blueprints)
		{
			// Try to find a UClass* source:
			bool bExactClass = false;
			bool bAnyPackage = true;
			UBlueprintGeneratedClass* BPGC = FindObjectFast<UBlueprintGeneratedClass>(nullptr, Entry->ScopeName, bExactClass, bAnyPackage, RF_NoFlags);

			bool bSuccess = false;

			if (BPGC)
			{
				UFunction* Function = BPGC->FindFunctionByName(Entry->FunctionName);
				if (Function)
				{
					UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(Entry->ContextObject, Function, Entry->ScriptOffset, true);
					if (Node)
					{
						FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);
						bSuccess = true;
					}
					else
					{
						FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Function);
						bSuccess = true;
					}
				}
				else
				{
					FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(BPGC);
					bSuccess = true;
				}
			}
			OpenMsgDlgInt(EAppMsgType::Ok, bSuccess ? LOCTEXT("BP_GotoSuccess", "Execution is currently paused, but the editor will focus on the blueprint node for this callstack entry once it resumes") : LOCTEXT("BP_GotoFail", "Couldn't focus on object"), LOCTEXT("MessageTitle", "Message"));
		}
		else
		{
			ISourceCodeAccessModule* SourceCodeAccessModule = FModuleManager::LoadModulePtr<ISourceCodeAccessModule>("SourceCodeAccess");
			if (SourceCodeAccessModule)
			{
				ISourceCodeAccessor& SourceCodeAccessor = SourceCodeAccessModule->GetAccessor();
				SourceCodeAccessor.OpenFileAtLine(ANSI_TO_TCHAR(Entry->SymbolInfo.Filename), Entry->SymbolInfo.LineNumber, 0);
			}
		}
	}
#endif
}

void SCallStackViewer::JumpToSelectedEntry()
{
	TSharedPtr<FCallStackRow> LastFrameClickedOnPinned = LastFrameClickedOn.Pin();
	if(LastFrameClickedOnPinned.IsValid())
	{
		JumpToEntry(LastFrameClickedOnPinned.ToSharedRef());
	}
	else
	{
		for(const TSharedRef<FCallStackRow>& Item : CallStackSource)
		{
			if(CallStackTreeWidget->IsItemSelected(Item))
			{
				JumpToEntry(Item);
			}
		}
	}
}

FReply SCallStackViewer::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

//Copied from FBlueprintEditorUtils so we can use it at runtime
static FString GetClassNameWithoutSuffix(const UClass* Class)
{
	if (Class != nullptr)
	{
		FString Result = Class->GetName();
		//This should never be a native class, and this check fails at runtime, so we remove it
		//if (Class->ClassGeneratedBy != nullptr)
		{
			Result.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
		}

		return Result;
	}
	else
	{
		return LOCTEXT("ClassIsNull", "None").ToString();
	}
}

static const FString FN_ExecuteUbergraphBase = TEXT("ExecuteUbergraph");

static FName GetUbergraphFunctionName(const UClass* Class)
{
	const FString UbergraphCallString = FN_ExecuteUbergraphBase + TEXT("_") + GetClassNameWithoutSuffix(Class);
	return FName(*UbergraphCallString);
}

TArray<TSharedRef<FCallStackRow>> UpdateDisplayedCallstack(const TArray<FExtendedProgramCounterSymbolInfo>& Stack)
{
	TArray<TSharedRef<FCallStackRow>> CallstackSource;
	if (Stack.Num() > 0)
	{
		for (auto& Frame : Stack)
		{
			const FFrame* StackNode = Frame.BlueprintFrame;
			
			if (StackNode)
			{
				bool bExactClass = false;
				bool bAnyPackage = true;
				UClass* SourceClass = Cast<UClass>(StackNode->Node->GetOuter());
				// We're using GetClassNameWithoutSuffix so that we can display the BP name, which will
				// be the most useful when debugging:
				FText ScopeDisplayName = SourceClass ? FText::FromString(GetClassNameWithoutSuffix(SourceClass)) : FText::FromName(StackNode->Node->GetOuter()->GetFName());
				FText FunctionDisplayName = FText::FromName(StackNode->Node->GetFName());
				if (SourceClass)
				{
					if (StackNode->Node->GetFName() == GetUbergraphFunctionName(SourceClass))
					{
						FunctionDisplayName = LOCTEXT("EventGraphCallStackName", "Event Graph");
					}
					else
					{
#if WITH_EDITOR
						UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(StackNode->Object, StackNode->Node, 0, true);
						if (Node)
						{
							FunctionDisplayName = Node->GetNodeTitle(ENodeTitleType::ListView);
						}
#else
						//TODO: provide alternate function display name (just the function name?)
						FunctionDisplayName = FText::FromString(StackNode->Node->GetName());
#endif
					}
				}

				CallstackSource.Add(
					MakeShared<FCallStackRow>(
						StackNode->Object,
						StackNode->Node->GetOuter()->GetFName(),
						StackNode->Node->GetFName(),
						StackNode->Code - StackNode->Node->Script.GetData() - 1,
						ECallstackLanguages::Blueprints,
						ScopeDisplayName,
						FunctionDisplayName
						)
				);
			}
			else
			{
				const int StackTraceReadableStringSize = 512;
				ANSICHAR StackTraceReadableString[StackTraceReadableStringSize] = { 0 };
				FPlatformStackWalk::SymbolInfoToHumanReadableString(Frame.SymbolInfo, StackTraceReadableString, StackTraceReadableStringSize);
				CallstackSource.Add(
					MakeShared<FCallStackRow>(
						nullptr,
						FName(),
						FName(),
						0,
						ECallstackLanguages::NativeCPP,
						FText::FromString(ANSI_TO_TCHAR(StackTraceReadableString)),
						FText(),
						&Frame.SymbolInfo
						)
				);
			}
		}
	}
	return CallstackSource;
}

static void CreateModalCallstackWindow(TSharedPtr<SWindow>& OutWindow, const FText& InTitle, DebugRegisterIndex BreakpointIndex, const TArray<FExtendedProgramCounterSymbolInfo>& CallStackData) {
	OutWindow = SNew(SWindow)
		.Title(InTitle)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(1000, 450))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		//.SupportsMinimize(false).SupportsMaximize(false)
		;
	auto CallStackSource = UpdateDisplayedCallstack(CallStackData);
	TSharedPtr<SCallStackViewer> CallstackViewer = SNew(SCallStackViewer, &CallStackSource)
		.ParentWindow(OutWindow)
		.BreakpointIndex(BreakpointIndex);

	OutWindow->SetContent(CallstackViewer.ToSharedRef());
}

void OpenModalCallstackWindow(DebugRegisterIndex BreakpointIndex, const TArray<FExtendedProgramCounterSymbolInfo>& CallStackData)
{
	TSharedPtr<SWindow> MsgWindow = NULL;

	const FText& InTitle = LOCTEXT("CallstackWindowTitle", "Hardware Breakpoints Callstack");

	CreateModalCallstackWindow(MsgWindow, InTitle, BreakpointIndex, CallStackData);

	// If there is already a modal window active, parent this new modal window to the existing window so that it doesn't fall behind
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveModalWindow();

	FSlateApplication::Get().AddModalWindow(MsgWindow.ToSharedRef(), ParentWindow);
}

#undef LOCTEXT_NAMESPACE
