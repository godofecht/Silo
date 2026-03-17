#pragma once

#include "JuceIncludes.h"
   
/**
 * @name      MOONBASE_DECLARE_LICENSING macros
 * 
 * @brief     These macros are used to declare usage of the  licensing API in your project.
 * 
 *            If you added your "_api_config.json" file to your Projucer project,
 *            it's enough in most cases to just declare MOONBASE_DECLARE_LICENSING_USING_JUCE_PROJECTINFO 
 *            in the private section of your AudioProcessor class.
 * 
 *            If you're using a custom BinaryData namespace to store your "_api_config.json" file,
 *            you can use MOONBASE_DECLARE_LICENSING_NAMED (BinaryDataName) instead.
 * 
 *            Lastly you can opt to construct Moonbase::JUCEClient::API yourself, 
 *            passing in the _api_config.json as parsed juce::var. 
 * 
 *            See JUCEClientAPI.h for more information (moonbase_JUCEClient/Source/JUCEClientAPI.h)
*/

#define _MOONBASE_GET_CONFIG_JSON(BinaryDataName) \
static inline const juce::var _getConfigJson() \
{ \
    for (int r = 0; r < BinaryDataName::namedResourceListSize; r++) \
    { \
        const juce::String originalFileName { BinaryDataName::originalFilenames [r] }; \
        if (originalFileName == "moonbase_api_config.json") \
        { \
            const auto name { BinaryDataName::namedResourceList[r] }; \
            int size; \
            auto resource = BinaryDataName::getNamedResource (name, size); \
            if (size > 0) \
            { \
                return juce::JSON::parse (juce::String::fromUTF8 (resource, size)); \
            } \
        } \
    } \
    return {}; \
};

#define MOONBASE_IMPLEMENT_CONFIG_GETTER \
    _MOONBASE_GET_CONFIG_JSON (BinaryData)

#define MOONBASE_GET_CONFIG \
    _getConfigJson()

#define MOONBASE_API_INIT(companyName, productName, productVersion) \
    Moonbase::JUCEClient::APIInitializer{_getConfigJson(), companyName, productName, productVersion}

#define MOONBASE_API_INIT_PROJECT_INFO \
   MOONBASE_API_INIT (ProjectInfo::companyName, ProjectInfo::projectName, ProjectInfo::versionString)

#define MOONBASE_INIT_API(companyName, productName, productVersion) \
    std::make_unique<Moonbase::JUCEClient::API> (MOONBASE_API_INIT(companyName, productName, productVersion))

#define MOONBASE_DECLARE_MEMBER \
    std::unique_ptr<Moonbase::JUCEClient::API> moonbaseClient    

#define MOONBASE_DECLARE_LICENSING_NAMED(BinaryDataName, companyName, productName, productVersion) \
    _MOONBASE_GET_CONFIG_JSON(BinaryDataName) \
    MOONBASE_DECLARE_MEMBER { MOONBASE_INIT_API(companyName, productName, productVersion) };

#define MOONBASE_DECLARE_LICENSING(companyName, productName, productVersion) \
    MOONBASE_DECLARE_LICENSING_NAMED (BinaryData, companyName, productName, productVersion)

#define MOONBASE_DECLARE_LICENSING_USING_JUCE_PROJECTINFO \
    MOONBASE_DECLARE_LICENSING (ProjectInfo::companyName, ProjectInfo::projectName, ProjectInfo::versionString)

#define MOONBASE_DECLARE_ACTIVATION_UI \
    std::unique_ptr<Moonbase::JUCEClient::ActivationUI> activationUI

#define MOONBASE_INIT_ACTIVATION_UI(processor) \
    activationUI.reset (processor.moonbaseClient->createActivationUi(*this));

#define MOONBASE_DECLARE_AND_INIT_ACTIVATION_UI(processor) \
    MOONBASE_DECLARE_ACTIVATION_UI { processor.moonbaseClient->createActivationUi(*this) };

#define MOONBASE_DECLARE_AND_INIT_ACTIVATION_UI_SAME_PARENT \
    MOONBASE_DECLARE_ACTIVATION_UI { moonbaseClient->createActivationUi(*this) };

#define MOONBASE_SHOW_ACTIVATION_UI \
    if (activationUI) \
        activationUI->show();
        
#define MOONBASE_RESIZE_ACTIVATION_UI \
    if (activationUI) \
        activationUI->setBounds (getLocalBounds());

#define MOONBASE_DECLARE_IMPLEMENTATION \
    void* impl = nullptr;  

#define MOONBASE_PREPARE_TO_PLAY(sr, bs) \
    if (moonbaseClient != nullptr) \
        moonbaseClient->prepareToPlay (sr, bs);

#define MOONBASE_PROCESS(buf) \
    if (moonbaseClient != nullptr) \
        moonbaseClient->processBlock (buf);

#define MB_IS_UNLOCKED_OBFUSCATED(api) \
    [&]() -> std::pair<juce::var, juce::String> { \
    OBF_BEGIN \
        const bool trueVal = true; \
        const bool falseVal = false; \
        const auto res = api.isUnlocked (); \
        const bool first = res.first; \
        const auto second = res.second; \
        IF (V(first) == V(falseVal) || V(first) == V(trueVal))\
            const auto returnPair = std::pair<juce::var, juce::String> {first, second}; \
            RETURN(returnPair); \
        ENDIF \
    OBF_END \
        return {juce::var(false), ""}; \
    }()