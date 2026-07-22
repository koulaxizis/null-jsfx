const guides = {

  // ─── UTILITY ───
  'gain-stage': {
    title: 'GAIN STAGE',
    subtitle: 'Clean Gain &amp; Boost',
    sliders: [
      { name: 'Input Gain', range: '-12 to +12 dB', desc: 'Adjust input level before processing' },
      { name: 'Output Gain', range: '-12 to +12 dB', desc: 'Adjust final output level' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Match track levels in a mix', 'Prevent digital clipping downstream', 'Add clean headroom'],
    properties: 'Transparent amplification, no coloration'
  },
  'phase-flip': {
    title: 'PHASE FLIP',
    subtitle: 'Polarity Inverter',
    sliders: [
      { name: 'Phase', range: '0 or 180 degrees', desc: 'Invert signal polarity' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Fix phase cancellation with mics', 'Align multi-mic setups', 'Correct wiring polarity'],
    properties: 'Instant polarity reversal, no latency'
  },
  'dc-block': {
    title: 'DC BLOCK',
    subtitle: 'Offset Remover',
    sliders: [
      { name: 'Cutoff', range: '5 to 20 Hz', desc: 'High-pass frequency threshold' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove DC offset from recordings', 'Clean up subsonic rumble', 'Fix ADC artifacts'],
    properties: 'Subtractive filtering below audible range'
  },
  'mono-check': {
    title: 'MONO CHECK',
    subtitle: 'Compatibility Test',
    sliders: [
      { name: 'Mode', range: 'Stereo or Mono', desc: 'Collapse to mono sum' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Check for phase issues in mono', 'Test club PA compatibility', 'Verify stereo balance'],
    properties: 'L+R summed, detects cancellation'
  },
  'solo-mute': {
    title: 'SOLO MUTE',
    subtitle: 'Channel Isolator',
    sliders: [
      { name: 'Mode', range: 'Mute L / Mute R / Solo L / Solo R', desc: 'Operation mode' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Listen to single channel', 'Isolate problems', 'Check phase issues'],
    properties: 'Channel selection, not routing'
  },
  'channel-balance': {
    title: 'CHANNEL BALANCE',
    subtitle: 'L/R Level Trim',
    sliders: [
      { name: 'Left', range: '-6 to +6 dB', desc: 'Left channel trim' },
      { name: 'Right', range: '-6 to +6 dB', desc: 'Right channel trim' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Correct mic imbalance', 'Fix stereo field', 'Fine-tune mixes'],
    properties: 'Independent channel gain'
  },
  'click-repair': {
    title: 'CLICK REPAIR',
    subtitle: 'Artifact Remover',
    sliders: [
      { name: 'Threshold', range: '-60 to 0 dB', desc: 'Detection sensitivity' },
      { name: 'Window', range: '1 to 20 samples', desc: 'Repair window size' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove vinyl clicks and pops', 'Clean digital glitches', 'Restore old recordings'],
    properties: 'Transient detection and interpolation'
  },

  // ─── DYNAMICS ───
  'compressor': {
    title: 'COMPRESSOR',
    subtitle: 'Dynamic Control',
    sliders: [
      { name: 'Threshold', range: '-60 to 0 dB', desc: 'Level above which compression starts' },
      { name: 'Ratio', range: '1:1 to 20:1', desc: 'Compression intensity' },
      { name: 'Attack', range: '1 to 100 ms', desc: 'Time to engage compression' },
      { name: 'Release', range: '10 to 500 ms', desc: 'Time to disengage compression' },
      { name: 'Makeup', range: '-12 to +12 dB', desc: 'Output gain compensation' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Control vocal dynamics', 'Glue drum bus together', 'Smooth out bass playing'],
    properties: 'VCA-style compression, low latency'
  },
  'expander': {
    title: 'EXPANDER',
    subtitle: 'Downward Expansion',
    sliders: [
      { name: 'Threshold', range: '-60 to 0 dB', desc: 'Level below which expansion occurs' },
      { name: 'Ratio', range: '1:1 to 1:4', desc: 'Expansion amount' },
      { name: 'Range', range: '-60 to 0 dB', desc: 'Maximum gain reduction' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Reduce noise between passages', 'Open up dynamics after compression', 'Tame quiet ambient noise'],
    properties: 'Inverse of compression, increases dynamic range'
  },
  'noise-gate': {
    title: 'NOISE GATE',
    subtitle: 'Floor Suppressor',
    sliders: [
      { name: 'Threshold', range: '-60 to 0 dB', desc: 'Level below which gate closes' },
      { name: 'Attack', range: '0.1 to 50 ms', desc: 'Time to open gate' },
      { name: 'Release', range: '10 to 500 ms', desc: 'Time to close gate' },
      { name: 'Hold', range: '0 to 200 ms', desc: 'Minimum time gate stays open' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Silence guitar between riffs', 'Kick drum separation', 'Mic bleed reduction'],
    properties: 'Mutes signal below threshold'
  },
  'de-esser': {
    title: 'DE-ESSER',
    subtitle: 'Sibilance Control',
    sliders: [
      { name: 'Frequency', range: '2000 to 8000 Hz', desc: 'Target sibilance frequency' },
      { name: 'Threshold', range: '-60 to 0 dB', desc: 'Level to trigger reduction' },
      { name: 'Amount', range: '0 to 100%', desc: 'Maximum attenuation' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Reduce harsh "S" and "T" sounds', 'Smooth vocal recordings', 'Fix bright consonants'],
    properties: 'Frequency-selective compression'
  },
  'vocal-gate': {
    title: 'VOCAL GATE',
    subtitle: 'Voice Optimized',
    sliders: [
      { name: 'Threshold', range: '-60 to 0 dB', desc: 'Gate activation point' },
      { name: 'Range', range: '-60 to 0 dB', desc: 'Reduction amount' },
      { name: 'Attack', range: '1 to 20 ms', desc: 'Opening speed' },
      { name: 'Release', range: '50 to 300 ms', desc: 'Closing speed' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Live vocal cleanup', 'Podcast silence removal', 'Narration gating'],
    properties: 'Voice-tuned detection curve'
  },
  'ceiling-limit': {
    title: 'CEILING LIMIT',
    subtitle: 'Brickwall Protector',
    sliders: [
      { name: 'Ceiling', range: '-3 to 0 dBTP', desc: 'Maximum output level' },
      { name: 'Lookahead', range: '0.5 to 5 ms', desc: 'Anticipation buffer' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Prevent inter-sample peaks', 'Protect masters from clipping', 'Streaming loudness safety'],
    properties: 'True-peak brickwall limiting'
  },
  'soft-limit': {
    title: 'SOFT LIMIT',
    subtitle: 'Transparent Ceiling',
    sliders: [
      { name: 'Ceiling', range: '-6 to 0 dB', desc: 'Soft ceiling threshold' },
      { name: 'Knee', range: '0.5 to 5 dB', desc: 'Transition softness' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Musical loudness control', 'Transparent mastering', 'Avoid aggressive limiting'],
    properties: 'Program-dependent soft knee'
  },
  'transient-shaper': {
    title: 'TRANSIENT SHAPER',
    subtitle: 'Envelope Shaper',
    sliders: [
      { name: 'Attack', range: '-100 to +100', desc: 'Increase/decrease transients' },
      { name: 'Sustain', range: '-100 to +100', desc: 'Increase/decrease sustain' },
      { name: 'Threshold', range: '0 to 100%', desc: 'Detection sensitivity' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Punchy kick drums', 'Extend guitar sustain', 'Control percussive attacks'],
    properties: 'Envelope detection, not EQ-based'
  },
  'clipper': {
    title: 'CLIPPER',
    subtitle: 'Hard Peak Limiter',
    sliders: [
      { name: 'Threshold', range: '-24 to 0 dB', desc: 'Clipping point' },
      { name: 'Softness', range: '0 to 100%', desc: 'Hard vs soft clip character' },
      { name: 'Output', range: '-12 to 0 dB', desc: 'Post-clip gain' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Digital saturation', 'Mastering loudness', 'Aggressive limiting alternative'],
    properties: 'Symmetrical clipping with blendable softness'
  },

  // ─── EQ / FILTERS ───
  'low-cut': {
    title: 'LOW CUT',
    subtitle: 'High-Pass Filter',
    sliders: [
      { name: 'Frequency', range: '20 to 500 Hz', desc: 'Cut-off point' },
      { name: 'Slope', range: '12 to 48 dB/oct', desc: 'Filter steepness' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove rumble from vocals', 'Clean sub frequencies', 'Free up low-end for bass/kick'],
    properties: 'Steep high-pass, phase-linear'
  },
  'high-cut': {
    title: 'HIGH CUT',
    subtitle: 'Low-Pass Filter',
    sliders: [
      { name: 'Frequency', range: '500 to 20000 Hz', desc: 'Cut-off point' },
      { name: 'Slope', range: '12 to 48 dB/oct', desc: 'Filter steepness' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove hiss/treble', 'Vintage tone darkening', 'Sidechain ducking cleanup'],
    properties: 'Steep low-pass, phase-linear'
  },
  'notch-filter': {
    title: 'NOTCH FILTER',
    subtitle: 'Surgical EQ',
    sliders: [
      { name: 'Frequency', range: '20 to 20000 Hz', desc: 'Center frequency' },
      { name: 'Width', range: '0.1 to 5 octaves', desc: 'Q factor' },
      { name: 'Gain', range: '-60 to 0 dB', desc: 'Attenuation depth' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove hum (50/60 Hz)', 'Eliminate ringing frequencies', 'Fix resonant peaks'],
    properties: 'Very narrow bandwidth rejection'
  },
  'wide-cut': {
    title: 'WIDE CUT',
    subtitle: 'Mud Remover',
    sliders: [
      { name: 'Frequency', range: '100 to 500 Hz', desc: 'Center of mud zone' },
      { name: 'Width', range: '0.5 to 2 octaves', desc: 'Q factor' },
      { name: 'Gain', range: '-12 to 0 dB', desc: 'Attenuation' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Clear muddy mixes', 'Tighten bass guitars', 'Reduce boxiness'],
    properties: 'Wide Q subtraction for mix clarity'
  },
  'sub-boost': {
    title: 'SUB BOOST',
    subtitle: 'Low Shelf Enhancer',
    sliders: [
      { name: 'Frequency', range: '20 to 200 Hz', desc: 'Shelf point' },
      { name: 'Gain', range: '0 to +12 dB', desc: 'Boost amount' },
      { name: 'Q', range: '0.5 to 3', desc: 'Shelf width' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add bass weight', 'Enhance kick/sub', 'Thicken synth bass'],
    properties: 'Shelving boost at low end'
  },
  'mid-presence': {
    title: 'MID PRESENCE',
    subtitle: 'Clarity Booster',
    sliders: [
      { name: 'Frequency', range: '500 to 4000 Hz', desc: 'Presence zone' },
      { name: 'Gain', range: '0 to +12 dB', desc: 'Boost amount' },
      { name: 'Q', range: '0.5 to 3', desc: 'Bandwidth' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Make vocals cut through', 'Add instrument definition', 'Fix distant recordings'],
    properties: 'Targeted mid-range enhancement'
  },
  'air-boost': {
    title: 'AIR BOOST',
    subtitle: 'High Shelf Sparkle',
    sliders: [
      { name: 'Frequency', range: '5000 to 20000 Hz', desc: 'Air shelf point' },
      { name: 'Gain', range: '0 to +12 dB', desc: 'Sparkle amount' },
      { name: 'Q', range: '0.5 to 2', desc: 'Shelf width' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add brilliance to vocals', 'Brighten dull recordings', 'Modern sheen effect'],
    properties: 'Shelving boost at high end'
  },
  '3-band-eq': {
    title: '3-BAND EQ',
    subtitle: 'Parametric EQ',
    sliders: [
      { name: 'Low Gain', range: '-12 to +12 dB', desc: 'Low shelf boost/cut' },
      { name: 'Mid Gain', range: '-12 to +12 dB', desc: 'Bell boost/cut' },
      { name: 'High Gain', range: '-12 to +12 dB', desc: 'High shelf boost/cut' },
      { name: 'Low-Mid Xover', range: '50 to 2000 Hz', desc: 'Low/mid crossover' },
      { name: 'Mid-High Xover', range: '500 to 10000 Hz', desc: 'Mid/high crossover' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Basic tone shaping', 'Mix balance', 'Instrument carving'],
    properties: 'Three-band parametric structure'
  },
  'filter': {
    title: 'FILTER',
    subtitle: 'Resonant Filter',
    sliders: [
      { name: 'Cutoff', range: '20 to 20000 Hz', desc: 'Filter frequency' },
      { name: 'Resonance', range: '0.1 to 10', desc: 'Peak emphasis at cutoff' },
      { name: 'Gain', range: '-12 to +12 dB', desc: 'Output gain' },
      { name: 'Mode', range: 'LP / BP / HP / Notch', desc: 'Filter type' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Synth-style filtering', 'Creative sweeps', 'DJ-style transitions'],
    properties: 'State-variable, resonance adjustable'
  },
  'comb-filter': {
    title: 'COMB FILTER',
    subtitle: 'Resonant Notches',
    sliders: [
      { name: 'Frequency', range: '20 to 8000 Hz', desc: 'Spacing between notches' },
      { name: 'Feedback', range: '0 to 95%', desc: 'Resonance level' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Metallic tones', 'Flanger-style effects', 'Alien textures'],
    properties: 'Series of harmonic notches'
  },
  'ladder-filter': {
    title: 'LADDER FILTER',
    subtitle: 'Moog-Style Filter',
    sliders: [
      { name: 'Cutoff', range: '20 to 20000 Hz', desc: 'Filter frequency' },
      { name: 'Resonance', range: '0 to 10', desc: 'Emphasis at cutoff' },
      { name: 'Mode', range: 'LP / BP / HP', desc: 'Filter type' },
      { name: 'Gain', range: '-12 to +12 dB', desc: 'Output gain' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Analog bass lines', 'Classic synth tones', 'Warm filter sweeps'],
    properties: 'Four-pole transistor ladder emulation'
  },

  // ─── TIME-BASED ───
  'pre-delay': {
    title: 'PRE DELAY',
    subtitle: 'Short Thickening',
    sliders: [
      { name: 'Time', range: '1 to 50 ms', desc: 'Delay before effect' },
      { name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Separate vocal from reverb', 'Add depth without wash', 'Double-track effect'],
    properties: 'Sub-50ms slapback only'
  },
  'delay': {
    title: 'DELAY',
    subtitle: 'Echo &amp; Feedback',
    sliders: [
      { name: 'Time', range: '1 to 2000 ms', desc: 'Delay time' },
      { name: 'Feedback', range: '0 to 100%', desc: 'Repetition amount' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Slapback echoes', 'Spacey repeats', 'Doubling effect'],
    properties: 'Stereo delay, variable feedback'
  },
  'reverb': {
    title: 'REVERB',
    subtitle: 'Room Simulation',
    sliders: [
      { name: 'Room Size', range: '10 to 100%', desc: 'Simulated space' },
      { name: 'Decay', range: '0 to 100%', desc: 'Reverb length' },
      { name: 'Damping', range: '0 to 100%', desc: 'High-frequency loss' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add natural space', 'Hall ambience', 'Plate-style reverb'],
    properties: 'Algorithmic reverb with comb filters and all-pass'
  },

  // ─── MODULATION ───
  'tremolo': {
    title: 'TREMOLO',
    subtitle: 'Amplitude Modulation',
    sliders: [
      { name: 'Rate', range: '0.1 to 20 Hz', desc: 'Modulation speed' },
      { name: 'Depth', range: '0 to 100%', desc: 'Modulation intensity' },
      { name: 'Wave', range: 'Sine / Triangle / Square', desc: 'LFO shape' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Vintage amp pulsing', 'Rhythmic gating', '60s organ effect'],
    properties: 'Pure amplitude oscillation'
  },
  'auto-pan': {
    title: 'AUTO PAN',
    subtitle: 'LFO Panning',
    sliders: [
      { name: 'Rate', range: '0.1 to 20 Hz', desc: 'Panning speed' },
      { name: 'Depth', range: '0 to 100%', desc: 'L/R swing amount' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Stereo movement', 'Dubby left/right panning', 'Psychedelic swells'],
    properties: 'Automatic left/right oscillation'
  },
  'chorus': {
    title: 'CHORUS',
    subtitle: 'Detune Thickening',
    sliders: [
      { name: 'Rate', range: '0.5 to 30 Hz', desc: 'Modulation speed' },
      { name: 'Depth', range: '0 to 10 ms', desc: 'Detune amount' },
      { name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['90s chorus guitars', 'Thicken pads', 'Vocal doubling'],
    properties: 'Pitch-modulated delay with detune'
  },
  'phaser': {
    title: 'PHASER',
    subtitle: 'Moving Notches',
    sliders: [
      { name: 'Rate', range: '0.1 to 5 Hz', desc: 'Sweep speed' },
      { name: 'Stages', range: '1 to 20', desc: 'Number of all-pass filters' },
      { name: 'Depth', range: '0.5 to 5', desc: 'Sweep extent' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Classic guitar phaser', '70s keyboard swirl', 'Vocal sweep effect'],
    properties: 'Phase cancellation moving notches'
  },
  'flanger': {
    title: 'FLANGER',
    subtitle: 'Jet Sweep Effect',
    sliders: [
      { name: 'Rate', range: '0.1 to 5 Hz', desc: 'Sweep speed' },
      { name: 'Depth', range: '1 to 20 ms', desc: 'Delay variation' },
      { name: 'Feedback', range: '0 to 100%', desc: 'Intensity' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Jet-plane whoosh', 'Drum flanging', 'Spacey textures'],
    properties: 'Very short modulated delay with feedback'
  },
  'ring-mod': {
    title: 'RING MOD',
    subtitle: 'Metallic Modulation',
    sliders: [
      { name: 'Frequency', range: '20 to 2000 Hz', desc: 'Carrier frequency' },
      { name: 'Mix', range: '0 to 100%', desc: 'Modulation level' },
      { name: 'Gain', range: '-12 to +12 dB', desc: 'Output gain' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Sci-fi robot voices', 'Bell-like harmonics', 'Industrial textures'],
    properties: 'Multiplication of signal by sine wave'
  },
  'vibrato': {
    title: 'VIBRATO',
    subtitle: 'Pitch Modulation',
    sliders: [
      { name: 'Rate', range: '0.1 to 15 Hz', desc: 'Pitch modulation speed' },
      { name: 'Depth', range: '0 to 20 cents', desc: 'Pitch deviation' },
      { name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Classic amp vibrato', 'Detuned vocals', 'Tape flutter effect'],
    properties: 'Pitch modulation, not amplitude (vs tremolo)'
  },

  // ─── SATURATION / DISTORTION ───
  'tape-saturation': {
    title: 'TAPE SATURATION',
    subtitle: 'Analog Warmth',
    sliders: [
      { name: 'Drive', range: '0 to 100%', desc: 'Saturation level' },
      { name: 'Bias', range: '0 to 100%', desc: 'High-frequency loss' },
      { name: 'Hiss', range: '0 to 100%', desc: 'Tape noise simulation' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Vintage warmth', 'Glue bus compression', 'Analog emulation'],
    properties: 'Soft-knee harmonic saturation'
  },
  'harmonic-drive': {
    title: 'HARMONIC DRIVE',
    subtitle: 'Warmth Exciter',
    sliders: [
      { name: 'Drive', range: '0 to 100%', desc: 'Harmonic generation' },
      { name: 'Tone', range: '-12 to +12 dB', desc: 'Frequency balance' },
      { name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add harmonics to quiet tracks', 'Exciter effect', 'Perceived loudness'],
    properties: 'Even-order harmonic emphasis'
  },
  'distortion': {
    title: 'DISTORTION',
    subtitle: 'Overdrive &amp; Grit',
    sliders: [
      { name: 'Drive', range: '0 to 100%', desc: 'Distortion amount' },
      { name: 'Tone', range: '-12 to +12 dB', desc: 'Frequency balance' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Guitar amps', 'Aggressive synths', 'Lo-fi grit'],
    properties: 'Soft-clipping wavefolding'
  },
  'bit-crush': {
    title: 'BIT CRUSH',
    subtitle: 'Lo-Fi Degradation',
    sliders: [
      { name: 'Bit Depth', range: '4 to 24 bits', desc: 'Sample resolution' },
      { name: 'Sample Rate', range: '1k to 48k Hz', desc: 'Downsample factor' },
      { name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['8-bit retro sounds', 'Digital artifacts', 'Chiptune emulation'],
    properties: 'Quantization and sample rate reduction'
  },

  // ─── STEREO ───
  'stereo-width': {
    title: 'STEREO WIDTH',
    subtitle: 'Mid/Side Spread',
    sliders: [
      { name: 'Width', range: '0 to 200%', desc: 'Stereo image size' },
      { name: 'Low Cut', range: '20 to 500 Hz', desc: 'Bass mono threshold' },
      { name: 'High Cut', range: '500 to 20000 Hz', desc: 'Treble mono threshold' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Widen synths', 'Monaural bass', 'Fix narrow recordings'],
    properties: 'Mid/Side manipulation'
  },

  // ─── CREATIVE ───
  'pitch-shifter': {
    title: 'PITCH SHIFTER',
    subtitle: 'Harmony Generator',
    sliders: [
      { name: 'Semitones', range: '-12 to +12', desc: 'Pitch shift interval' },
      { name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Create harmonies', 'Detune guitars', 'Pitch correction assist'],
    properties: 'Granular pitch shifting'
  }
};

function openGuide(pluginId) {
  const guide = guides[pluginId];
  if (!guide) {
    console.warn('No guide found for:', pluginId);
    return;
  }

  const modalBody = document.getElementById('modal-body');
  let html = '<div class="guide-header"><h3>' + guide.title + '</h3><p>' + guide.subtitle + '</p></div>';

  html += '<div class="guide-body">';

  html += '<div class="guide-section"><h4>Controls</h4><ul class="guide-sliders">';
  guide.sliders.forEach(function(slider) {
    html += '<li><span class="slider-name">' + slider.name + '</span><span class="slider-desc">' + slider.range + ' — ' + slider.desc + '</span></li>';
  });
  html += '</ul></div>';

  html += '<div class="guide-section"><h4>Best Uses</h4><ul class="use-cases">';
  guide.uses.forEach(function(use) {
    html += '<li>' + use + '</li>';
  });
  html += '</ul></div>';

  html += '<div class="guide-section"><h4>Properties</h4><p style="font-size:12px;color:#999;">' + guide.properties + '</p></div>';

  html += '</div>';

  modalBody.innerHTML = html;
  document.getElementById('guide-modal').classList.add('active');
}

function closeModal() {
  document.getElementById('guide-modal').classList.remove('active');
}

document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') {
    closeModal();
  }
});

document.getElementById('guide-modal').addEventListener('click', function(e) {
  if (e.target === this) {
    closeModal();
  }
});

console.log('NULL JSFX Guides loaded:', Object.keys(guides).length, 'plugins');