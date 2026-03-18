#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SiloShaders.h"
#include "../../shared/QuilioSplash.h"

using namespace juce::gl;

// ============================================================
//  Colormap builder  (cold palette: black→navy→blue→teal→cyan→white)
// ============================================================
namespace SiloColormap
{
    struct RGB { uint8_t r, g, b; };
    inline void build(std::array<RGB, 1024>& out)
    {
        struct Stop { float p; uint8_t r,g,b; };
        static constexpr Stop s[] = {
            {0.00f,  0,  0,  4}, {0.10f,  3,  3, 28}, {0.22f,  6, 14, 82},
            {0.36f,  2, 55,138}, {0.50f,  0,118,168}, {0.64f,  0,178,198},
            {0.76f, 18,212,218}, {0.88f,118,236,236}, {1.00f,222,255,255},
        };
        for (int i = 0; i < 1024; ++i)
        {
            float t = i / 1023.f;
            for (int k = 0; k < 8; ++k)
                if (t <= s[k+1].p) {
                    float u = (t-s[k].p)/(s[k+1].p-s[k].p);
                    out[i] = {(uint8_t)(s[k].r+u*(s[k+1].r-s[k].r)),
                              (uint8_t)(s[k].g+u*(s[k+1].g-s[k].g)),
                              (uint8_t)(s[k].b+u*(s[k+1].b-s[k].b))};
                    break;
                }
        }
    }
}

// ============================================================
//  GL helpers
// ============================================================
namespace SiloGL
{
    inline void buildQuad(GLuint& vao, GLuint& vbo, GLuint& ebo,
                          juce::OpenGLContext& c)
    {
        static const float  v[] = {-1,-1, 1,-1, 1,1, -1,1};
        static const unsigned idx[] = {0,1,2, 0,2,3};
        c.extensions.glGenVertexArrays(1,&vao);
        c.extensions.glBindVertexArray(vao);
        c.extensions.glGenBuffers(1,&vbo);
        c.extensions.glBindBuffer(GL_ARRAY_BUFFER,vbo);
        c.extensions.glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
        c.extensions.glGenBuffers(1,&ebo);
        c.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
        c.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
        c.extensions.glEnableVertexAttribArray(0);
        c.extensions.glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,8,nullptr);
        c.extensions.glBindVertexArray(0);
    }
    inline void deleteQuad(GLuint& vao,GLuint& vbo,GLuint& ebo,juce::OpenGLContext& c)
    {
        c.extensions.glDeleteVertexArrays(1,&vao); vao=0;
        c.extensions.glDeleteBuffers(1,&vbo); vbo=0;
        c.extensions.glDeleteBuffers(1,&ebo); ebo=0;
    }
    inline void drawQuad(GLuint vao, juce::OpenGLContext& c)
    {
        c.extensions.glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
        c.extensions.glBindVertexArray(0);
    }
    inline GLuint makeTex(GLenum iFmt, int w, int h,
                          GLenum fmt, GLenum type, const void* d=nullptr)
    {
        GLuint t; glGenTextures(1,&t);
        glBindTexture(GL_TEXTURE_2D,t);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,(GLint)iFmt,w,h,0,fmt,type,d);
        glBindTexture(GL_TEXTURE_2D,0);
        return t;
    }
}

// ============================================================
//  Note name helpers
// ============================================================
namespace SiloNote
{
    inline juce::String midiToName(int midi)
    {
        static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int oct = midi/12 - 1;
        return juce::String(names[midi%12]) + juce::String(oct);
    }
    inline float freqToY(float f, float H)
    {
        float t = (std::log2(juce::jlimit(20.f,20000.f,f))-std::log2(20.f))
                / (std::log2(20000.f)-std::log2(20.f));
        return (1.f-t)*H;
    }
    // Musical note frequencies: C notes + A440 reference
    static const float kNoteFreqs[] = {
        32.7f,65.4f,130.8f,261.6f,523.3f,1046.5f,2093.f,4186.f,8372.f  // C1-C9
    };
}

// ============================================================
//  SpectrogramGL
// ============================================================
class SpectrogramGL : public juce::Component, public juce::OpenGLRenderer
{
public:
    static constexpr int kMaxCols = 1500;

    explicit SpectrogramGL(SiloProcessor& p) : proc(p)
    {
        ctx.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
        ctx.setRenderer(this);
        ctx.setComponentPaintingEnabled(true);
        ctx.setContinuousRepainting(false);
        ctx.attachTo(*this);
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }
    ~SpectrogramGL() override { ctx.detach(); }

    void setBias(float b) noexcept { bias_.store(b, std::memory_order_relaxed); }

    // mid01[numBins], width01[numBins]
    void pushColumn(const float* mid01, const float* width01, int nb, double sr)
    {
        int wc = writeCol.load(std::memory_order_relaxed);
        for (int b = 0; b < std::min(nb, SiloProcessor::numBins); ++b)
        {
            pending[b*2  ] = mid01  [b];
            pending[b*2+1] = width01[b];
        }
        pendingNB  = nb;
        pendingSR  = (float)sr;
        pendingCol = wc;
        colDirty.store(true, std::memory_order_release);
        writeCol.store((wc+1)%kMaxCols, std::memory_order_relaxed);
        validCols.store(std::min(validCols.load()+1, kMaxCols), std::memory_order_relaxed);
        ctx.triggerRepaint();
    }

    // OpenGLRenderer
    void newOpenGLContextCreated() override
    {
        shader = std::make_unique<juce::OpenGLShaderProgram>(ctx);
        if (!shader->addVertexShader  (SiloShaders::kQuadVert)       ||
            !shader->addFragmentShader(SiloShaders::kSpectrogramFrag) ||
            !shader->link())
        { DBG("Spectrogram shader: " << shader->getLastError()); shader.reset(); return; }

        SiloGL::buildQuad(vao, vbo, ebo, ctx);

        // RG32F: .r=mid, .g=stereoWidth
        dataTex = SiloGL::makeTex(GL_RG32F, kMaxCols, SiloProcessor::numBins,
                                  GL_RG, GL_FLOAT);
        glBindTexture(GL_TEXTURE_2D, dataTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::array<SiloColormap::RGB,1024> cm; SiloColormap::build(cm);
        cmapTex = SiloGL::makeTex(GL_RGB8, 1024, 1, GL_RGB, GL_UNSIGNED_BYTE, cm.data());
    }

    void renderOpenGL() override
    {
        if (!shader) return;
        if (colDirty.exchange(false, std::memory_order_acq_rel))
        {
            glBindTexture(GL_TEXTURE_2D, dataTex);
            // pending[] is interleaved [mid0,width0, mid1,width1 ...]
            glTexSubImage2D(GL_TEXTURE_2D, 0, pendingCol, 0, 1, pendingNB,
                            GL_RG, GL_FLOAT, pending.data());
            glBindTexture(GL_TEXTURE_2D, 0);
            latestSR = pendingSR;
        }

        juce::OpenGLHelpers::clear(juce::Colour(0xff030306));
        glViewport(0, 0, getWidth(), getHeight());

        shader->use();
        ctx.extensions.glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, dataTex);
        shader->setUniform("imageData", 0);

        ctx.extensions.glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cmapTex);
        shader->setUniform("colourMapTex", 1);

        shader->setUniform("resolution",   (float)getWidth(), (float)getHeight());
        shader->setUniform("numBins",      SiloProcessor::numBins);
        shader->setUniform("startIndex",   writeCol.load());
        shader->setUniform("validColumns", validCols.load());
        shader->setUniform("minFreq",      20.f);
        shader->setUniform("maxFreq",      20000.f);
        shader->setUniform("sampleRate",   latestSR > 0.f ? latestSR : 44100.f);
        shader->setUniform("bias",         bias_.load(std::memory_order_relaxed));
        shader->setUniform("curve",        0.28f);

        SiloGL::drawQuad(vao, ctx);
        ctx.extensions.glActiveTexture(GL_TEXTURE0);
    }

    void openGLContextClosing() override
    {
        shader.reset();
        SiloGL::deleteQuad(vao, vbo, ebo, ctx);
        glDeleteTextures(1, &dataTex);  dataTex=0;
        glDeleteTextures(1, &cmapTex);  cmapTex=0;
    }

    void paint(juce::Graphics& g) override
    {
        int W = getWidth(), H = getHeight();
        drawFreqGrid(g, W, H);
        drawNoteGrid(g, W, H);
        drawCentroidLine(g, W, H);
        drawPitchOverlay(g, W, H);
        if (mouseInside) drawCursorInfo(g, W, H);
    }

    void resized() override {}

    // Use ctx.triggerRepaint() — NOT repaint() — so overlay always goes
    // through the GL pipeline and gets composited correctly each frame.
    void mouseMove (const juce::MouseEvent& e) override
        { mousePos = e.getPosition(); mouseInside = true;  ctx.triggerRepaint(); }
    void mouseEnter(const juce::MouseEvent& e) override
        { mousePos = e.getPosition(); mouseInside = true;  ctx.triggerRepaint(); }
    void mouseExit (const juce::MouseEvent&)   override
        { mouseInside = false; ctx.triggerRepaint(); }

private:
    SiloProcessor& proc;
    juce::OpenGLContext ctx;

    GLuint vao=0,vbo=0,ebo=0, dataTex=0, cmapTex=0;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;

    std::atomic<int>  writeCol{0}, validCols{0};
    std::array<float, SiloProcessor::numBins * 2> pending{};
    std::atomic<bool> colDirty{false};
    int   pendingCol=0, pendingNB=SiloProcessor::numBins;
    float pendingSR=44100.f, latestSR=44100.f;

    std::atomic<float> bias_{0.09f};

    juce::Point<int> mousePos;
    bool mouseInside = false;

    void drawFreqGrid(juce::Graphics& g, int W, int H)
    {
        static const float kF[] = {
            30,40,50,60,80,100,200,300,400,500,600,800,
            1000,2000,3000,4000,5000,6000,8000,10000,20000
        };
        g.setColour(juce::Colour(0xff1a2540).withAlpha(0.40f));
        for (float f : kF) g.drawHorizontalLine((int)SiloNote::freqToY(f,H), 0.f, (float)(W-28));

        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.f)));
        g.setColour(juce::Colour(0xff6a8ca0));
        for (float f : kF)
        {
            int y = (int)SiloNote::freqToY(f,H);
            juce::String lbl = f>=1000.f ? juce::String((int)(f/1000.f))+"k"
                                         : juce::String((int)f);
            g.drawText(lbl, W-27, y-6, 25, 12, juce::Justification::centredRight, false);
        }
    }

    // C-note musical grid — faint dashed lines only, no text labels
    void drawNoteGrid(juce::Graphics& g, int W, int H)
    {
        // Only C2–C7 (musically useful range)
        static const float cNotes[] = {65.4f, 130.8f, 261.6f, 523.3f, 1046.5f, 2093.f};
        static const char* cNames[] = {"C2",  "C3",   "C4",   "C5",   "C6",    "C7"  };
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.f)));
        for (int i = 0; i < 6; ++i)
        {
            int y = (int)SiloNote::freqToY(cNotes[i], H);
            // Very faint amber line
            g.setColour(juce::Colour(0xffff9040).withAlpha(0.12f));
            g.drawHorizontalLine(y, 0.f, (float)(W - 28));
            // Tiny label at far left only
            g.setColour(juce::Colour(0xffff9040).withAlpha(0.38f));
            g.drawText(cNames[i], 2, y-5, 20, 10, juce::Justification::left, false);
        }
    }

    // Spectral centroid — single thin line + compact label
    void drawCentroidLine(juce::Graphics& g, int W, int H)
    {
        float hz = proc.spectralCentroid.load();
        if (hz < 20.f || hz > 20000.f) return;
        int y = (int)SiloNote::freqToY(hz, H);

        g.setColour(juce::Colour(0xff30d8c0).withAlpha(0.55f));
        g.drawHorizontalLine(y, 0.f, (float)(W - 28));

        // Label tucked right of grid labels
        juce::String lbl = hz >= 1000.f ? juce::String(hz/1000.f,1)+"k" : juce::String((int)hz);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f)));
        g.setColour(juce::Colour(0xff30d8c0).withAlpha(0.70f));
        g.drawText("~"+lbl, W-27, y-5, 25, 10, juce::Justification::centredRight, false);
    }

    // Pitch detection — single thin line + one badge in bottom-left corner
    void drawPitchOverlay(juce::Graphics& g, int W, int H)
    {
        int   midi  = proc.detectedMidiNote.load();
        float pitch = proc.detectedPitch.load();
        float cents = proc.detectedCents.load();
        if (midi < 0 || pitch < 20.f) return;

        int y = (int)SiloNote::freqToY(pitch, H);
        g.setColour(juce::Colour(0xffa0f030).withAlpha(0.65f));
        g.drawHorizontalLine(y, 0.f, (float)(W - 28));

        // Single badge fixed in top-left so it never overlaps other labels
        juce::String note = SiloNote::midiToName(midi);
        juce::String cstr = std::abs(cents) > 4.f
            ? (cents > 0 ? "+" : "") + juce::String((int)cents) + juce::String(L"\u00a2") : "";
        juce::String lbl  = note + " " + cstr;

        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.f).withStyle("Bold")));
        g.setColour(juce::Colour(0xff050e05).withAlpha(0.75f));
        g.fillRoundedRectangle(4, 4, 58, 14, 2.f);
        g.setColour(juce::Colour(0xffa8ff38).withAlpha(0.92f));
        g.drawText(lbl, 6, 4, 54, 14, juce::Justification::centredLeft, false);
    }

    // Mouse cursor — small pill label near cursor, no crosshair lines
    void drawCursorInfo(juce::Graphics& g, int W, int H)
    {
        float mx = (float)mousePos.x, my = (float)mousePos.y;
        if (mx < 0 || mx >= W || my < 0 || my >= H) return;

        // Thin crosshair at cursor position only
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.18f));
        g.drawVerticalLine  ((int)mx, 0.f, (float)H);
        g.drawHorizontalLine((int)my, 0.f, (float)(W-28));

        // Compute frequency at cursor Y
        float t    = 1.f - my / (float)H;
        float freq = std::pow(2.f, std::log2(20.f) + t*(std::log2(20000.f)-std::log2(20.f)));

        float exactMidi  = 69.f + 12.f * std::log2(freq / 440.f);
        int   roundMidi  = juce::jlimit(0, 127, (int)std::round(exactMidi));
        float cts        = (exactMidi - (float)roundMidi) * 100.f;

        juce::String freqStr = freq >= 1000.f
            ? juce::String(freq/1000.f, 2) + "k" : juce::String((int)freq) + "Hz";
        juce::String note    = SiloNote::midiToName(roundMidi);
        juce::String cStr    = std::abs(cts) > 3.f
            ? (cts>0?"+":"") + juce::String((int)cts) + juce::String(L"\u00a2") : "";

        juce::String info = freqStr + "  " + note + cStr;

        float bW = 118.f, bH = 15.f;
        // Keep pill inside bounds, offset from cursor
        float bx = juce::jlimit(2.f, (float)W - bW - 30.f, mx + 8.f);
        float by = juce::jlimit(2.f, (float)H - bH - 2.f,  my - bH - 4.f);

        g.setColour(juce::Colour(0xff060d18).withAlpha(0.85f));
        g.fillRoundedRectangle(bx, by, bW, bH, 3.f);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)));
        g.setColour(juce::Colour(0xff50c8e0).withAlpha(0.95f));
        g.drawText(info, (int)bx+4, (int)by, (int)bW-8, (int)bH,
                   juce::Justification::centredLeft, false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramGL)
};

// ============================================================
//  AnalyserGL
// ============================================================
class AnalyserGL : public juce::Component, public juce::OpenGLRenderer
{
public:
    explicit AnalyserGL(SiloProcessor& p) : proc(p)
    {
        ctx.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
        ctx.setRenderer(this); ctx.setComponentPaintingEnabled(true);
        ctx.setContinuousRepainting(false); ctx.attachTo(*this);
    }
    ~AnalyserGL() override { ctx.detach(); }

    void setSlope(float s) noexcept { slope_.store(s, std::memory_order_relaxed); ctx.triggerRepaint(); }

    void updateData(const float* norm01, int nb, double sr)
    {
        constexpr float kA=0.88f, kR=0.9965f, kP=0.9993f;
        if ((int)ampBuf.size()!=nb){ ampBuf.assign(nb,0.f); ribBuf.assign(nb,0.f); }
        for (int i=0; i<nb; ++i) {
            float v=norm01[i], &a=ampBuf[i];
            a = v>a ? kA*a+(1-kA)*v : kR*a+(1-kR)*v;
            float& r=ribBuf[i];
            if (v>r) r=v; else r*=kP;
        }
        nb_=nb; sr_=(float)sr;
        dirty.store(true, std::memory_order_release);
        ctx.triggerRepaint();
    }

    void newOpenGLContextCreated() override
    {
        shader=std::make_unique<juce::OpenGLShaderProgram>(ctx);
        if (!shader->addVertexShader(SiloShaders::kQuadVert)||
            !shader->addFragmentShader(SiloShaders::kAnalyserFrag)||!shader->link())
        { DBG("Analyser: "<<shader->getLastError()); shader.reset(); return; }
        SiloGL::buildQuad(vao,vbo,ebo,ctx);
        ampTex=SiloGL::makeTex(GL_R32F,SiloProcessor::numBins,1,GL_RED,GL_FLOAT);
        ribTex=SiloGL::makeTex(GL_R32F,SiloProcessor::numBins,1,GL_RED,GL_FLOAT);
        std::array<SiloColormap::RGB,1024> cm; SiloColormap::build(cm);
        cmapTex=SiloGL::makeTex(GL_RGB8,1024,1,GL_RGB,GL_UNSIGNED_BYTE,cm.data());
    }

    void renderOpenGL() override
    {
        if (!shader) return;
        if (dirty.exchange(false,std::memory_order_acq_rel) && !ampBuf.empty()) {
            glBindTexture(GL_TEXTURE_2D,ampTex);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,(int)ampBuf.size(),1,GL_RED,GL_FLOAT,ampBuf.data());
            glBindTexture(GL_TEXTURE_2D,ribTex);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,(int)ribBuf.size(),1,GL_RED,GL_FLOAT,ribBuf.data());
            glBindTexture(GL_TEXTURE_2D,0);
        }
        juce::OpenGLHelpers::clear(juce::Colour(0xff050510));
        glViewport(0,0,getWidth(),getHeight());
        shader->use();
        ctx.extensions.glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,ampTex);  shader->setUniform("ampData",0);
        ctx.extensions.glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,ribTex);  shader->setUniform("ribbonData",1);
        ctx.extensions.glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,cmapTex); shader->setUniform("colourMapTex",2);
        shader->setUniform("resolution",(float)getWidth(),(float)getHeight());
        shader->setUniform("numBins",   nb_>0?nb_:SiloProcessor::numBins);
        shader->setUniform("minFreq",20.f); shader->setUniform("maxFreq",20000.f);
        shader->setUniform("sampleRate",sr_>0?sr_:44100.f);
        shader->setUniform("slope",      slope_.load(std::memory_order_relaxed));
        SiloGL::drawQuad(vao,ctx);
        ctx.extensions.glActiveTexture(GL_TEXTURE0);
    }

    void openGLContextClosing() override
    {
        shader.reset(); SiloGL::deleteQuad(vao,vbo,ebo,ctx);
        glDeleteTextures(1,&ampTex); ampTex=0;
        glDeleteTextures(1,&ribTex); ribTex=0;
        glDeleteTextures(1,&cmapTex); cmapTex=0;
    }

    void paint(juce::Graphics& g) override
    {
        int W=getWidth(), H=getHeight();
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.f)));
        g.setColour(juce::Colour(0xff506070));
        for (int db : {-70,-60,-50,-40,-30,-20,-10}) {
            float norm=(db+80.f)/80.f;
            float x=(1.f-norm)*W;
            g.drawText(juce::String(db),(int)x-12,H-12,24,11,juce::Justification::centred,false);
        }
    }
    void resized() override {}

private:
    SiloProcessor& proc;
    juce::OpenGLContext ctx;
    GLuint vao=0,vbo=0,ebo=0, ampTex=0,ribTex=0,cmapTex=0;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::vector<float> ampBuf, ribBuf;
    int nb_=0; float sr_=44100.f;
    std::atomic<bool>  dirty{false};
    std::atomic<float> slope_{0.f};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyserGL)
};

// ============================================================
//  OscilloscopeGL
// ============================================================
class OscilloscopeGL : public juce::Component, public juce::OpenGLRenderer
{
public:
    explicit OscilloscopeGL(SiloProcessor& p) : proc(p)
    {
        ctx.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
        ctx.setRenderer(this); ctx.setComponentPaintingEnabled(false);
        ctx.setContinuousRepainting(false); ctx.attachTo(*this);
    }
    ~OscilloscopeGL() override { ctx.detach(); }
    void triggerRepaint() { ctx.triggerRepaint(); }

    void newOpenGLContextCreated() override
    {
        shader=std::make_unique<juce::OpenGLShaderProgram>(ctx);
        if (!shader->addVertexShader(SiloShaders::kQuadVert)||
            !shader->addFragmentShader(SiloShaders::kOscFrag)||!shader->link())
        { DBG("Osc: "<<shader->getLastError()); shader.reset(); return; }
        SiloGL::buildQuad(vao,vbo,ebo,ctx);
    }

    void renderOpenGL() override
    {
        if (!shader) return;
        int W=getWidth(), H=getHeight();
        if (W<=0||H<=0) return;

        if (W!=prevW) {
            if (waveTex) { glDeleteTextures(1,&waveTex); waveTex=0; }
            waveTex=SiloGL::makeTex(GL_RG32F,W,2,GL_RG,GL_FLOAT);
            glBindTexture(GL_TEXTURE_2D,waveTex);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D,0);
            prevW=W;
        }

        int ring=SiloProcessor::waveRingSize;
        int tot =std::min(ring,(int)proc.currentSampleRate.load());
        int spp =std::max(1,tot/W);
        int wPos=proc.waveWritePos.load(std::memory_order_acquire);
        int si  =(wPos-tot+ring)&(ring-1);

        std::vector<float> rL(W*2), rR(W*2);
        for (int px=0; px<W; ++px) {
            float mnL=1,mxL=-1,mnR=1,mxR=-1;
            for (int s=0; s<spp; ++s) {
                int idx=(si+px*spp+s)&(ring-1);
                float l=proc.waveL[idx], r=proc.waveR[idx];
                if(l<mnL)mnL=l; if(l>mxL)mxL=l;
                if(r<mnR)mnR=r; if(r>mxR)mxR=r;
            }
            rL[px*2]=mnL; rL[px*2+1]=mxL;
            rR[px*2]=mnR; rR[px*2+1]=mxR;
        }
        glBindTexture(GL_TEXTURE_2D,waveTex);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,W,1,GL_RG,GL_FLOAT,rL.data());
        glTexSubImage2D(GL_TEXTURE_2D,0,0,1,W,1,GL_RG,GL_FLOAT,rR.data());
        glBindTexture(GL_TEXTURE_2D,0);

        juce::OpenGLHelpers::clear(juce::Colour(0xff040410));
        glViewport(0,0,W,H);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        shader->use();
        ctx.extensions.glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,waveTex);
        shader->setUniform("waveData",0);
        shader->setUniform("resolution",(float)W,(float)H);
        SiloGL::drawQuad(vao,ctx);
    }

    void openGLContextClosing() override
    {
        shader.reset(); SiloGL::deleteQuad(vao,vbo,ebo,ctx);
        if (waveTex){glDeleteTextures(1,&waveTex);waveTex=0;}
    }
    void resized() override {}

private:
    SiloProcessor& proc;
    juce::OpenGLContext ctx;
    GLuint vao=0,vbo=0,ebo=0, waveTex=0;
    int prevW=0;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeGL)
};

// ============================================================
//  LissajousGL
// ============================================================
class LissajousGL : public juce::Component, public juce::OpenGLRenderer
{
public:
    explicit LissajousGL(SiloProcessor& p) : proc(p)
    {
        ctx.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
        ctx.setRenderer(this); ctx.setComponentPaintingEnabled(false);
        ctx.setContinuousRepainting(false); ctx.attachTo(*this);
    }
    ~LissajousGL() override { ctx.detach(); }
    void triggerRepaint() { ctx.triggerRepaint(); }

    void newOpenGLContextCreated() override
    {
        lissShader=std::make_unique<juce::OpenGLShaderProgram>(ctx);
        if (!lissShader->addVertexShader(SiloShaders::kLissVert)||
            !lissShader->addFragmentShader(SiloShaders::kLissFrag)||!lissShader->link())
        { lissShader.reset(); }

        gridShader=std::make_unique<juce::OpenGLShaderProgram>(ctx);
        if (!gridShader->addVertexShader(SiloShaders::kLineVert)||
            !gridShader->addFragmentShader(SiloShaders::kLineFrag)||!gridShader->link())
        { gridShader.reset(); }

        ctx.extensions.glGenBuffers(1,&lissVBO);
        buildGrid();
    }

    void renderOpenGL() override
    {
        juce::OpenGLHelpers::clear(juce::Colour(0xff030308));
        glViewport(0,0,getWidth(),getHeight());
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        drawGrid(); drawTrace();
    }

    void openGLContextClosing() override
    {
        lissShader.reset(); gridShader.reset();
        if (lissVBO){ctx.extensions.glDeleteBuffers(1,&lissVBO);lissVBO=0;}
        if (gridVAO){ctx.extensions.glDeleteVertexArrays(1,&gridVAO);gridVAO=0;}
        if (gridVBO){ctx.extensions.glDeleteBuffers(1,&gridVBO);gridVBO=0;}
    }
    void resized() override {}

private:
    SiloProcessor& proc;
    juce::OpenGLContext ctx;
    std::unique_ptr<juce::OpenGLShaderProgram> lissShader, gridShader;
    GLuint lissVBO=0, gridVAO=0, gridVBO=0;
    int gridN=0;
    struct Vtx { float l,r,age; };

    void buildGrid()
    {
        static const float lines[] = {
            -0.9f,0, 0.9f,0,   0,-0.9f, 0,0.9f,
            -0.9f,-0.9f, 0.9f,0.9f,  -0.9f,0.9f, 0.9f,-0.9f,
             0,-0.9f, 0.9f,0,  0.9f,0, 0,0.9f,
             0,0.9f,-0.9f,0,  -0.9f,0, 0,-0.9f,
        };
        gridN=(int)(sizeof(lines)/(2*sizeof(float)));
        ctx.extensions.glGenVertexArrays(1,&gridVAO);
        ctx.extensions.glBindVertexArray(gridVAO);
        ctx.extensions.glGenBuffers(1,&gridVBO);
        ctx.extensions.glBindBuffer(GL_ARRAY_BUFFER,gridVBO);
        ctx.extensions.glBufferData(GL_ARRAY_BUFFER,sizeof(lines),lines,GL_STATIC_DRAW);
        ctx.extensions.glEnableVertexAttribArray(0);
        ctx.extensions.glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,8,nullptr);
        ctx.extensions.glBindVertexArray(0);
    }

    void drawGrid()
    {
        if (!gridShader||!gridVAO) return;
        gridShader->use();
        gridShader->setUniform("lineColor",0.08f,0.13f,0.22f,1.f);
        ctx.extensions.glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES,0,gridN);
        ctx.extensions.glBindVertexArray(0);
    }

    void drawTrace()
    {
        if (!lissShader) return;
        int ring=SiloProcessor::lissSize;
        int wPos=proc.lissWritePos.load(std::memory_order_acquire);
        std::vector<Vtx> v; v.reserve(ring);
        for (int i=0;i<ring;++i) {
            int idx=(wPos-ring+i+ring)&(ring-1);
            v.push_back({proc.lissL[idx],proc.lissR[idx],(float)i/(ring-1)});
        }
        ctx.extensions.glBindBuffer(GL_ARRAY_BUFFER,lissVBO);
        ctx.extensions.glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(v.size()*sizeof(Vtx)),
                                    v.data(),GL_DYNAMIC_DRAW);
        lissShader->use();
        ctx.extensions.glEnableVertexAttribArray(0);
        ctx.extensions.glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)0);
        ctx.extensions.glEnableVertexAttribArray(1);
        ctx.extensions.glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)(2*sizeof(float)));
        glDrawArrays(GL_LINE_STRIP,0,(GLsizei)v.size());
        ctx.extensions.glDisableVertexAttribArray(0);
        ctx.extensions.glDisableVertexAttribArray(1);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LissajousGL)
};

// ============================================================
//  LevelMeterView  (CPU)
// ============================================================
class LevelMeterView : public juce::Component
{
public:
    explicit LevelMeterView(SiloProcessor& p) : proc(p) {}
    void paint(juce::Graphics& g) override
    {
        int W = getWidth(), H = getHeight();
        g.fillAll(juce::Colour(0xff040410));

        // Layout: 2px outer pad, 1px gap between bars
        int pad = 2;
        int gap = 1;
        int bW  = (W - pad*2 - gap) / 2;
        int bX0 = pad;
        int bX1 = pad + bW + gap;
        int bTop = 14;      // leave room for L/R labels
        int bH   = H - bTop - 2;

        // dB tick lines (drawn first, behind bars)
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.f)));
        for (float db : { 0.f, -12.f, -24.f, -36.f, -48.f, -60.f })
        {
            int ty = bTop + (int)(juce::jmap(db, -60.f, 0.f, (float)bH, 0.f));
            g.setColour(juce::Colour(0xff0e1828));
            g.drawHorizontalLine(ty, (float)bX0, (float)(bX1 + bW));
        }

        // Draw one bar
        auto drawBar = [&](float rms, float peak, int x)
        {
            float rN = juce::jlimit(0.f, 1.f, juce::jmap(
                20.f * std::log10(std::max(rms, 1e-6f)), -60.f, 0.f, 0.f, 1.f));
            float pN = juce::jlimit(0.f, 1.f, juce::jmap(
                20.f * std::log10(std::max(peak, 1e-6f)), -60.f, 0.f, 0.f, 1.f));

            // Track bg
            g.setColour(juce::Colour(0xff08091c));
            g.fillRect(x, bTop, bW, bH);

            // RMS fill
            int fillH = (int)(rN * bH);
            if (fillH > 0)
            {
                juce::ColourGradient gr(juce::Colour(0xff00c0b4), (float)x, (float)(bTop + bH),
                                        juce::Colour(0xffddff30), (float)x, (float)bTop, false);
                gr.addColour(0.80, juce::Colour(0xffff3030));
                g.setGradientFill(gr);
                g.fillRect(x, bTop + bH - fillH, bW, fillH);
            }

            // Peak hold tick
            if (pN > 0.005f)
            {
                int py = bTop + (int)((1.f - pN) * bH);
                g.setColour(juce::Colour(0xffffffff).withAlpha(0.80f));
                g.fillRect(x, py, bW, 1);
            }
        };

        drawBar(proc.rmsL.load(), proc.peakL.load(), bX0);
        drawBar(proc.rmsR.load(), proc.peakR.load(), bX1);

        // Channel labels
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.f).withStyle("Bold")));
        g.setColour(juce::Colour(0xff2e4860));
        g.drawText("L", bX0, 2, bW, 11, juce::Justification::centred, false);
        g.drawText("R", bX1, 2, bW, 11, juce::Justification::centred, false);

        // 0 dB clipping indicator
        if (proc.peakL.load() >= 1.f || proc.peakR.load() >= 1.f)
        {
            g.setColour(juce::Colour(0xffff2020).withAlpha(0.85f));
            g.fillRect(pad, bTop, W - pad*2, 2);
        }
    }
private:
    SiloProcessor& proc;
};

// ============================================================
//  CorrelationMeterView
// ============================================================
class CorrelationMeterView : public juce::Component
{
public:
    explicit CorrelationMeterView(SiloProcessor& p) : proc(p) {}

    void paint(juce::Graphics& g) override
    {
        int W = getWidth(), H = getHeight();
        g.fillAll(juce::Colour(0xff040410));

        // ── Compute phase correlation from ring buffer ──────────────
        {
            int ring = SiloProcessor::waveRingSize;
            int n    = std::min(4096, ring);
            int pos  = proc.waveWritePos.load(std::memory_order_acquire);
            float sLR = 0.f, sLL = 0.f, sRR = 0.f;
            for (int i = 0; i < n; ++i) {
                int idx = (pos - n + i + ring) & (ring - 1);
                float l = proc.waveL[idx], r = proc.waveR[idx];
                sLR += l * r;  sLL += l * l;  sRR += r * r;
            }
            float denom = std::sqrt(sLL * sRR);
            corr_ = denom > 1e-8f ? sLR / denom : 0.f;
        }

        // ── Compute avg stereo width from FFT bins ──────────────────
        {
            int wi = proc.fftReadIdx.load(std::memory_order_acquire);
            float sum = 0.f;
            for (int b = 0; b < SiloProcessor::numBins; ++b)
                sum += proc.fftWidth[wi][b];
            width_ = sum / (float)SiloProcessor::numBins;
        }

        int pad = 6;
        int bW  = W - pad * 2;

        // ── Section: PHASE CORRELATION ──────────────────────────────
        int secY = 8;

        // Label row
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f).withStyle("Bold")));
        g.setColour(juce::Colour(0xff2a3e58));
        g.drawText("PHASE", pad, secY, 36, 10, juce::Justification::left, false);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f)));
        g.setColour(juce::Colour(0xff3a6080));
        g.drawText(juce::String(corr_, 2), pad + 36, secY, bW - 36, 10,
                   juce::Justification::right, false);

        // Bar
        int barY = secY + 12, barH = 12;
        g.setColour(juce::Colour(0xff060a14));
        g.fillRect(pad, barY, bW, barH);

        float cx = pad + bW / 2.f;
        float normC = (corr_ + 1.f) * 0.5f;
        int   fillX = pad + (int)(normC * bW);

        if (corr_ >= 0.f) {
            juce::ColourGradient gr(juce::Colour(0xff18a060), cx, 0.f,
                                    juce::Colour(0xff08d090), (float)(pad + bW), 0.f, false);
            g.setGradientFill(gr);
            g.fillRect((int)cx, barY, fillX - (int)cx, barH);
        } else {
            juce::ColourGradient gr(juce::Colour(0xffb02010), (float)pad, 0.f,
                                    juce::Colour(0xffe06820), cx, 0.f, false);
            g.setGradientFill(gr);
            g.fillRect(fillX, barY, (int)cx - fillX, barH);
        }

        // Center mark
        g.setColour(juce::Colour(0xff182038));
        g.drawVerticalLine((int)cx, (float)barY, (float)(barY + barH));

        // Axis labels
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.f)));
        g.setColour(juce::Colour(0xff1e3048));
        g.drawText("-1", pad, barY + barH + 2, 14, 9, juce::Justification::left, false);
        g.drawText("0",  (int)cx - 4, barY + barH + 2, 10, 9, juce::Justification::centred, false);
        g.drawText("+1", pad + bW - 13, barY + barH + 2, 14, 9, juce::Justification::right, false);

        // ── Section: STEREO WIDTH ────────────────────────────────────
        secY = barY + barH + 18;

        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f).withStyle("Bold")));
        g.setColour(juce::Colour(0xff2a3e58));
        g.drawText("WIDTH", pad, secY, 38, 10, juce::Justification::left, false);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f)));
        g.setColour(juce::Colour(0xff2a80b0));
        g.drawText(juce::String((int)(width_ * 100)) + "%",
                   pad + 38, secY, bW - 38, 10, juce::Justification::right, false);

        int wBarY = secY + 12, wBarH = 10;
        g.setColour(juce::Colour(0xff060a14));
        g.fillRect(pad, wBarY, bW, wBarH);

        juce::ColourGradient wgr(juce::Colour(0xff004488), (float)pad, 0.f,
                                  juce::Colour(0xff00b0d0), (float)(pad + bW), 0.f, false);
        g.setGradientFill(wgr);
        g.fillRect(pad, wBarY, (int)(width_ * bW), wBarH);

        // ── Section: RMS / PEAK readout ──────────────────────────────
        int readY = wBarY + wBarH + 12;
        if (readY + 30 <= H)
        {
            auto dbStr = [](float v) -> juce::String {
                float db = 20.f * std::log10(std::max(v, 1e-6f));
                return db > -0.1f ? "  0.0" : juce::String(db, 1);
            };

            float rmsL  = proc.rmsL .load();
            float rmsR  = proc.rmsR .load();
            float pkL   = proc.peakL.load();
            float pkR   = proc.peakR.load();

            g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f).withStyle("Bold")));
            g.setColour(juce::Colour(0xff1e3048));
            g.drawText("RMS", pad, readY, 24, 10, juce::Justification::left, false);
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f)));
            g.setColour(juce::Colour(0xff2a6880));
            g.drawText("L " + dbStr(rmsL), pad, readY + 11, bW / 2, 10,
                       juce::Justification::left, false);
            g.drawText("R " + dbStr(rmsR), pad + bW / 2, readY + 11, bW / 2, 10,
                       juce::Justification::right, false);

            g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f).withStyle("Bold")));
            g.setColour(juce::Colour(0xff1e3048));
            g.drawText("PEAK", pad, readY + 22, 28, 10, juce::Justification::left, false);
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f)));
            bool clip = pkL >= 1.f || pkR >= 1.f;
            g.setColour(clip ? juce::Colour(0xffff3030) : juce::Colour(0xff2a6880));
            g.drawText("L " + dbStr(pkL), pad, readY + 33, bW / 2, 10,
                       juce::Justification::left, false);
            g.drawText("R " + dbStr(pkR), pad + bW / 2, readY + 33, bW / 2, 10,
                       juce::Justification::right, false);
        }
    }

private:
    SiloProcessor& proc;
    float corr_  = 0.f;
    float width_ = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorrelationMeterView)
};

// ============================================================
//  SiloLAF  — look & feel for control bar buttons
// ============================================================
class SiloLAF : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                              const juce::Colour&, bool hl, bool) override
    {
        auto r  = btn.getLocalBounds().toFloat().reduced(0.5f);
        bool on = btn.getToggleState();
        g.setColour(on ? juce::Colour(0xff122840) : (hl ? juce::Colour(0xff0a1020) : juce::Colour(0xff070910)));
        g.fillRoundedRectangle(r, 3.f);
        g.setColour(on ? juce::Colour(0xff1e4060) : juce::Colour(0xff101828));
        g.drawRoundedRectangle(r, 3.f, 1.f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool hl, bool) override
    {
        bool on = btn.getToggleState();
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f).withStyle(on ? "Bold" : "")));
        g.setColour(on  ? juce::Colour(0xff50a8d8)
                        : (hl ? juce::Colour(0xff304860) : juce::Colour(0xff1e3448)));
        g.drawText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, false);
    }
};

// ============================================================
//  SiloEditor
// ============================================================
class SiloEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SiloEditor(SiloProcessor&);
    ~SiloEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    SiloProcessor& proc;

    // ── Visualiser panels ──────────────────────────────────────────
    std::unique_ptr<SpectrogramGL>       spectrogram;
    std::unique_ptr<AnalyserGL>          analyser;
    std::unique_ptr<OscilloscopeGL>      oscilloscope;
    std::unique_ptr<LissajousGL>         lissajous;
    std::unique_ptr<LevelMeterView>      levelMeter;
    std::unique_ptr<CorrelationMeterView> corrMeter;

    // ── Custom LAF (must outlive all buttons) ──────────────────────
    SiloLAF siloLAF;

    // ── Controls bar buttons ───────────────────────────────────────
    juce::TextButton btnFreeze   { "FREEZE"    };  // toggle

    juce::TextButton btnSlow     { "0.5\xc3\x97" };  // radio group 1: speed
    juce::TextButton btnMed      { "1\xc3\x97"   };
    juce::TextButton btnFast     { "2\xc3\x97"   };

    juce::TextButton btnSlope0   { "0 dB/oct"  };  // radio group 2: spectral tilt
    juce::TextButton btnSlope3   { "+3 dB/oct" };
    juce::TextButton btnSlope45  { "+4.5"      };

    juce::TextButton btnFloor60  { "-60 dB"  };  // radio group 3: noise floor
    juce::TextButton btnFloor80  { "-80 dB"  };
    juce::TextButton btnFloor100 { "-100 dB" };

    // ── Controls state ─────────────────────────────────────────────
    bool freeze_    = false;
    int  speedMode_ = 1;    // 0 = 0.5×, 1 = 1×, 2 = 2×
    int  skipCount_ = 0;

    void timerCallback() override;

    QuilioSplash quilioSplash;
    QuilioSplashOverlay splashOverlay { quilioSplash };

    MOONBASE_DECLARE_AND_INIT_ACTIVATION_UI(proc);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SiloEditor)
};
