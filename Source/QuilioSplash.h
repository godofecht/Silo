#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>

// ── Quilio splash screen + Moonbase theming shared across all plugins ────────
//
// Usage in your editor:
//   1. Add `#include "../../shared/QuilioSplash.h"`
//   2. Add member: `QuilioSplash quilioSplash;`
//   3. In paint(), at the very end: `quilioSplash.paint(g, getLocalBounds());`
//   4. In timerCallback(): `quilioSplash.tick();`
//   5. In constructor after activationUI init:
//      `QuilioSplash::themeActivationUI(activationUI, "Plugin Name");`

namespace MoonbaseBinary
{
    extern const char* RobotoRegular_ttf;
    extern const int   RobotoRegular_ttfSize;
}

struct QuilioSplash
{
    float alpha = 1.0f;
    int frames  = 0;
    static constexpr int kHoldFrames = 55;  // ~1.8s at 30fps

    bool isVisible() const noexcept { return alpha > 0.01f; }

    void tick()
    {
        if (alpha <= 0.0f) return;
        frames++;
        if (frames > kHoldFrames)
            alpha += (0.0f - alpha) * 0.08f;
        if (alpha < 0.015f)
            alpha = 0.0f;
    }

    void paint(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (!isVisible()) return;

        const float W = (float)bounds.getWidth();
        const float H = (float)bounds.getHeight();

        // Flowing gradient background (Q logo palette: plum → purple → navy)
        {
            juce::ColourGradient bg(
                juce::Colour(0xFF1A1028), 0, 0,
                juce::Colour(0xFF2A1A30), W, H, false);
            bg.addColour(0.4, juce::Colour(0xFF251535));
            g.setGradientFill(bg);
            g.setOpacity(alpha);
            g.fillRect(bounds);
        }

        // Warm glow top-left
        {
            juce::ColourGradient glow(
                juce::Colour(0x15F06B6B), W * 0.25f, H * 0.2f,
                juce::Colours::transparentBlack, W * 0.5f, H * 0.5f, true);
            g.setGradientFill(glow);
            g.setOpacity(alpha);
            g.fillRect(bounds);
        }

        // "Made with" — Roboto
        {
            auto roboto = juce::Font(juce::FontOptions(
                juce::Typeface::createSystemTypefaceFor(
                    MoonbaseBinary::RobotoRegular_ttf, MoonbaseBinary::RobotoRegular_ttfSize)));
            g.setFont(roboto.withHeight(13.0f));
            g.setColour(juce::Colour(0xFFB0A0B8).withAlpha(alpha * 0.65f));
            g.drawText("Made with", bounds.withHeight(20).withY((int)(H * 0.5f - 68)),
                        juce::Justification::centred);
        }

        // Q logo
        static auto qImg = juce::ImageFileFormat::loadFrom(BinaryData::quilio_Q_png,
                                                             BinaryData::quilio_Q_pngSize);
        if (qImg.isValid())
        {
            float logoH = juce::jmin(100.0f, H * 0.38f);
            float logoW = logoH * ((float)qImg.getWidth() / (float)qImg.getHeight());
            const auto dest = juce::Rectangle<float>(
                W * 0.5f - logoW * 0.5f,
                H * 0.5f - logoH * 0.5f + 5.0f,
                logoW,
                logoH);
            g.setOpacity(alpha);
            g.drawImage(qImg, dest);
        }

        // "quilio engine" + "quilio.dev"
        {
            auto roboto = juce::Font(juce::FontOptions(
                juce::Typeface::createSystemTypefaceFor(
                    MoonbaseBinary::RobotoRegular_ttf, MoonbaseBinary::RobotoRegular_ttfSize)));

            g.setFont(roboto.withHeight(15.0f));
            g.setColour(juce::Colours::white.withAlpha(alpha * 0.85f));
            float ty = H * 0.5f + 56.0f;
            g.drawText("quilio engine", bounds.withY((int)ty).withHeight(18),
                        juce::Justification::centred);

            g.setFont(roboto.withHeight(9.0f));
            g.setColour(juce::Colour(0xFF9080A0).withAlpha(alpha * 0.45f));
            g.drawText("quilio.dev", bounds.withY((int)ty + 19).withHeight(12),
                        juce::Justification::centred);
        }
    }

    // ── Call this on activationUI after it's created ─────────────────────
    static void themeActivationUI(
        std::unique_ptr<Moonbase::JUCEClient::ActivationUI>& activationUI,
        const juce::String& pluginName)
    {
        if (!activationUI) return;

        activationUI->setWelcomePageText(pluginName, "by Quilio");

        // Q logo as company logo at 2x
        class QLogo : public juce::Component
        {
        public:
            QLogo() { img = juce::ImageFileFormat::loadFrom(BinaryData::quilio_Q_png,
                                                              BinaryData::quilio_Q_pngSize); }
            void paint(juce::Graphics& g) override {
                if (img.isValid())
                    g.drawImage(img, getLocalBounds().toFloat().reduced(2),
                                juce::RectanglePlacement::centred);
            }
        private:
            juce::Image img;
        };
        activationUI->setCompanyLogo(std::make_unique<QLogo>());
        activationUI->setCompanyLogoScale(2.0f);

        // Spinner
        auto spinImg = juce::ImageFileFormat::loadFrom(BinaryData::quilio_Q_png,
                                                        BinaryData::quilio_Q_pngSize);
        if (spinImg.isValid())
        {
            auto d = std::make_unique<juce::DrawableImage>();
            d->setImage(spinImg);
            activationUI->setSpinnerLogo(std::move(d));
            activationUI->setSpinnerLogoScale(0.4f);
        }
    }
};

class QuilioSplashOverlay : public juce::Component, private juce::Timer
{
public:
    explicit QuilioSplashOverlay(QuilioSplash& splashToUse) : splash(splashToUse)
    {
        setAlwaysOnTop(true);
        setOpaque(false);
        setInterceptsMouseClicks(true, true);
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        splash.paint(g, getLocalBounds());
    }

    void visibilityChanged() override
    {
        if (isVisible())
            toFront(false);
    }

private:
    void timerCallback() override
    {
        const bool wasVisible = splash.isVisible();
        splash.tick();
        const bool nowVisible = splash.isVisible();

        if (wasVisible || nowVisible)
            repaint();

        if (nowVisible)
            toFront(false);

        setVisible(nowVisible);
    }

    QuilioSplash& splash;
};
