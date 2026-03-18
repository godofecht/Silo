#if INCLUDE_MOONBASE_UI
#include "LoginPage.h"
#include "../../Fonts.h"
//==============================================================================

LoginPage::LoginPage (ContentHolder& parent, API& api_)
:
ContentBase (parent, api_)
{
    const auto setupEditor = [&](juce::TextEditor& editor)
    {
        addAndMakeVisible (editor);
        editor.setLookAndFeel (&textEditorLAF);
        editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor.setColour (juce::TextEditor::highlightColourId, juce::Colours::white.withAlpha (0.35f));
        editor.setColour (juce::TextEditor::highlightedTextColourId,  juce::Colour (0xFFFFFFFF));
        editor.setColour (juce::TextEditor::textColourId,  juce::Colour (0xFFFFFFFF));
        editor.setColour (juce::CaretComponent::caretColourId,  juce::Colour (0xFFFFFFFF));
        editor.onTextChange = std::bind (&LoginPage::textEditorsChanged, this);
        editor.onReturnKey = std::bind (&LoginPage::loginButtonClicked, this);
    };

    setupEditor (emailEditor);


    setupEditor (passwordEditor);
    passwordEditor.setPasswordCharacter ('*');


    addAndMakeVisible (forgotPasswordButton);
    forgotPasswordButton.setLookAndFeel (&textLinkLAF);

    addAndMakeVisible (loginButton);
    loginButton.setLookAndFeel (&narrowTextButtonLAF);
    loginButton.onClick = std::bind (&LoginPage::loginButtonClicked, this);

    textEditorsChanged ();
}

LoginPage::~LoginPage ()
{
    emailEditor.setLookAndFeel (nullptr);
    passwordEditor.setLookAndFeel (nullptr);
    forgotPasswordButton.setLookAndFeel (nullptr);
    loginButton.setLookAndFeel (nullptr);
}

void LoginPage::textEditorsChanged ()
{
    loginButton.setEnabled (!emailEditor.getText ().isEmpty () && !passwordEditor.getText ().isEmpty ());
}

void LoginPage::loginButtonClicked ()
{
    const auto email = emailEditor.getText ();
    const auto password = passwordEditor.getText ();

    if (email.isEmpty () || password.isEmpty ())
        return;

    waitingOverlay.show ("");
    api.requestOnlineActivation (email, password);
}

ContentBase::BackButtonConfig LoginPage::getBackButtonConfig ()
{
    return { true, [&]() {
        contentHolder.setNextPage (ContentHolder::ActivationMethod, false, true);
    }};
}

float LoginPage::getErrorDisplayCentreYRelative ()
{
    return (0.23f);
}

StringPair LoginPage::getTitleAndSubtitle ()
{
    return { "License Activation", "Enter your email address and password" };
}

void LoginPage::mouseUp (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition ();
    if (emailEditor.getBounds ().contains (pos) || passwordEditor.getBounds ().contains (pos))
        return;

    grabKeyboardFocus ();
}

void LoginPage::paint  (juce::Graphics& g)
{
    ContentBase::paint (g);
    const auto headlineTranslationY = getHeight () *  (-0.05f);
    const auto headlineTranslationX = getWidth () *  (0.02f);

    const auto emailTextArea = emailEditor.getBounds ().translated (headlineTranslationX, headlineTranslationY);
    const auto passwordTextArea = passwordEditor.getBounds ().translated (headlineTranslationX, headlineTranslationY);

     juce::Font f (Roboto::Regular ().withHeight (getHeight () * (0.035f)));
    g.setColour ( juce::Colour (0xFFFFFFFF));
    g.setFont (f);
    g.drawText ("E-Mail", emailTextArea,  juce::Justification::topLeft);
    g.drawText ("Password", passwordTextArea,  juce::Justification::topLeft);
}

void LoginPage::resized ()
{
    ContentBase::resized ();
    const auto area = getLocalBounds ();
    const auto editorHeight = area.getHeight () *  (0.105f);
    const auto editorWidth = textEditorLAF.getWidthForHeight (editorHeight);

     juce::Rectangle<int> editorArea (editorWidth, editorHeight);

    const int emailCentreY = area.getHeight () *  (0.39f);
    const int passwordCentreY = area.getHeight () *  (0.6f);

    emailEditor.setBounds (editorArea.withCentre ({area.getCentreX(), emailCentreY}));
    passwordEditor.setBounds (editorArea.withCentre ({area.getCentreX(), passwordCentreY}));

     juce::Font f = Roboto::Regular ().withHeight (editorHeight * (0.35f));
    emailEditor.setFont (f);
    emailEditor.applyFontToAllText (f);

    const auto editorBorder = emailEditor.getHeight () * (0.2);
    emailEditor.setBorder (juce::BorderSize<int> (editorBorder));

    passwordEditor.setFont (f);
    passwordEditor.applyFontToAllText (f);
    passwordEditor.setBorder (juce::BorderSize<int> (editorBorder));

    const  juce::Rectangle<int> forgotPasswordArea (getWidth() * (0.45f),
                                             getHeight() * (0.68f),
                                             getWidth() * (0.4f),
                                             getHeight() * (0.037f));
    forgotPasswordButton.setBounds (forgotPasswordArea);

    loginButton.setBounds (bottomRightSmallButtonArea);
    waitingOverlay.setBounds (bottomRightSmallButtonArea.expanded (bottomRightSmallButtonArea.getHeight () * (0.25f)));
}
#endif
