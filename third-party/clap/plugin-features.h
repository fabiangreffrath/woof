/*
 * CLAP - CLever Audio Plugin
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2014...2022 Alexandre BIQUE <bique.alexandre@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

// This file provides a set of standard plugin features meant to be used
// within clap_plugin_descriptor.features.

#define CLAP_PLUGIN_FEATURE_INSTRUMENT "instrument"
#define CLAP_PLUGIN_FEATURE_AUDIO_EFFECT "audio-effect"
#define CLAP_PLUGIN_FEATURE_NOTE_EFFECT "note-effect"
#define CLAP_PLUGIN_FEATURE_NOTE_DETECTOR "note-detector"
#define CLAP_PLUGIN_FEATURE_ANALYZER "analyzer"

#define CLAP_PLUGIN_FEATURE_SYNTHESIZER "synthesizer"
#define CLAP_PLUGIN_FEATURE_SAMPLER "sampler"
#define CLAP_PLUGIN_FEATURE_DRUM "drum"
#define CLAP_PLUGIN_FEATURE_DRUM_MACHINE "drum-machine"

#define CLAP_PLUGIN_FEATURE_FILTER "filter"
#define CLAP_PLUGIN_FEATURE_PHASER "phaser"
#define CLAP_PLUGIN_FEATURE_EQUALIZER "equalizer"
#define CLAP_PLUGIN_FEATURE_DEESSER "de-esser"
#define CLAP_PLUGIN_FEATURE_PHASE_VOCODER "phase-vocoder"
#define CLAP_PLUGIN_FEATURE_GRANULAR "granular"
#define CLAP_PLUGIN_FEATURE_FREQUENCY_SHIFTER "frequency-shifter"
#define CLAP_PLUGIN_FEATURE_PITCH_SHIFTER "pitch-shifter"

#define CLAP_PLUGIN_FEATURE_DISTORTION "distortion"
#define CLAP_PLUGIN_FEATURE_TRANSIENT_SHAPER "transient-shaper"
#define CLAP_PLUGIN_FEATURE_COMPRESSOR "compressor"
#define CLAP_PLUGIN_FEATURE_EXPANDER "expander"
#define CLAP_PLUGIN_FEATURE_GATE "gate"
#define CLAP_PLUGIN_FEATURE_LIMITER "limiter"

#define CLAP_PLUGIN_FEATURE_FLANGER "flanger"
#define CLAP_PLUGIN_FEATURE_CHORUS "chorus"
#define CLAP_PLUGIN_FEATURE_DELAY "delay"
#define CLAP_PLUGIN_FEATURE_REVERB "reverb"

#define CLAP_PLUGIN_FEATURE_TREMOLO "tremolo"
#define CLAP_PLUGIN_FEATURE_GLITCH "glitch"

#define CLAP_PLUGIN_FEATURE_UTILITY "utility"
#define CLAP_PLUGIN_FEATURE_PITCH_CORRECTION "pitch-correction"
#define CLAP_PLUGIN_FEATURE_RESTORATION "restoration"

#define CLAP_PLUGIN_FEATURE_MULTI_EFFECTS "multi-effects"

#define CLAP_PLUGIN_FEATURE_MIXING "mixing"
#define CLAP_PLUGIN_FEATURE_MASTERING "mastering"

#define CLAP_PLUGIN_FEATURE_MONO "mono"
#define CLAP_PLUGIN_FEATURE_STEREO "stereo"
#define CLAP_PLUGIN_FEATURE_SURROUND "surround"
#define CLAP_PLUGIN_FEATURE_AMBISONIC "ambisonic"