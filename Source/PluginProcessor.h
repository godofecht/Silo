#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

class SiloProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize  = 1 << fftOrder;  // 4096
    static constexpr int numBins  = fftSize / 2;     // 2048
    static constexpr int hopSize  = 1024;

    // ---- FFT: mid amplitude + stereo width (double-buffered) ----
    // fftMid[b]   = 0-1 normalised dB  (-80dB→0, 0dB→1)
    // fftWidth[b] = 0-1 stereo width   (0=mono, 1=fully stereo)
    float fftMid[2][numBins]{};
    float fftWidth[2][numBins]{};
    std::atomic<int>  fftReadIdx{0};
    std::atomic<bool> fftReady{false};

    // ---- Waveform ring buffer ----
    static constexpr int waveRingSize = 131072;
    float waveL[waveRingSize]{};
    float waveR[waveRingSize]{};
    std::atomic<int> waveWritePos{0};

    // ---- Lissajous ring buffer ----
    static constexpr int lissSize = 4096;
    float lissL[lissSize]{};
    float lissR[lissSize]{};
    std::atomic<int> lissWritePos{0};

    // ---- Level meters ----
    std::atomic<float> rmsL{0.f}, rmsR{0.f};
    std::atomic<float> peakL{0.f}, peakR{0.f};

    // ---- Analysis results ----
    std::atomic<float> detectedPitch{0.f};      // Hz, 0 = none
    std::atomic<int>   detectedMidiNote{-1};    // MIDI 0-127, -1 = none
    std::atomic<float> detectedCents{0.f};      // cents offset from note centre
    std::atomic<float> spectralCentroid{0.f};   // Hz

    std::atomic<double> currentSampleRate{44100.0};

    SiloProcessor();
    ~SiloProcessor() override = default;

    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Silo"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int  getNumPrograms()                       override { return 1; }
    int  getCurrentProgram()                    override { return 0; }
    void setCurrentProgram(int)                 override {}
    const juce::String getProgramName(int)      override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int)   override {}

private:
    juce::dsp::FFT                      fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize * 2> midWorkBuf{};
    std::array<float, fftSize * 2> sideWorkBuf{};
    int fftFillPos = 0;

    float peakDecayL = 0.f, peakDecayR = 0.f;
    float rmsAccL = 0.f, rmsAccR = 0.f;
    int   rmsSamples = 0;

    void runFFT();
    void computeAnalysis(const float* midMag, double sr);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SiloProcessor)
};
