#pragma once

namespace SiloShaders
{
    const char* const kQuadVert = R"(
        attribute vec2 position;
        void main() { gl_Position = vec4(position, 0.0, 1.0); }
    )";

    // ---- Spectrogram ----
    // imageData: GL_RG32F [kMaxCols x numBins]
    //   .r = mid amplitude (0-1 normalised dB)
    //   .g = stereo width  (0=mono, 1=full side)
    // colourMapTex: GL_RGB8 [1024 x 1]  (cold palette)
    const char* const kSpectrogramFrag = R"(
        uniform vec2      resolution;
        uniform sampler2D imageData;
        uniform sampler2D colourMapTex;
        uniform int   numBins;
        uniform int   startIndex;
        uniform int   validColumns;
        uniform float minFreq;
        uniform float maxFreq;
        uniform float sampleRate;
        uniform float bias;
        uniform float curve;

        float freqFromNorm(float t) {
            return minFreq * pow(maxFreq / minFreq, t);
        }
        float sCurve(float x, float str) {
            float y = x * 2.0 - 1.0;
            float s = tanh(y * mix(0.0, 6.0, abs(str))) * 0.5 + 0.5;
            if (str < 0.0) s = 1.0 - s;
            return mix(x, s, abs(str));
        }

        void main() {
            vec2  uv  = gl_FragCoord.xy / resolution.xy;
            int   vc  = max(validColumns, 1);
            int   col0 = (startIndex + 1 + int(uv.x * float(vc))) % vc;
            int   col1 = (col0 + 1) % vc;

            float pixH = 1.0 / resolution.y;
            float midVal = 0.0, wdVal = 0.0, wsum = 0.0;

            for (int i = -1; i <= 1; i++) {
                float yy   = clamp(uv.y + float(i) * pixH, 0.0, 1.0);
                float w    = exp(-float(i*i) * 0.6);
                float f    = freqFromNorm(yy);
                float fsz  = float(2 * (numBins - 1));
                float binF = clamp(f * fsz / sampleRate, 0.0, float(numBins - 1));
                int   b0   = int(binF);
                int   b1   = min(b0 + 1, numBins - 1);
                float t    = fract(binF);

                vec2 s0 = mix(texelFetch(imageData, ivec2(col0, b0), 0).rg,
                              texelFetch(imageData, ivec2(col0, b1), 0).rg, t);
                vec2 s1 = mix(texelFetch(imageData, ivec2(col1, b0), 0).rg,
                              texelFetch(imageData, ivec2(col1, b1), 0).rg, t);
                vec2 sv = max(s0, s1);
                midVal += sv.r * w;
                wdVal  += sv.g * w;
                wsum   += w;
            }
            midVal /= max(wsum, 1e-6);
            wdVal  /= max(wsum, 1e-6);

            float cv = sCurve(midVal, curve);

            // Cold palette for mid content
            vec3 coldCol = texture2D(colourMapTex, vec2(clamp(cv, 0.0, 1.0), 0.5)).rgb;

            // Warm (amber/orange) palette for stereo/side content
            // Inline warm gradient: black -> dark-orange -> amber -> yellow
            vec3 warmCol;
            float cv2 = cv;
            if      (cv2 < 0.25) warmCol = mix(vec3(0.0,0.0,0.0),    vec3(0.18,0.04,0.0),   cv2/0.25);
            else if (cv2 < 0.55) warmCol = mix(vec3(0.18,0.04,0.0),  vec3(0.75,0.22,0.0),   (cv2-0.25)/0.30);
            else if (cv2 < 0.82) warmCol = mix(vec3(0.75,0.22,0.0),  vec3(1.0, 0.60,0.05),  (cv2-0.55)/0.27);
            else                 warmCol = mix(vec3(1.0, 0.60,0.05), vec3(1.0, 0.90,0.60),  (cv2-0.82)/0.18);

            // Blend: stereoWidth drives warm tint, scaled by amplitude so quiet is still dark
            vec3 colour = mix(coldCol, warmCol, wdVal * 0.75);

            if (cv < bias) colour *= 0.02;
            gl_FragColor = vec4(colour, 1.0);
        }
    )";

    // ---- Analyser (spectrum bar) ----
    const char* const kAnalyserFrag = R"(
        uniform vec2      resolution;
        uniform sampler2D ampData;
        uniform sampler2D ribbonData;
        uniform sampler2D colourMapTex;
        uniform int   numBins;
        uniform float minFreq;
        uniform float maxFreq;
        uniform float sampleRate;
        uniform float slope;   // spectral tilt dB/octave (e.g. 0, 3.0, 4.5)

        float freqFromNorm(float t) { return minFreq * pow(maxFreq / minFreq, t); }

        void main() {
            vec2  uv   = gl_FragCoord.xy / resolution.xy;
            float freq = freqFromNorm(uv.y);
            if (freq < minFreq || freq > maxFreq) discard;

            float fsz  = float(2 * (numBins - 1));
            float binF = clamp(freq * fsz / sampleRate, 0.0, float(numBins - 1));
            int   b0   = int(binF);
            int   b1   = min(b0 + 1, numBins - 1);
            float t    = fract(binF);

            float amp = mix(texelFetch(ampData,    ivec2(b0,0),0).r,
                            texelFetch(ampData,    ivec2(b1,0),0).r, t);
            float rib = mix(texelFetch(ribbonData, ivec2(b0,0),0).r,
                            texelFetch(ribbonData, ivec2(b1,0),0).r, t);

            // Spectral tilt: +dB/oct above 1kHz, -dB/oct below
            float tilt = slope * log2(max(freq, 20.0) / 1000.0) / 80.0;
            amp = clamp(amp + tilt, 0.0, 1.0);
            rib = clamp(rib + tilt, 0.0, 1.0);

            float fromRight = 1.0 - uv.x;
            bool  atPeak    = abs(fromRight - rib) < (2.5 / resolution.x) && rib > 0.005;

            if (fromRight > amp && !atPeak) discard;

            vec3 col = atPeak
                ? vec3(1.0, 0.85, 0.12)
                : texture2D(colourMapTex, vec2(amp, 0.5)).rgb;

            gl_FragColor = vec4(col, 1.0);
        }
    )";

    // ---- Oscilloscope ----
    const char* const kOscFrag = R"(
        uniform vec2      resolution;
        uniform sampler2D waveData;
        void main() {
            vec2 uv  = gl_FragCoord.xy / resolution.xy;
            int  col = int(uv.x * resolution.x);
            float amp = (uv.y - 0.5) * 2.0;
            vec2 mmL = texelFetch(waveData, ivec2(col, 0), 0).rg;
            vec2 mmR = texelFetch(waveData, ivec2(col, 1), 0).rg;
            bool inL = amp >= mmL.r && amp <= mmL.g;
            bool inR = amp >= mmR.r && amp <= mmR.g;
            if (!inL && !inR) discard;
            if (inL && inR)
                gl_FragColor = vec4(0.28, 0.85, 0.88, 0.92);
            else if (inL)
                gl_FragColor = vec4(0.0, 0.78, 0.84, 0.85);
            else
                gl_FragColor = vec4(0.42, 0.36, 0.82, 0.65);
        }
    )";

    // ---- Lissajous ----
    const char* const kLissVert = R"(
        attribute vec2  lr;
        attribute float age;
        varying   float vAge;
        void main() {
            float x = (lr.x + lr.y) * 0.5 * 0.92;
            float y = (lr.x - lr.y) * 0.5 * 0.92;
            gl_Position = vec4(x, y, 0.0, 1.0);
            vAge = age;
        }
    )";
    const char* const kLissFrag = R"(
        varying float vAge;
        void main() {
            float a2  = vAge * vAge;
            vec3  col = mix(vec3(0.0, 0.18, 0.32), vec3(0.45, 0.92, 0.95), vAge);
            gl_FragColor = vec4(col, a2 * 0.88);
        }
    )";

    const char* const kLineVert = R"(
        attribute vec2 position;
        void main() { gl_Position = vec4(position, 0.0, 1.0); }
    )";
    const char* const kLineFrag = R"(
        uniform vec4 lineColor;
        void main() { gl_FragColor = lineColor; }
    )";
}
