#include "PluginProcessor.h"
#include "PluginEditor.h"

SiloProcessor::SiloProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      fft(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{}

void SiloProcessor::prepareToPlay(double sr, int)
{
    currentSampleRate.store(sr);
    fftFillPos = 0;
    midWorkBuf.fill(0.f);
    sideWorkBuf.fill(0.f);
    peakDecayL = peakDecayR = 0.f;
    rmsAccL = rmsAccR = 0.f;
    rmsSamples = 0;
}

void SiloProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();
    const float* L = ch > 0 ? buffer.getReadPointer(0) : nullptr;
    const float* R = ch > 1 ? buffer.getReadPointer(1) : L;

    int wPos = waveWritePos.load(std::memory_order_relaxed);
    int lPos = lissWritePos.load(std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        float l = L ? L[i] : 0.f;
        float r = R ? R[i] : 0.f;

        waveL[wPos] = l;  waveR[wPos] = r;
        wPos = (wPos + 1) & (waveRingSize - 1);

        lissL[lPos] = l;  lissR[lPos] = r;
        lPos = (lPos + 1) & (lissSize - 1);

        rmsAccL += l * l;  rmsAccR += r * r;
        rmsSamples++;

        peakDecayL = std::max(peakDecayL, std::abs(l));
        peakDecayR = std::max(peakDecayR, std::abs(r));

        // Push mid and side simultaneously
        midWorkBuf [fftFillPos] = (l + r) * 0.5f;
        sideWorkBuf[fftFillPos] = (l - r) * 0.5f;

        if (++fftFillPos >= hopSize)
        {
            // Slide the window by hopSize
            std::copy(midWorkBuf .begin() + hopSize, midWorkBuf .begin() + fftSize, midWorkBuf .begin());
            std::copy(sideWorkBuf.begin() + hopSize, sideWorkBuf.begin() + fftSize, sideWorkBuf.begin());
            std::fill(midWorkBuf .begin() + fftSize - hopSize, midWorkBuf .begin() + fftSize * 2, 0.f);
            std::fill(sideWorkBuf.begin() + fftSize - hopSize, sideWorkBuf.begin() + fftSize * 2, 0.f);
            fftFillPos = fftSize - hopSize;
            runFFT();
        }
    }

    waveWritePos.store(wPos, std::memory_order_release);
    lissWritePos.store(lPos, std::memory_order_release);

    if (rmsSamples > 0)
    {
        rmsL.store(std::sqrt(rmsAccL / rmsSamples));
        rmsR.store(std::sqrt(rmsAccR / rmsSamples));
        rmsAccL = rmsAccR = 0.f;  rmsSamples = 0;
    }
    peakL.store(peakDecayL);  peakDecayL *= 0.9997f;
    peakR.store(peakDecayR);  peakDecayR *= 0.9997f;
}

void SiloProcessor::runFFT()
{
    double sr = currentSampleRate.load();

    // ---- Mid FFT ----
    std::array<float, fftSize * 2> mBuf{};
    std::copy(midWorkBuf.begin(), midWorkBuf.begin() + fftSize, mBuf.begin());
    window.multiplyWithWindowingTable(mBuf.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(mBuf.data());

    // ---- Side FFT ----
    std::array<float, fftSize * 2> sBuf{};
    std::copy(sideWorkBuf.begin(), sideWorkBuf.begin() + fftSize, sBuf.begin());
    window.multiplyWithWindowingTable(sBuf.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(sBuf.data());

    // ---- Write to inactive buffer ----
    int wi = 1 - fftReadIdx.load(std::memory_order_acquire);
    float* mid   = fftMid  [wi];
    float* width = fftWidth[wi];
    constexpr float kNorm  = (float)(fftSize / 2);
    constexpr float kFloor = -80.f;

    for (int b = 0; b < numBins; ++b)
    {
        float mMag = mBuf[b] / kNorm;
        float sMag = sBuf[b] / kNorm;

        float dB = 20.f * std::log10(std::max(mMag, 1e-6f));
        mid[b]   = juce::jlimit(0.f, 1.f, (dB - kFloor) / (-kFloor));

        // Stereo width: 0 = fully mid, 1 = fully side
        width[b] = sMag / (mMag + sMag + 1e-6f);
    }

    fftReadIdx.store(wi, std::memory_order_release);
    fftReady.store(true, std::memory_order_release);

    // Run extra analysis on mid magnitudes (raw, pre-normalised)
    computeAnalysis(mBuf.data(), sr);
}

void SiloProcessor::computeAnalysis(const float* midMag, double sr)
{
    // ---- Spectral centroid ----
    float cNum = 0.f, cDen = 0.f;
    for (int b = 1; b < numBins; ++b)
    {
        float freq = (float)b * (float)sr / (float)fftSize;
        cNum += freq * midMag[b];
        cDen += midMag[b];
    }
    spectralCentroid.store(cDen > 0.f ? cNum / cDen : 0.f);

    // ---- HPS pitch detection (orders 2-5) ----
    const int minBin = (int)(60.f  * (float)fftSize / (float)sr);   // 60 Hz
    const int maxBin = (int)(2000.f * (float)fftSize / (float)sr);   // 2 kHz
    const int hpsMax = std::min(maxBin, numBins / 5);

    int   peakBin = minBin;
    float peakHPS = 0.f;

    for (int b = std::max(2, minBin); b < hpsMax; ++b)
    {
        float h = midMag[b];
        for (int ord = 2; ord <= 5; ++ord)
        {
            int hb = b * ord;
            if (hb < numBins) h *= midMag[hb];
        }
        if (h > peakHPS) { peakHPS = h; peakBin = b; }
    }

    // Accept only if the fundamental is within 20 dB of the loudest bin
    float maxMag = *std::max_element(midMag, midMag + numBins);
    if (midMag[peakBin] > maxMag * 0.1f && peakBin >= minBin)
    {
        float fund = (float)peakBin * (float)sr / (float)fftSize;
        detectedPitch.store(fund);

        float exactMidi = 69.f + 12.f * std::log2(fund / 440.f);
        int   roundMidi = (int)std::round(exactMidi);
        detectedMidiNote.store(juce::jlimit(0, 127, roundMidi));
        detectedCents.store((exactMidi - (float)roundMidi) * 100.f);
    }
    else
    {
        detectedPitch.store(0.f);
        detectedMidiNote.store(-1);
        detectedCents.store(0.f);
    }
}

juce::AudioProcessorEditor* SiloProcessor::createEditor() { return new SiloEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()   { return new SiloProcessor(); }
