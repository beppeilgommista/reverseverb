# DynamicConvolutionReverb

A JUCE-based audio plugin implementing dynamic convolution reverb with fractional octave processing.

## Requirements

- **CMake** 3.15 or higher
- **C++17** compatible compiler (GCC, Clang, or MSVC)
- **JUCE** framework (included as git submodule)

### Linux Dependencies

Install required development packages:

```bash
sudo apt-get update
sudo apt-get install libasound2-dev libcurl4-openssl-dev libgtk-3-dev
```

### macOS

Xcode Command Line Tools required (no additional packages needed for basic build).

### Windows

Visual Studio 2019 or later with C++ workload.

## Build Instructions

### Clone the Repository

```bash
git clone --recursive https://github.com/yourusername/reverseverb.git
cd reverseverb
```

If cloning without `--recursive`, initialize submodules:

```bash
git submodule update --init --recursive JUCE
```

### Configure & Build

```bash
cmake -B build
cmake --build build --config Release
```

For faster builds on multi-core systems:

```bash
cmake --build build --config Release -- -j4
```

## Output Artifacts

After successful build, you'll find:

- **VST3 Plugin**: `build/DynamicConvolutionReverb_artefacts/VST3/DynamicReverb.vst3`
- **Standalone App**: `build/DynamicConvolutionReverb_artefacts/Standalone/DynamicReverb`

### Installing VST3 Plugin (Linux)

```bash
mkdir -p ~/.vst3
cp -r build/DynamicConvolutionReverb_artefacts/VST3/DynamicReverb.vst3 ~/.vst3/
```

## Development Build (Debug)

```bash
cmake -B build_debug
cmake --build build_debug --config Debug
```

## Troubleshooting

- **Missing ALSA headers** (Linux): Install `libasound2-dev`
- **CMake not found**: Ensure CMake is installed and in your PATH
- **Build cache issues**: Remove the `build` directory and reconfigure: `rm -rf build && cmake -B build`
