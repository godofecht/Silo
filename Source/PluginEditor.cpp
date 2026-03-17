#include "PluginEditor.h"

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int kOuter  = 8;    // outer margin all sides
static constexpr int kHdr    = 24;   // header strip height
static constexpr int kCtrl   = 26;   // controls bar height
static constexpr int kGap    = 5;    // gap between panel frames
static constexpr int kFrm    = 1;    // panel frame border thickness
static constexpr int kMeterW = 52;   // level-meter panel width
static constexpr int kCorrW  = 96;   // correlation panel width

// ── Colour tokens ─────────────────────────────────────────────────────────────
static const juce::Colour kBg           { 0xff030407 };
static const juce::Colour kHdrBg        { 0xff060a14 };
static const juce::Colour kCtrlBg       { 0xff05080f };
static const juce::Colour kHdrBorder    { 0xff0c1420 };
static const juce::Colour kOuterBorder  { 0xff182038 };
static const juce::Colour kFrameBorder  { 0xff141c2c };
static const juce::Colour kPanelLabel   { 0xff1e3048 };
static const juce::Colour kTitle        { 0xff8fa8c4 };
static const juce::Colour kSubtitle     { 0xff263a50 };
static const juce::Colour kLegendText   { 0xff405870 };
static const juce::Colour kSepLine      { 0xff0c1624 };
static const juce::Colour kCtrlLabel    { 0xff1e3040 };

// ── Panels: all five frame rects + control bar ────────────────────────────────
struct Panels
{
    juce::Rectangle<int> spec, ana, osc, lvl, corr, liss, ctrl;

    Panels(int W, int H)
    {
        int cx    = kOuter;
        int ctrlY = kOuter + kHdr + kGap;
        int cy    = ctrlY + kCtrl + kGap;   // main content starts below controls bar
        int cW    = W - kOuter * 2;
        int cH    = H - cy - kOuter;

        int mW    = cW * 4 / 5;             // left column  (spectrogram / osc)
        int rW    = cW - mW - kGap;         // right column (analyser / lvl+corr+liss)
        int tH    = cH * 79 / 100;          // top row      (spectrogram / analyser)
        int bH    = cH - tH - kGap;         // bottom row
        int lW    = rW - kMeterW - kCorrW - kGap * 2; // lissajous

        ctrl = { cx,                                          ctrlY,  cW,      kCtrl };
        spec = { cx,                                          cy,     mW,      tH    };
        ana  = { cx + mW + kGap,                              cy,     rW,      tH    };
        osc  = { cx,                                          cy+tH+kGap, mW,  bH    };
        lvl  = { cx + mW + kGap,                              cy+tH+kGap, kMeterW, bH };
        corr = { cx + mW + kGap + kMeterW + kGap,             cy+tH+kGap, kCorrW,  bH };
        liss = { cx + mW + kGap + kMeterW + kGap + kCorrW + kGap, cy+tH+kGap, lW,  bH };
    }
};

// ── Helpers ───────────────────────────────────────────────────────────────────
static void radioOn(juce::TextButton& btn)
{
    btn.setToggleState(true, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────

SiloEditor::SiloEditor(SiloProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    // ── Visualiser panels ──────────────────────────────────────────────────
    spectrogram  = std::make_unique<SpectrogramGL>(p);
    analyser     = std::make_unique<AnalyserGL>(p);
    oscilloscope = std::make_unique<OscilloscopeGL>(p);
    lissajous    = std::make_unique<LissajousGL>(p);
    levelMeter   = std::make_unique<LevelMeterView>(p);
    corrMeter    = std::make_unique<CorrelationMeterView>(p);

    addAndMakeVisible(*spectrogram);
    addAndMakeVisible(*analyser);
    addAndMakeVisible(*oscilloscope);
    addAndMakeVisible(*lissajous);
    addAndMakeVisible(*levelMeter);
    addAndMakeVisible(*corrMeter);

    // ── Apply custom LAF to entire editor (all child buttons inherit it) ───
    setLookAndFeel(&siloLAF);

    // ── Controls: FREEZE ──────────────────────────────────────────────────
    btnFreeze.setClickingTogglesState(true);
    btnFreeze.onClick = [this] {
        freeze_ = btnFreeze.getToggleState();
    };
    addAndMakeVisible(btnFreeze);

    // ── Controls: SPEED (radio group 1) ───────────────────────────────────
    for (auto* b : { &btnSlow, &btnMed, &btnFast })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(1);
        addAndMakeVisible(b);
    }
    radioOn(btnMed);
    btnSlow.onClick = [this] { speedMode_ = 0; skipCount_ = 0; };
    btnMed .onClick = [this] { speedMode_ = 1; skipCount_ = 0; };
    btnFast.onClick = [this] { speedMode_ = 2; skipCount_ = 0; };

    // ── Controls: SLOPE (radio group 2) ───────────────────────────────────
    for (auto* b : { &btnSlope0, &btnSlope3, &btnSlope45 })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(2);
        addAndMakeVisible(b);
    }
    radioOn(btnSlope0);
    btnSlope0 .onClick = [this] { analyser->setSlope(0.f);  };
    btnSlope3 .onClick = [this] { analyser->setSlope(3.f);  };
    btnSlope45.onClick = [this] { analyser->setSlope(4.5f); };

    // ── Controls: FLOOR (radio group 3) ───────────────────────────────────
    for (auto* b : { &btnFloor60, &btnFloor80, &btnFloor100 })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(3);
        addAndMakeVisible(b);
    }
    radioOn(btnFloor80);
    btnFloor60 .onClick = [this] { spectrogram->setBias(0.25f); };
    btnFloor80 .onClick = [this] { spectrogram->setBias(0.09f); };
    btnFloor100.onClick = [this] { spectrogram->setBias(0.02f); };

    // ── Window setup ──────────────────────────────────────────────────────
    setResizable(true, false);
    setResizeLimits(800, 480, 2400, 1350);
    setSize(1200, 620);
    startTimerHz(30);
}

SiloEditor::~SiloEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ── Timer: feed data to visualisers ──────────────────────────────────────────
void SiloEditor::timerCallback()
{
    if (proc.fftReady.exchange(false, std::memory_order_acq_rel))
    {
        if (!freeze_)
        {
            bool doPush = true;
            if      (speedMode_ == 0) doPush = ((skipCount_++ & 1) == 0);  // 0.5×
            else if (speedMode_ == 2) skipCount_ = 0;                       // 2×

            if (doPush)
            {
                int    idx = proc.fftReadIdx.load(std::memory_order_acquire);
                double sr  = proc.currentSampleRate.load();
                spectrogram->pushColumn(proc.fftMid[idx], proc.fftWidth[idx],
                                        SiloProcessor::numBins, sr);
                if (speedMode_ == 2)   // double push for 2× visual speed
                    spectrogram->pushColumn(proc.fftMid[idx], proc.fftWidth[idx],
                                            SiloProcessor::numBins, sr);
                analyser->updateData(proc.fftMid[idx], SiloProcessor::numBins, sr);
            }
        }
    }
    oscilloscope->triggerRepaint();
    lissajous->triggerRepaint();
    levelMeter->repaint();
    corrMeter->repaint();
}

// ── Background + header (painted behind all children) ────────────────────────
void SiloEditor::paint(juce::Graphics& g)
{
    int W = getWidth(), H = getHeight();

    // Base fill
    g.fillAll(kBg);

    // Header strip
    g.setColour(kHdrBg);
    g.fillRect(0, 0, W, kOuter + kHdr);

    // Controls bar background
    Panels p(W, H);
    g.setColour(kCtrlBg);
    g.fillRect(p.ctrl);

    // Outer plugin frame
    g.setColour(kOuterBorder);
    g.drawRect(kOuter / 2, kOuter / 2, W - kOuter, H - kOuter, 1);

    // Header bottom hairline
    g.setColour(kHdrBorder);
    g.fillRect(kOuter, kOuter + kHdr - 1, W - kOuter * 2, 1);

    // ── Logo ──────────────────────────────────────────────────────────────
    int hy = kOuter, hH = kHdr;
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(14.f).withStyle("Bold")));
    g.setColour(kTitle);
    g.drawText("SILO", kOuter + 6, hy, 38, hH, juce::Justification::centredLeft, false);

    g.setColour(kSepLine.brighter(0.8f));
    g.fillRect(kOuter + 47, hy + 5, 1, hH - 10);

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.f)));
    g.setColour(kSubtitle.brighter(0.5f));
    g.drawText("Spectrum Analyzer", kOuter + 52, hy + 1, 130, hH,
               juce::Justification::centredLeft, false);

    // ── Legend (over the analyser column) ────────────────────────────────
    int lx = p.ana.getX() + 6, ly = hy;
    auto legend = [&](int x, juce::Colour c, const char* txt) {
        g.setColour(c);
        g.fillRect(x, ly + hH / 2 - 3, 7, 6);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f)));
        g.setColour(kLegendText);
        g.drawText(txt, x + 9, ly, 40, hH, juce::Justification::centredLeft, false);
    };
    legend(lx,        juce::Colour(0xff00b8d8).withAlpha(0.85f), "Mid");
    legend(lx + 44,   juce::Colour(0xffff8820).withAlpha(0.85f), "Side");
    legend(lx + 92,   juce::Colour(0xff30d8c0).withAlpha(0.85f), "Centroid");
    legend(lx + 152,  juce::Colour(0xff90e030).withAlpha(0.85f), "Pitch");

    // ── Controls bar group labels ─────────────────────────────────────────
    auto ctrlR = p.ctrl.reduced(kFrm);
    int cy2 = ctrlR.getY();
    int bH  = ctrlR.getHeight();

    // Derive x positions from button layout (mirrors resized() positions)
    // Groups are: FREEZE | SPEED | SLOPE | FLOOR
    // We'll draw thin separator lines and group labels above each group
    int x0 = ctrlR.getX() + 4;

    auto groupLabel = [&](int gx, const char* txt) {
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.f).withStyle("Bold")));
        g.setColour(kCtrlLabel);
        g.drawText(txt, gx, cy2 + 1, 60, 8, juce::Justification::left, false);
    };

    // Separator between groups
    auto sep = [&](int sx) {
        g.setColour(kSepLine);
        g.fillRect(sx, cy2 + 4, 1, bH - 8);
    };

    groupLabel(x0,           "FREEZE");
    sep(x0 + 58);
    groupLabel(x0 + 64,      "SPEED");
    sep(x0 + 64 + 118);
    groupLabel(x0 + 188,     "SLOPE");
    sep(x0 + 188 + 178);
    groupLabel(x0 + 372,     "FLOOR");
}

// ── Frames + panel labels painted over all children ──────────────────────────
void SiloEditor::paintOverChildren(juce::Graphics& g)
{
    int W = getWidth(), H = getHeight();
    Panels p(W, H);

    auto drawPanel = [&](juce::Rectangle<int> r, const char* label) {
        g.setColour(kFrameBorder);
        g.drawRect(r, kFrm);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.f).withStyle("Bold")));
        g.setColour(kPanelLabel);
        g.drawText(label, r.getX() + 5, r.getY() + 3, 110, 10,
                   juce::Justification::left, false);
    };

    drawPanel(p.spec, "SPECTROGRAM");
    drawPanel(p.ana,  "ANALYSER");
    drawPanel(p.osc,  "WAVEFORM");
    drawPanel(p.lvl,  "LVL");
    drawPanel(p.corr, "CORRELATION");
    drawPanel(p.liss, "STEREO SCOPE");
    drawPanel(p.ctrl, "");   // just the frame, no label (group labels drawn in paint)
}

// ── Layout ────────────────────────────────────────────────────────────────────
void SiloEditor::resized()
{
    int W = getWidth(), H = getHeight();
    Panels p(W, H);

    // Visualiser panels: inset 1 px inside frame
    spectrogram ->setBounds(p.spec.reduced(kFrm));
    analyser    ->setBounds(p.ana .reduced(kFrm));
    oscilloscope->setBounds(p.osc .reduced(kFrm));
    levelMeter  ->setBounds(p.lvl .reduced(kFrm));
    corrMeter   ->setBounds(p.corr.reduced(kFrm));
    lissajous   ->setBounds(p.liss.reduced(kFrm));

    // Controls bar buttons
    auto ctrlR = p.ctrl.reduced(kFrm);
    int bH = ctrlR.getHeight() - 6;
    int bY = ctrlR.getY() + 3;
    int x  = ctrlR.getX() + 4;

    // FREEZE
    btnFreeze.setBounds(x, bY, 52, bH); x += 56;
    x += 6;  // separator gap

    // SPEED (3 buttons)
    btnSlow .setBounds(x, bY, 36, bH); x += 38;
    btnMed  .setBounds(x, bY, 36, bH); x += 38;
    btnFast .setBounds(x, bY, 36, bH); x += 40;
    x += 6;  // separator gap

    // SLOPE (3 buttons)
    btnSlope0  .setBounds(x, bY, 54, bH); x += 56;
    btnSlope3  .setBounds(x, bY, 60, bH); x += 62;
    btnSlope45 .setBounds(x, bY, 44, bH); x += 48;
    x += 6;  // separator gap

    // FLOOR (3 buttons)
    btnFloor60 .setBounds(x, bY, 50, bH); x += 52;
    btnFloor80 .setBounds(x, bY, 50, bH); x += 52;
    btnFloor100.setBounds(x, bY, 56, bH);

    MOONBASE_RESIZE_ACTIVATION_UI;
}
