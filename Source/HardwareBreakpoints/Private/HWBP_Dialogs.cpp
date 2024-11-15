#include "HWBP_Dialogs.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Runtime/Launch/Resources/Version.h"
#include "SlateOptMacros.h"
#include "HAL/PlatformApplicationMisc.h"

#include "HardwareBreakpointsLog.h"
#include "Slate/HWBP_Styles.h"
#include "Slate/HWBP_StyleContainer.h"

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 20
	#include "Widgets/Layout/SUniformGridPanel.h"
	#include "Widgets/Layout/SScrollBox.h"
#else
	#include "Layout/SUniformGridPanel.h"
	#include "Layout/SScrollBox.h"
#endif

DECLARE_DELEGATE_TwoParams(FOnMsgDlgResult, const TSharedRef<SWindow>&, EAppReturnType::Type);

#define LOCTEXT_NAMESPACE "Dialogs"

///////////////////////////////////////////////////////////////////////////////
//
// Local classes.
//
///////////////////////////////////////////////////////////////////////////////

class SChoiceDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SChoiceDialog )	{}
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(FText, Message)	
		SLATE_ATTRIBUTE(float, WrapMessageAt)
		SLATE_ATTRIBUTE(EAppMsgType::Type, MessageType)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		ParentWindow = InArgs._ParentWindow.Get();
		ParentWindow->SetWidgetToFocusOnActivate(SharedThis(this));
		Response = EAppReturnType::Cancel;

		const FHWBP_Style& DialogStyle = FHWBP_Styles::Get().GetWidgetStyle<FHWBP_Style>("DialogStyle");

		FSlateFontInfo MessageFont( DialogStyle.LargeFont);
		MyMessage = InArgs._Message;

		TSharedPtr<SUniformGridPanel> ButtonBox;

		auto GroupBorder = &DialogStyle.GroupBorder;
		auto& SlotPadding = DialogStyle.SlotPadding;
		auto& DesiredWidth = DialogStyle.MinDesiredSlotWidth;
		auto& DesiredHeight = DialogStyle.MinDesiredSlotHeight;
		auto& ContentPadding = DialogStyle.ContentPadding;

		this->ChildSlot
			[	
				SNew(SBorder)
					.BorderImage(GroupBorder)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							.FillHeight(1.0f)
							.MaxHeight(550)
							.Padding(12.0f)
							[
								SNew(SScrollBox)

								+ SScrollBox::Slot()
									[
										SNew(STextBlock)
											.Text(MyMessage)
											.Font(MessageFont)
											.WrapTextAt(InArgs._WrapMessageAt)
									]
							]

						+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Bottom)
									.Padding(12.0f)
									[
										SNew(SHyperlink)
											.OnNavigate(this, &SChoiceDialog::HandleCopyMessageHyperlinkNavigate)
											.Text( NSLOCTEXT("SChoiceDialog", "CopyMessageHyperlink", "Copy Message") )
											.ToolTipText( NSLOCTEXT("SChoiceDialog", "CopyMessageTooltip", "Copy the text in this message to the clipboard (CTRL+C)") )
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Bottom)
									.Padding(2.f)
									[
										SAssignNew( ButtonBox, SUniformGridPanel )
											.SlotPadding(SlotPadding)
											.MinDesiredSlotWidth(DesiredWidth)
											.MinDesiredSlotHeight(DesiredHeight)
									]
							]
					]
			];

		int32 SlotIndex = 0;

#define ADD_SLOT(Button)\
		ButtonBox->AddSlot(SlotIndex++,0)\
		[\
		SNew( SButton )\
		.Text( EAppReturnTypeToText(EAppReturnType::Button) )\
		.OnClicked( this, &SChoiceDialog::HandleButtonClicked, EAppReturnType::Button )\
		.ContentPadding(ContentPadding)\
		.HAlign(HAlign_Center)\
		];

		switch ( InArgs._MessageType.Get() )
		{	
		case EAppMsgType::Ok:
			ADD_SLOT(Ok)
			break;
		case EAppMsgType::YesNo:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			break;
		case EAppMsgType::OkCancel:
			ADD_SLOT(Ok)
			ADD_SLOT(Cancel)
			break;
		case EAppMsgType::YesNoCancel:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(Cancel)
			break;
		case EAppMsgType::CancelRetryContinue:
			ADD_SLOT(Cancel)
			ADD_SLOT(Retry)
			ADD_SLOT(Continue)
			break;
		case EAppMsgType::YesNoYesAllNoAll:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			ADD_SLOT(NoAll)
			break;
		case EAppMsgType::YesNoYesAllNoAllCancel:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			ADD_SLOT(NoAll)
			ADD_SLOT(Cancel)
			break;
		case EAppMsgType::YesNoYesAll:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			break;
		default:
			UE_LOG(LogHardwareBreakpoints, Fatal, TEXT("Invalid Message Type"));
		}

#undef ADD_SLOT
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION


	EAppReturnType::Type GetResponse()
	{
		return Response;
	}

	virtual	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		//see if we pressed the Enter or Spacebar keys
		if( InKeyEvent.GetKey() == EKeys::Escape )
		{
			return HandleButtonClicked(EAppReturnType::Cancel);
		}

		if (InKeyEvent.GetKey() == EKeys::C && InKeyEvent.IsControlDown())
		{
			CopyMessageToClipboard();

			return FReply::Handled();
		}

		//if it was some other button, ignore it
		return FReply::Unhandled();
	}

	/** Override the base method to allow for keyboard focus */
	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	/** Converts an EAppReturnType into a localized FText */
	static FText EAppReturnTypeToText(EAppReturnType::Type ReturnType)
	{
		switch(ReturnType)
		{
		case EAppReturnType::No:
			return LOCTEXT("EAppReturnTypeNo", "No");
		case EAppReturnType::Yes:
			return LOCTEXT("EAppReturnTypeYes", "Yes");
		case EAppReturnType::YesAll:
			return LOCTEXT("EAppReturnTypeYesAll", "Yes All");
		case EAppReturnType::NoAll:
			return LOCTEXT("EAppReturnTypeNoAll", "No All");
		case EAppReturnType::Cancel:
			return LOCTEXT("EAppReturnTypeCancel", "Cancel");
		case EAppReturnType::Ok:
			return LOCTEXT("EAppReturnTypeOk", "OK");
		case EAppReturnType::Retry:
			return LOCTEXT("EAppReturnTypeRetry", "Retry");
		case EAppReturnType::Continue:
			return LOCTEXT("EAppReturnTypeContinue", "Continue");
		default:
			return LOCTEXT("MissingType", "MISSING RETURN TYPE");
		}
	}

protected:

	/**
	 * Copies the message text to the clipboard.
	 */
	void CopyMessageToClipboard( )
	{
		FPlatformApplicationMisc::ClipboardCopy( *MyMessage.Get().ToString() );
	}


private:

	// Handles clicking a message box button.
	FReply HandleButtonClicked( EAppReturnType::Type InResponse )
	{
		Response = InResponse;

		ResultCallback.ExecuteIfBound(ParentWindow.ToSharedRef(), Response);


		ParentWindow->RequestDestroyWindow();

		return FReply::Handled();
	}

	// Handles clicking the 'Copy Message' hyper link.
	void HandleCopyMessageHyperlinkNavigate( )
	{
		CopyMessageToClipboard();
	}
		
public:
	/** Callback delegate that is triggered, when the dialog is run in non-modal mode */
	FOnMsgDlgResult ResultCallback;

private:

	EAppReturnType::Type Response;
	TSharedPtr<SWindow> ParentWindow;
	TAttribute<FText> MyMessage;
};


void CreateMsgDlgWindow(TSharedPtr<SWindow>& OutWindow, TSharedPtr<SChoiceDialog>& OutDialog, EAppMsgType::Type InMessageType,
						const FText& InMessage, const FText& InTitle, FOnMsgDlgResult ResultCallback=NULL){
	OutWindow = SNew(SWindow)
		.Title(InTitle)
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false).SupportsMaximize(false);

	OutDialog = SNew(SChoiceDialog)
		.ParentWindow(OutWindow)
		.Message(InMessage)
		.WrapMessageAt(1200.0f)
		.MessageType(InMessageType);

	OutDialog->ResultCallback = ResultCallback;

	OutWindow->SetContent(OutDialog.ToSharedRef());
}

EAppReturnType::Type OpenMsgDlgInt(EAppMsgType::Type InMessageType, const FText& InMessage, const FText& InTitle)
{
	TSharedPtr<SWindow> MsgWindow = NULL;
	TSharedPtr<SChoiceDialog> MsgDialog = NULL;

	CreateMsgDlgWindow(MsgWindow, MsgDialog, InMessageType, InMessage, InTitle);

	// If there is already a modal window active, parent this new modal window to the existing window so that it doesn't fall behind
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveModalWindow();

	FSlateApplication::Get().AddModalWindow(MsgWindow.ToSharedRef(), ParentWindow);

	EAppReturnType::Type Response = MsgDialog->GetResponse();

	return Response;
}

#undef LOCTEXT_NAMESPACE