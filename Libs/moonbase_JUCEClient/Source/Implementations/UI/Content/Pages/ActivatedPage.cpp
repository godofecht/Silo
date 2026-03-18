#if INCLUDE_MOONBASE_UI
#include "ActivatedPage.h"

#include "../ContentHolder.h"
#include "../../ActivationComponent.h"
#include "../../UI_Impl.h"

//==============================================================================

ActivatedPage::ActivatedPage (ContentHolder& parent, API& api_)
    : ContentBase (parent, api_)
{
    addAndMakeVisible (text);
    text.setReadOnly (true);
    text.setMultiLine (true, true);

    text.setColour (juce::TextEditor::ColourIds::backgroundColourId,  juce::Colour (0));
    text.setColour (juce::TextEditor::ColourIds::outlineColourId,  juce::Colour (0));
    text.setColour (juce::TextEditor::ColourIds::focusedOutlineColourId,  juce::Colour (0));
    text.setColour (juce::TextEditor::ColourIds::highlightColourId, juce::Colours::white.withAlpha (0.15f));
    text.setColour (juce::TextEditor::ColourIds::highlightedTextColourId,  juce::Colour (0xFFFFFFFF));
    text.setColour (juce::TextEditor::ColourIds::textColourId,  juce::Colour (0xFFFFFFFF));

    addChildComponent (deactivateButton);
    deactivateButton.onClick = std::bind (&ActivatedPage::handleDeactivateButtonClicked, this);
    deactivateButton.setLookAndFeel (&wideTextButtonLAF);
    wideTextButtonLAF.setRed();
}

ActivatedPage::~ActivatedPage ()
{
    deactivateButton.setLookAndFeel (nullptr);
}

void ActivatedPage::pageBecomingVisible ()
{
    ContentBase::pageBecomingVisible ();

    static const juce::String copyrightSym { juce::CharPointer_UTF8 ("\xc2\xa9") };
    juce::String content = copyrightSym;
    content << " " << juce::String(juce::Time::getCurrentTime ().getYear ()) << " " << api.getCompanyName () << ". All rights reserved.\n";
    content << "Version: " << api.getProductVersion () << "\n";
    content << "License Type: " << juce::String (api.isOfflineActivated () ? "Offline" : "Online") << "\n";
    content << "Registered to: " << api.getUserEmail () << "\n";
    content << "License ID: " << api.getLicenseId () << "\n";

    text.setText (content);
    text.setJustification ( juce::Justification::centred);
    text.applyColourToAllText ( juce::Colour (0xFFFFFFFF));

    addChildComponent (waitingOverlay);

    deactivateButton.setVisible (!api.isOfflineActivated ());
}

ContentBase::BackButtonConfig ActivatedPage::getBackButtonConfig ()
{
    return { false };
}

float ActivatedPage::getErrorDisplayCentreYRelative ()
{
    return (0.1f); //off screen, don't need an error display here and hiding it just in case
}

StringPair ActivatedPage::getTitleAndSubtitle ()
{
    return { "", "" };
}

void ActivatedPage::handleDeactivateButtonClicked ()
{
    waitingOverlay.show ("Deactivating license...");
    api.deactivateLicense ([&](const bool success)
    {
        if (success)
            contentHolder.setNextPage (ContentHolder::Welcome, false, true);

        waitingOverlay.hide ();
    });
}

void ActivatedPage::resized ()
{
    ContentBase::resized ();
    const auto area = getLocalBounds ();

    const auto fontHeight = getHeight () * (0.03f);
    text.applyFontToAllText ( juce::Font ().withHeight (fontHeight));

    const auto textX = getWidth () * (0.1f);
    const auto textY = getHeight () * (0.35f);
    const auto textW = getWidth () * (0.8f);
    const auto textH = getHeight () * (0.4f);

    text.setBounds (textX, textY, textW, textH);

    if (companyLogo != nullptr && companyLogo->getParentComponent () == this)
    {
        companyLogo->setVisible (true);
        float scale = 1.0f;
        if (auto activationComp = findParentComponentOfClass<ActivationComponent> ())
        {
            scale = activationComp->uiImpl.getCompanyLogoScale ();
        }
        else jassertfalse;

        const auto logoSize = getWidth () * (0.125f) * scale;
        const auto logoCentreY = getHeight () * (0.26f);
         juce::Rectangle<int> logoArea (logoSize, logoSize);
        logoArea.setCentre (area.getCentreX(), logoCentreY);
        companyLogo->setBounds (logoArea);
    }



    deactivateButton.setBounds (bottomCenterWideButtonArea);

    waitingOverlay.setBounds (area);
}
#endif
