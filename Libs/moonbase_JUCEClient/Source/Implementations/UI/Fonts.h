#if INCLUDE_MOONBASE_UI
#pragma once

#include "../../JuceIncludes.h"
#include "../../../Assets/MoonbaseBinary.h"

namespace Roboto
{
    static inline  juce::Font& Regular ()
    {
        static  juce::Font f ( juce::Font (juce::Typeface::createSystemTypefaceFor (MoonbaseBinary::RobotoRegular_ttf,
                                                                MoonbaseBinary::RobotoRegular_ttfSize))
                                                                .withHorizontalScale (1.0f)
                                                                .withExtraKerningFactor (0.05f));
        return f;
    };
};
#endif
