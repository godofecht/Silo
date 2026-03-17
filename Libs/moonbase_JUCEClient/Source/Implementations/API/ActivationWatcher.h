#pragma once

#include "Includes.h"

namespace Moonbase
{
namespace JUCEClient
{

    class API;
    class APIImpl;

    //==============================================================================
    //==============================================================================
    class ActivationWatcher : private juce::Timer
    {
    public:
        ActivationWatcher (APIImpl& api);

        void update ();

    private:
        APIImpl& api;
        void timerCallback () override;

        JUCE_DECLARE_WEAK_REFERENCEABLE (ActivationWatcher)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationWatcher)
    };

};
};
