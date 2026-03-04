# Plugin Code Structure & Comments Guide

## Overview
**Dynamic Reverb** is a VST3 audio plugin that:
1. Analyzes incoming audio spectrum via FFT
2. Creates an inverse/opposite EQ curve
3. Applies reverb convolution with user-selected IR
4. Applies the inverse EQ to prevent "muddy" coloration

---

## File Structure

### Source/PluginProcessor.h
**Main DSP Engine**

```
CLASS: DynamicConvolutionReverbAudioProcessor

MEMBERS:
- convolutionReverb      : JUCE Convolution object (applies reverb)
- forwardFFT/inverseFFT  : FFT engines (frequency analysis)
- window                 : Hann window function (reduces spectral leakage)
- fftData                : Working buffer for FFT (2048 complex samples)
- irData                 : Storage for inverse magnitude filter
- dynamicEQ              : Stereo FIR filter applying inverse EQ
- parameters             : Parameter tree (smoothing level selector)

KEY METHODS:
- prepareToPlay()        : Initializes DSP when playback starts
- processBlock()         : Main audio processing (called each buffer)
- updateDynamicEQ()      : Core algorithm - computes inverse EQ
- loadImpulseResponseFile() : Loads IR file for reverb
- getState/setStateInformation() : Save/restore plugin state
```

---

### Source/PluginProcessor.cpp
**Complete Processing Implementation**

#### Constructor
- Initializes AudioProcessor with stereo I/O
- Creates FFT engines (order 11 = 2048 samples)
- Sets up Hann window for spectral analysis
- Creates parameter tree with 4 smoothing choices

#### loadImpulseResponseFile(const juce::File&)
- Loads audio file (.wav, .aiff, .flac)
- Passes to convolution engine with:
  - Stereo: yes (uses both channels if stereo IR)
  - Trim: no (keeps full IR)
  - Normalise: yes (prevents clipping)
- Sets irLoaded flag and stores file path

#### getStateInformation / setStateInformation
- Serializes plugin state for host save/recall
- Saves: parameter state (XML) + IR file path
- On restore, IR is automatically reloaded in prepareToPlay()

#### prepareToPlay(double sampleRate, int samplesPerBlock)
1. Creates ProcessSpec with host settings
2. Prepares convolution engine
3. Prepares dynamic EQ filter with unity (1.0) initially
4. Reloads IR if one was saved

#### processBlock(AudioBuffer&, MidiBuffer&)
**MAIN AUDIO PROCESSING - 5 STEP CHAIN:**

```
1. CLEANUP
   - Clear unused output channels

2. SPECTRUM ANALYSIS
   - Copy dry signal
   - Call updateDynamicEQ() to compute inverse EQ

3. CONVOLUTION (Reverb)
   - If IR loaded: apply convolutionReverb to buffer
   - If no IR: signal passes through unchanged

4. DYNAMIC EQ APPLICATION
   - Apply dynamicEQ FIR filter to result
   - This compensates for the dry signal's coloration

5. OUTPUT
   - Processed audio with reverb + balanced spectrum
```

#### updateDynamicEQ(const AudioBuffer&) - CORE ALGORITHM
**This is where the magic happens!**

```
STEP 1: COPY & WINDOW
  - Copy first channel to fftData
  - Apply Hann window (reduces spectral leakage)

STEP 2: FORWARD FFT
  - Time domain → Frequency domain (complex numbers)
  - Output: [Re0, Im0, Re1, Im1, ..., Re1024, Im1024]

STEP 3: MAGNITUDE EXTRACTION
  - magnitude[i] = sqrt(Re^2 + Im^2) + epsilon
  - Result: positive magnitudes for each frequency bin

STEP 4: FRACTIONAL-OCTAVE SMOOTHING
  - Moving average based on user selection:
    * 1/2 octave  → 2 bins averaging
    * 1/4 octave  → 4 bins
    * 1/8 octave  → 8 bins (default)
    * 1/16 octave → 16 bins
  - Result: smooth curve matching human hearing

STEP 5: INVERT SPECTRUM
  - inverted[i] = 1 / smoothMag[i]
  - Boosts weak frequencies, reduces strong ones
  - Converts to complex form (real = inverted, imag = 0)

STEP 6: INVERSE FFT
  - Frequency domain → Time domain
  - Result: FIR filter coefficients

STEP 7: APPLY FILTER
  - Create FIR::Coefficients from impulse
  - Update dynamicEQ filter
  - Will be applied in next processBlock()
```

**RESULT:**
The inverse filter cancels the dry signal's coloration in the reverb,
creating a clean, balanced reverberant output without "muddy" buildup.

---

### Source/PluginEditor.h
**User Interface Declaration**

```
CLASS: DynamicConvolutionReverbAudioProcessorEditor

UI COMPONENTS:
1. loadButton
   - Text: "Load IR…"
   - Action: Opens file chooser for impulse response files
   
2. smoothingBox
   - ComboBox with 4 choices:
     * "1/2 octave"  - Broadest smoothing (less aggressive)
     * "1/4 octave"  - Medium smoothing
     * "1/8 octave"  - Fine smoothing (default)
     * "1/16 octave" - Finest smoothing (most aggressive)

3. statusLabel
   - Displays loaded IR filename
   - Shows "<no IR loaded>" if empty

INTERNAL:
- smoothingAttachment: Automatically syncs ComboBox with processor.parameters
- processor reference: Access to DSP objects
```

---

### Source/PluginEditor.cpp
**User Interface Implementation**

#### Constructor
- Initializes UI components
- Adds listeners (button click handler)
- Creates ComboBoxAttachment to link smoothing selector to parameters
- Displays IR filename status
- Sets window size to 400x160

#### paint(Graphics&)
- Fills background with dark grey
- Very minimal - focus on functionality

#### resized()
- Positions components vertically:
  1. Load IR button (top, 30px)
  2. Smoothing combobox (middle, 30px)
  3. Status label (bottom, 25px)

#### buttonClicked(Button*)
- Triggered when user clicks "Load IR…" button
- Opens file chooser dialog (async)
- Filters for audio files: *.wav, *.aiff, *.flac
- On selection:
  - Calls processor.loadImpulseResponseFile()
  - Updates statusLabel with filename

---

## Signal Flow Summary

```
INPUT AUDIO
    ↓
[Copy to FFT buffer]
    ↓
[Forward FFT - get spectrum]
    ↓
[Extract magnitudes]
    ↓
[Fractional-octave smoothing]
    ↓
[Invert magnitude (1/mag)]
    ↓
[Inverse FFT - get FIR coefficients]
    ↓
[Update dynamicEQ filter]
    ↓
[Apply Convolution Reverb] (if IR loaded)
    ↓
[Apply Dynamic EQ Filter]
    ↓
OUTPUT AUDIO (reverb + balanced spectrum)
```

---

## Parameters

### "smoothing" (Discrete Choice)
- **Type:** AudioParameterChoice (4 options)
- **Values:** 0 (1/2 oct), 1 (1/4 oct), 2 (1/8 oct), 3 (1/16 oct)
- **Default:** 2 (1/8 octave)
- **Effect:** Controls frequency bin averaging window size in FFT analysis
  - Lower = broader smoothing (less detailed)
  - Higher = finer smoothing (more detailed)

---

## Technical Specs

| Parameter | Value | Notes |
|-----------|-------|-------|
| FFT Size | 2048 (2^11) | Balance of resolution & latency |
| Freq. Resolution @ 48kHz | 23.4 Hz | Suitable for octave smoothing |
| Latency | ~21 ms | Acceptable for reverb |
| Window | Hann | Reduces spectral leakage |
| I/O | Stereo | 2 channels in/out |
| Convolution | Stereo | Full IR stereo support |
| EQ Filter | FIR, Stereo | ProcessorDuplicator (L+R) |

---

## Key Insights

### Why FFT + Inverse?
The inverse magnitude curve naturally creates a complementary EQ that "undoes"
the coloration of the dry signal. When applied to the reverb output, it
removes the "muddy" quality that happens when spectral imbalance accumulates.

### Why Fractional-Octave Smoothing?
Human hearing is logarithmic (octave-based), not linear. A 10 Hz difference
matters more at low frequencies than at high frequencies. Octave smoothing
makes the EQ curve more perceptually balanced.

### Why Window Function?
FFT assumes the signal repeats infinitely. A Hann window "fades in/out" the
signal at boundaries, reducing spectral leakage (artifacts at frequency bin edges).

### Why ProcessorDuplicator?
FIR filters in JUCE are mono. ProcessorDuplicator duplicates the mono filter
to process both stereo channels independently without cross-channel interaction.

