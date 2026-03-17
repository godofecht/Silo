#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../../JuceIncludes.h"
#include "../../ModuleIncludes.h"
#include "../ContentBase.h"
namespace Moonbase
{
namespace JUCEClient
{


    //==============================================================================
    class UI_Impl;
    class WelcomePage : public ContentBase
    {
    public:
        WelcomePage (ContentHolder& parent, API& api_);
        ~WelcomePage () override;

    private:
        void initUiImplPtr ();
        UI_Impl* uiImpl = nullptr;
         juce::ToggleButton activatePluginButton;

        void paint  (juce::Graphics& g) override;
        void resized () override;

        float getErrorDisplayCentreYRelative () override;

        JUCE_DECLARE_WEAK_REFERENCEABLE (WelcomePage)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WelcomePage)
    };

}; //end namespace JUCEClient
}; //end namespace Moonbase

#endif
