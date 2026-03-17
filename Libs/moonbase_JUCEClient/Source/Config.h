#pragma once
#include "JuceIncludes.h"

namespace Moonbase
{
namespace JUCEClient
{
    struct APIInitializer {
        const juce::var configJson;
        const juce::String companyName;
        const juce::String productName;
        const juce::String productVersion;

        const juce::int64 parallelOnlineValidationTimeout = 1; //minute(s)
        // Prevents duplicate online validation requests by setting a timeout after a successful one.
        // Helps avoid simultaneous requests during session startup across multiple instances.
        // A 1-minute timeout keeps (de)activation behavior responsive
        // Longer timeouts (e.g., 60min) further limit requests but may allow parallel use on different machines — use with caution.
    };

    struct AutoActivationConfig {
        const juce::URL pollUrl;
        const juce::URL browserUrl;
    };

    using AutoActivationRequestCallback = std::function<void (const AutoActivationConfig& config)>;
    using AutoActivationPollCallback = std::function<void (const bool finished, bool success, const juce::String& responseContent)>;
    using MachineFileCallback = std::function<void (bool success, const juce::File& machineFile)>;
    using DeactivationCallback = std::function<void (bool success)>;

    using ActivationStateChangedCallback = std::function<void (const bool unlocked, const bool trial, const bool offlineActivated)>;

    using GetAnalyticsCallback = std::function <const juce::StringPairArray (bool& includeExtendedDefaultAnalytics)>;

}; //end namespace JUCEClient
}; //end namespace Moonbase