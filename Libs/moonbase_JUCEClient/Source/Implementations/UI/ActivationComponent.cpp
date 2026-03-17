#if INCLUDE_MOONBASE_UI

#include "ActivationComponent.h"



//==============================================================================
//==============================================================================
ActivationComponent::ActivationComponent (API& api_, UI_Impl& ui_)
:
api (api_),
contentHolder (api_),
uiImpl (ui_)
{
    bg = MB_LOAD_SVG (MoonbaseBinary::BG_svg);

    addAndMakeVisible (contentHolder);
    // addAndMakeVisible (emailEditor);
    // addAndMakeVisible (pwEditor);
    // addAndMakeVisible (activateButton);

    // activateButton.setButtonText ("Activate");
    // activateButton.onClick = [&] ()
    // {
    //     api.requestOnlineActivation (emailEditor.getText (), pwEditor.getText ());
    // };
}

void ActivationComponent::update ()
{
    contentHolder.update ();
    resized ();
}

int ActivationComponent::getHeightForWidth (const int width)
{
    return width * (bg->getHeight () / (float)bg->getWidth ());
}

int ActivationComponent::getWidthForHeight (const int height)
{
    return height * (bg->getWidth () / (float)bg->getHeight ());
}

void ActivationComponent::setSpinnerLogo (std::unique_ptr<juce::Drawable> logo)
{
    contentHolder.setSpinnerLogo (std::move (logo));
}

void ActivationComponent::setSpinnerLogoScale (const float scaleNormalized)
{
    contentHolder.setSpinnerLogoScale (scaleNormalized);
}

void ActivationComponent::paint  (juce::Graphics& g)
{
    const auto area = getLocalBounds ().toFloat ();
    if (bg != nullptr)
        bg->drawWithin (g, area, juce::RectanglePlacement::centred, 1.0f);
}

void ActivationComponent::resized ()
{
    auto area = getLocalBounds ();
    contentHolder.setBounds (area);
    contentHolder.resized ();
}

#endif
