#pragma once

#include "Includes.h"
#include "HttpHelper.h"

namespace Moonbase
{
namespace JUCEClient
{
    class APIImpl;
    class OnlineValidator
    {
    public:
        OnlineValidator (APIImpl& api);
        ~OnlineValidator ();

        void startNow ();

    private:
        APIImpl& api;

        std::unique_ptr<HttpHelper> httpHelper;

        void delayedStart ();

        const juce::int64 revalidationInterval = 1;//min
        void handleValidationResponse (bool wasOk, const juce::String& response, const int statusCode);

        JUCE_DECLARE_WEAK_REFERENCEABLE (OnlineValidator)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnlineValidator)
    };

}; // end namespace JUCEClient
}; // end namespace Moonbase
