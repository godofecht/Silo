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
    {
        juce::ColourGradient grad (
            juce::Colour (0xFF2A1A30), area.getX (), area.getY (),
            juce::Colour (0xFF1A1028), area.getRight (), area.getBottom (),
            false);
        grad.addColour (0.3, juce::Colour (0xFF3D1F3D));
        grad.addColour (0.6, juce::Colour (0xFF251535));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (area.reduced (1.0f), 10.0f);
    }
    {
        juce::ColourGradient glow (
            juce::Colour (0x18F06B6B), area.getX () + area.getWidth () * 0.2f, area.getY () + area.getHeight () * 0.15f,
            juce::Colours::transparentBlack, area.getCentreX (), area.getCentreY (),
            true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (area.reduced (2.0f), 9.0f);
    }
    g.setColour (juce::Colour (0x20FFFFFF));
    g.drawRoundedRectangle (area.reduced (1.5f), 10.0f, 1.0f);
}

void ActivationComponent::resized ()
{
    auto area = getLocalBounds ();
    contentHolder.setBounds (area);
    contentHolder.resized ();
}

#endif
