const guides = {
  // --- DYNAMICS ---
  'gain-stage': {
    title: 'GAIN STAGE',
    subtitle: 'Clean Gain & Boost',
    sliders: [
      { id: 1, name: 'Input Gain', range: '-12 to +12 dB', desc: 'Adjust input level before processing' },
      { id: 2, name: 'Output Gain', range: '-12 to +12 dB', desc: 'Adjust final output level' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Match track levels in a mix', 'Prevent digital clipping downstream', 'Add clean headroom'],
    properties: 'Transparent amplification, no coloration'
  },
  'phase-flip': {
    title: 'PHASE FLIP',
    subtitle: 'Polarity Inverter',
    sliders: [
      { id: 1, name: 'Phase', range: '0° or 180°', desc: 'Invert signal polarity' },
      { id: 2, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Fix phase cancellation with mics', 'Align multi-mic setups', 'Correct wiring polarity'],
    properties: 'Instant polarity reversal, no latency'
  },
  'dc-block': {
    title: 'DC BLOCK',
    subtitle: 'Offset Remover',
    sliders: [
      { id: 1, name: 'Cutoff', range: '5 to 20 Hz', desc: 'High-pass frequency threshold' },
      { id: 2, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove DC offset from recordings', 'Clean up subsonic rumble', 'Fix ADC artifacts'],
    properties: 'Subtractive filtering below audible range'
  },
  'mono-check': {
    title: 'MONO CHECK',
    subtitle: 'Compatibility Test',
    sliders: [
      { id: 1, name: 'Mono', range: 'Stereo or Mono', desc: 'Collapse to mono sum' },
      { id: 2, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Check for phase issues in mono', 'Test club PA compatibility', 'Verify stereo balance'],
    properties: 'L+R summed, detects cancellation'
  },
  'compressor': {
    title: 'COMPRESSOR',
    subtitle: 'Dynamic Control',
    sliders: [
      { id: 1, name: 'Threshold', range: '-60 to 0 dB', desc: 'Level above which compression starts' },
      { id: 2, name: 'Ratio', range: '1:1 to 20:1', desc: 'Compression intensity' },
      { id: 3, name: 'Attack', range: '1 to 100 ms', desc: 'Time to engage compression' },
      { id: 4, name: 'Release', range: '10 to 500 ms', desc: 'Time to disengage compression' },
      { id: 5, name: 'Makeup', range: '-12 to +12 dB', desc: 'Output gain compensation' },
      { id: 6, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Control vocal dynamics', 'Glue drum bus together', 'Smooth out bass playing'],
    properties: 'VCA-style compression, low latency'
  },
  'expander': {
    title: 'EXPANDER',
    subtitle: 'Downward Expansion',
    sliders: [
      { id: 1, name: 'Threshold', range: '-60 to 0 dB', desc: 'Level below which expansion occurs' },
      { id: 2, name: 'Ratio', range: '1:1 to 1:4', desc: 'Expansion amount' },
      { id: 3, name: 'Range', range: '-60 to 0 dB', desc: 'Maximum gain reduction' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Reduce noise between passages', 'Open up dynamics after compression', 'Tame quiet ambient noise'],
    properties: 'Inverse of compression, increases dynamic range'
  },
  'noise-gate': {
    title: 'NOISE GATE',
    subtitle: 'Floor Suppressor',
    sliders: [
      { id: 1, name: 'Threshold', range: '-60 to 0 dB', desc: 'Level below which gate closes' },
      { id: 2, name: 'Attack', range: '0.1 to 50 ms', desc: 'Time to open gate' },
      { id: 3, name: 'Release', range: '10 to 500 ms', desc: 'Time to close gate' },
      { id: 4, name: 'Hold', range: '0 to 200 ms', desc: 'Minimum time gate stays open' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Silent guitar between riffs', 'Kick drum separation', 'Mic bleed reduction'],
    properties: 'Mutes signal below threshold'
  },
  'de-esser': {
    title: 'DE-ESSER',
    subtitle: 'Sibilance Control',
    sliders: [
      { id: 1, name: 'Frequency', range: '2000 to 8000 Hz', desc: 'Target sibilance frequency' },
      { id: 2, name: 'Threshold', range: '-60 to 0 dB', desc: 'Level to trigger reduction' },
      { id: 3, name: 'Amount', range: '0 to 100%', desc: 'Maximum attenuation' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Reduce harsh "S" and "T" sounds', 'Smooth vocal recordings', 'Fix bright consonants'],
    properties: 'Frequency-selective compression'
  },
  'vocal-gate': {
    title: 'VOCAL GATE',
    subtitle: 'Voice Optimized',
    sliders: [
      { id: 1, name: 'Threshold', range: '-60 to 0 dB', desc: 'Gate activation point' },
      { id: 2, name: 'Range', range: '-60 to 0 dB', desc: 'Reduction amount' },
      { id: 3, name: 'Attack', range: '1 to 20 ms', desc: 'Opening speed' },
      { id: 4, name: 'Release', range: '50 to 300 ms', desc: 'Closing speed' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Live vocal cleanup', 'Podcast silence removal', 'Narration gating'],
    properties: 'Voice-tuned detection curve'
  },
  'ceiling-limit': {
    title: 'CEILING LIMIT',
    subtitle: 'Brickwall Protector',
    sliders: [
      { id: 1, name: 'Ceiling', range: '-3 to 0 dBTP', desc: 'Maximum output level' },
      { id: 2, name: 'Lookahead', range: '0.5 to 5 ms', desc: 'Anticipation buffer' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Prevent inter-sample peaks', 'Protect masters from clipping', 'Streaming loudness safety'],
    properties: 'True-peak brickwall limiting'
  },
  'soft-limit': {
    title: 'SOFT LIMIT',
    subtitle: 'Transparent Ceiling',
    sliders: [
      { id: 1, name: 'Ceiling', range: '-6 to 0 dB', desc: 'Soft ceiling threshold' },
      { id: 2, name: 'Knee', range: '0.5 to 5 dB', desc: 'Transition softness' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Musical loudness control', 'Transparent mastering', 'Avoid aggressive limiting'],
    properties: 'Program-dependent soft knee'
  },
  'transient-shaper': {
    title: 'TRANSIENT SHAPER',
    subtitle: 'Envelope Shaper',
    sliders: [
      { id: 1, name: 'Attack', range: '-100 to +100', desc: 'Increase/decrease transients' },
      { id: 2, name: 'Sustain', range: '-100 to +100', desc: 'Increase/decrease sustain' },
      { id: 3, name: 'Threshold', range: '0 to 100%', desc: 'Detection sensitivity' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Punchy kick drums', 'Extend guitar sustain', 'Control percussive attacks'],
    properties: 'Envelop detection, not EQ-based'
  },
  'clipper': {
    title: 'CLIPPER',
    subtitle: 'Hard Peak Limiter',
    sliders: [
      { id: 1, name: 'Threshold', range: '-24 to 0 dB', desc: 'Clipping point' },
      { id: 2, name: 'Softness', range: '0 to 100%', desc: 'Hard vs soft clip' },
      { id: 3, name: 'Output', range: '-12 to 0 dB', desc: 'Post-clip gain' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Digital saturation', 'Mastering loudness', 'Aggressive limiting alternative'],
    properties: 'Symmetrical or asymmetrical clipping'
  },

  // --- EQ/FILTERS ---
  'low-cut': {
    title: 'LOW CUT',
    subtitle: 'High-Pass Filter',
    sliders: [
      { id: 1, name: 'Frequency', range: '20 to 500 Hz', desc: 'Cut-off point' },
      { id: 2, name: 'Slope', range: '12 to 48 dB/oct', desc: 'Filter steepness' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove rumble from vocals', 'Clean sub frequencies', 'Free up low-end for bass/kick'],
    properties: 'Steep high-pass, phase-linear'
  },
  'high-cut': {
    title: 'HIGH CUT',
    subtitle: 'Low-Pass Filter',
    sliders: [
      { id: 1, name: 'Frequency', range: '500 to 20000 Hz', desc: 'Cut-off point' },
      { id: 2, name: 'Slope', range: '12 to 48 dB/oct', desc: 'Filter steepness' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove hiss/treble', 'Vintage tone darkening', 'Sidechain ducking cleanup'],
    properties: 'Steep low-pass, phase-linear'
  },
  'notch-filter': {
    title: 'NOTCH FILTER',
    subtitle: 'Surgical EQ',
    sliders: [
      { id: 1, name: 'Frequency', range: '20 to 20000 Hz', desc: 'Center frequency' },
      { id: 2, name: 'Width', range: '0.1 to 5 octaves', desc: 'Q factor' },
      { id: 3, name: 'Gain', range: '-60 to 0 dB', desc: 'Attenuation depth' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Remove hum (50/60Hz)', 'Eliminate ringing frequencies', 'Fix resonant peaks'],
    properties: 'Very narrow bandwidth rejection'
  },
  'wide-cut': {
    title: 'WIDE CUT',
    subtitle: 'Mud Remover',
    sliders: [
      { id: 1, name: 'Frequency', range: '100 to 500 Hz', desc: 'Center of mud zone' },
      { id: 2, name: 'Width', range: '0.5 to 2 octaves', desc: 'Q factor' },
      { id: 3, name: 'Gain', range: '-12 to 0 dB', desc: 'Attenuation' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Clear muddy mixes', 'Tighten bass guitars', 'Reduce boxiness'],
    properties: 'Wide Q subtraction for mix clarity'
  },
  'sub-boost': {
    title: 'SUB BOOST',
    subtitle: 'Low Shelf Enhancer',
    sliders: [
      { id: 1, name: 'Frequency', range: '20 to 200 Hz', desc: 'Shelf point' },
      { id: 2, name: 'Gain', range: '0 to +12 dB', desc: 'Boost amount' },
      { id: 3, name: 'Q', range: '0.5 to 3', desc: 'Shelf width' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add bass weight', 'Enhance kick/sub', 'Thicken synth bass'],
    properties: 'Shelving boost at low end'
  },
  'mid-presence': {
    title: 'MID PRESENCE',
    subtitle: 'Clarity Booster',
    sliders: [
      { id: 1, name: 'Frequency', range: '500 to 4000 Hz', desc: 'Presence zone' },
      { id: 2, name: 'Gain', range: '0 to +12 dB', desc: 'Boost amount' },
      { id: 3, name: 'Q', range: '0.5 to 3', desc: 'Bandwidth' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Make vocals cut through', 'Add instrument definition', 'Fix distant recordings'],
    properties: 'Targeted mid-range enhancement'
  },
  'air-boost': {
    title: 'AIR BOOST',
    subtitle: 'High Shelf Sparkle',
    sliders: [
      { id: 1, name: 'Frequency', range: '5000 to 20000 Hz', desc: 'Air shelf point' },
      { id: 2, name: 'Gain', range: '0 to +12 dB', desc: 'Sparkle amount' },
      { id: 3, name: 'Q', range: '0.5 to 2', desc: 'Shelf width' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add brilliance to vocals', 'Brighten dull recordings', 'Modern sheen effect'],
    properties: 'Shelving boost at high end'
  },
  '3-band-eq': {
    title: '3-BAND EQ',
    subtitle: 'Parametric EQ',
    sliders: [
      { id: 1, name: 'Low Gain', range: '-12 to +12 dB', desc: 'Low shelf boost/cut' },
      { id: 2, name: 'Mid Gain', range: '-12 to +12 dB', desc: 'Bell boost/cut' },
      { id: 3, name: 'High Gain', range: '-12 to +12 dB', desc: 'High shelf boost/cut' },
      { id: 4, name: 'Low-Mid Xover', range: '50 to 2000 Hz', desc: 'Low/mid crossover' },
      { id: 5, name: 'Mid-High Xover', range: '500 to 10000 Hz', desc: 'Mid/high crossover' },
      { id: 6, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Basic tone shaping', 'Mix balance', 'Instrument carving'],
    properties: 'Three-band parametric structure'
  },
  'filter': {
    title: 'FILTER',
    subtitle: 'Resonant Filter',
    sliders: [
      { id: 1, name: 'Cutoff', range: '20 to 20000 Hz', desc: 'Filter frequency' },
      { id: 2, name: 'Resonance', range: '0.1 to 10', desc: 'Peak emphasis' },
      { id: 3, name: 'Gain', range: '-12 to +12 dB', desc: 'Output gain' },
      { id: 4, name: 'Mode', range: 'LP/BP/HP/LP+BP', desc: 'Low-pass/Band-pass/High-pass/Notch' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Synth-style filtering', 'Creative sweeps', 'DJ-style transitions'],
    properties: 'State-variable, resonance adjustable'
  },
  'comb-filter': {
    title: 'COMB FILTER',
    subtitle: 'Resonant Notches',
    sliders: [
      { id: 1, name: 'Frequency', range: '20 to 8000 Hz', desc: 'Spacing between notches' },
      { id: 2, name: 'Feedback', range: '0 to 95%', desc: 'Resonance level' },
      { id: 3, name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Metallic tones', 'Flanger-style effects', 'Alien textures'],
    properties: 'Series of harmonic notches'
  },
  'ladder-filter': {
    title: 'LADDER FILTER',
    subtitle: 'Moog-Style Filter',
    sliders: [
      { id: 1, name: 'Cutoff', range: '20 to 20000 Hz', desc: 'Filter frequency' },
      { id: 2, name: 'Resonance', range: '0 to 10', desc: 'Emphasis' },
      { id: 3, name: 'Mode', range: 'LP/BP/HP', desc: 'Filter type' },
      { id: 4, name: 'Gain', range: '-12 to +12 dB', desc: 'Output gain' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Analog bass lines', 'Classic synth tones', 'Warm filter sweeps'],
    properties: 'Four-pole transistor ladder emulation'
  },

  // --- TIME-BASED ---
  'pre-delay': {
    title: 'PRE DELAY',
    subtitle: 'Short Thickening',
    sliders: [
      { id: 1, name: 'Time', range: '1 to 50 ms', desc: 'Delay before effect' },
      { id: 2, name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Separate vocal from reverb', 'Add depth without wash', 'Double-track effect'],
    properties: 'Sub-50ms slapback only'
  },
  'delay': {
    title: 'DELAY',
    subtitle: 'Echo & Feedback',
    sliders: [
      { id: 1, name: 'Time', range: '1 to 2000 ms', desc: 'Delay time' },
      { id: 2, name: 'Feedback', range: '0 to 100%', desc: 'Repetition amount' },
      { id: 3, name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Slapback echoes', 'Spacey repeats', 'Doubling effect'],
    properties: 'Stereo delay, variable feedback'
  },
  'reverb': {
    title: 'REVERB',
    subtitle: 'Room Simulation',
    sliders: [
      { id: 1, name: 'Room Size', range: '10 to 100%', desc: 'Simulated space' },
      { id: 2, name: 'Decay', range: '0 to 100%', desc: 'Reverb length' },
      { id: 3, name: 'Damping', range: '0 to 100%', desc: 'High-frequency loss' },
      { id: 4, name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add natural space', 'Hall ambience', 'Plate-style reverb'],
    properties: 'Algorithmic convolution-like'
  },

  // --- MODULATION ---
  'tremolo': {
    title: 'TREMOLO',
    subtitle: 'Amplitude Modulation',
    sliders: [
      { id: 1, name: 'Rate', range: '0.1 to 20 Hz', desc: 'Modulation speed' },
      { id: 2, name: 'Depth', range: '0 to 100%', desc: 'Modulation intensity' },
      { id: 3, name: 'Wave', range: 'Sine/Triangle/Square', desc: 'LFO shape' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Vintage amp pulsing', 'Rhythmic gating', '60s organ effect'],
    properties: 'Pure amplitude oscillation'
  },
  'auto-pan': {
    title: 'AUTO PAN',
    subtitle: 'LFO Panning',
    sliders: [
      { id: 1, name: 'Rate', range: '0.1 to 20 Hz', desc: 'Panning speed' },
      { id: 2, name: 'Depth', range: '0 to 100%', desc: 'L/R swing amount' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Stereo movement', 'Dubby left/right panning', 'Psychedelic swells'],
    properties: 'Automatic left/right oscillation'
  },
  'chorus': {
    title: 'CHORUS',
    subtitle: 'Detune Thickening',
    sliders: [
      { id: 1, name: 'Rate', range: '0.5 to 30 Hz', desc: 'Modulation speed' },
      { id: 2, name: 'Depth', range: '0 to 10 ms', desc: 'Detune amount' },
      { id: 3, name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['90s chorus guitars', 'Thicken pads', 'Vocal doubling'],
    properties: 'Pitch-modulated delay with detune'
  },
  'phaser': {
    title: 'PHASER',
    subtitle: 'Moving Notches',
    sliders: [
      { id: 1, name: 'Rate', range: '0.1 to 5 Hz', desc: 'Sweep speed' },
      { id: 2, name: 'Stages', range: '1 to 20', desc: 'Number of all-pass filters' },
      { id: 3, name: 'Depth', range: '0.5 to 5', desc: 'Sweep extent' },
      { id: 4, name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Classic guitar phaser', '70s keyboard swirl', 'Vocal sweep effect'],
    properties: 'Phase cancellation moving notches'
  },
  'flanger': {
    title: 'FLANGER',
    subtitle: 'Jet Sweep Effect',
    sliders: [
      { id: 1, name: 'Rate', range: '0.1 to 5 Hz', desc: 'Sweep speed' },
      { id: 2, name: 'Depth', range: '1 to 20 ms', desc: 'Delay variation' },
      { id: 3, name: 'Feedback', range: '0 to 100%', desc: 'Intensity' },
      { id: 4, name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Jet-plane whoosh', 'Drum flanging', 'Spacey textures'],
    properties: 'Very short modulated delay with feedback'
  },
  'ring-mod': {
    title: 'RING MODULATOR',
    subtitle: 'Metallic Modulation',
    sliders: [
      { id: 1, name: 'Frequency', range: '20 to 2000 Hz', desc: 'Carrier frequency' },
      { id: 2, name: 'Mix', range: '0 to 100%', desc: 'Modulation level' },
      { id: 3, name: 'Gain', range: '-12 to +12 dB', desc: 'Output gain' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Sci-fi robot voices', 'Bell-like harmonics', 'Industrial textures'],
    properties: 'Multiplication of signal by sine wave'
  },
  'vibrato': {
    title: 'VIBRATO',
    subtitle: 'Pitch Modulation',
    sliders: [
      { id: 1, name: 'Rate', range: '0.1 to 15 Hz', desc: 'Pitch modulation speed' },
      { id: 2, name: 'Depth', range: '0 to 20 cents', desc: 'Pitch deviation' },
      { id: 3, name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Classic amp vibrato', 'Detuned vocals', 'Tape flutter effect'],
    properties: 'Pitch modulation, not amplitude (vs tremolo)'
  },

  // --- SATURATION/DISTORTION ---
  'tape-saturation': {
    title: 'TAPE SATURATION',
    subtitle: 'Analog Warmth',
    sliders: [
      { id: 1, name: 'Drive', range: '0 to 100%', desc: 'Saturation level' },
      { id: 2, name: 'Bias', range: '0 to 100%', desc: 'High-frequency loss' },
      { id: 3, name: 'Hiss', range: '0 to 100%', desc: 'Tape noise simulation' },
      { id: 4, name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { id: 5, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Vintage warmth', 'Glue bus compression', 'Analog emulation'],
    properties: 'Soft-knee harmonic saturation'
  },
  'harmonic-drive': {
    title: 'HARMONIC DRIVE',
    subtitle: 'Warmth Exciter',
    sliders: [
      { id: 1, name: 'Drive', range: '0 to 100%', desc: 'Harmonic generation' },
      { id: 2, name: 'Tone', range: '-12 to +12 dB', desc: 'Frequency balance' },
      { id: 3, name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Add harmonics to quiet tracks', 'Exciter effect', 'Perceived loudness'],
    properties: 'Even-order harmonic emphasis'
  },
  'distortion': {
    title: 'DISTORTION',
    subtitle: 'Overdrive & Grit',
    sliders: [
      { id: 1, name: 'Drive', range: '0 to 100%', desc: 'Distortion amount' },
      { id: 2, name: 'Tone', range: '-12 to +12 dB', desc: 'Frequency balance' },
      { id: 3, name: 'Mix', range: '0 to 100%', desc: 'Dry/wet balance' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Guitar amps', 'Aggressive synths', 'Lo-fi grit'],
    properties: 'Soft-clipping wavefolding'
  },
  'bit-crush': {
    title: 'BIT CRUSH',
    subtitle: 'Lo-Fi Degradation',
    sliders: [
      { id: 1, name: 'Bit Depth', range: '4 to 24 bits', desc: 'Sample resolution' },
      { id: 2, name: 'Sample Rate', range: '1k to 48k Hz', desc: 'Downsample factor' },
      { id: 3, name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['8-bit retro sounds', 'Digital artifacts', 'Chiptune emulation'],
    properties: 'Quantization and sample rate reduction'
  },

  // --- STEREO/UTILITY ---
  'stereo-width': {
    title: 'STEREO WIDTH',
    subtitle: 'Mid/Side Spread',
    sliders: [
      { id: 1, name: 'Width', range: '0 to 200%', desc: 'Stereo image size' },
      { id: 2, name: 'Low Cut', range: '20 to 500 Hz', desc: 'Bass mono threshold' },
      { id: 3, name: 'High Cut', range: '500 to 20000 Hz', desc: 'Treble mono threshold' },
      { id: 4, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Widen synths', 'Monaural bass', 'Fix narrow recordings'],
    properties: 'Mid/Side manipulation'
  },
  'channel-balance': {
    title: 'CHANNEL BALANCE',
    subtitle: 'L/R Level Trim',
    sliders: [
      { id: 1, name: 'Left', range: '-6 to +6 dB', desc: 'Left channel trim' },
      { id: 2, name: 'Right', range: '-6 to +6 dB', desc: 'Right channel trim' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Correct mic imbalance', 'Fix stereo field', 'Fine-tune mixes'],
    properties: 'Independent channel gain'
  },
  'solo-mute': {
    title: 'SOLO MUTE',
    subtitle: 'Channel Isolator',
    sliders: [
      { id: 1, name: 'Mode', range: 'Mute L/Mute R/Solo L/Solo R', desc: 'Operation mode' },
      { id: 2, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Listen to single channel', 'Isolate problems', 'Check phase issues'],
    properties: 'Channel selection, not routing'
  },
  'pitch-shifter': {
    title: 'PITCH SHIFTER',
    subtitle: 'Harmony Generator',
    sliders: [
      { id: 1, name: 'Semitones', range: '-12 to +12', desc: 'Pitch shift interval' },
      { id: 2, name: 'Mix', range: '0 to 100%', desc: 'Effect level' },
      { id: 3, name: 'Bypass', range: '0 or 1', desc: 'Toggle plugin on/off' }
    ],
    uses: ['Create harmonies', 'Detune guitars', 'Pitch correction assist'],
    properties: 'Granular pitch shifting'
  }
};

// Open modal with guide content
function openGuide(pluginId) {
  const guide = guides[pluginId];
  if (!guide) {
    console.warn('No guide found for:', pluginId);
    return;
  }
  
  const modalBody = document.getElementById('modal-body');
  let html = `<div class="guide-header"><h2>${guide.title}</h2><p class="guide-subtitle">${guide.subtitle}</p></div>`;
  
  html += '<div class="guide-sliders"><h3>Controls</h3>';
  guide.sliders.forEach(slider => {
    html += `<div class="guide-slider"><strong>${slider.name}:</strong> ${slider.range}<br><small>${slider.desc}</small></div>`;
  });
  html += '</div>';
  
  html += '<div class="guide-uses"><h3>Best Uses</h3><ul>';
  guide.uses.forEach(use => {
    html += `<li>${use}</li>`;
  });
  html += '</ul></div>';
  
  html += `<div class="guide-properties"><strong>Properties:</strong> ${guide.properties}</div>`;
  
  modalBody.innerHTML = html;
  document.getElementById('guide-modal').classList.add('active');
}

// Close modal
function closeModal() {
  document.getElementById('guide-modal').classList.remove('active');
}

// Close on escape key
document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') {
    closeModal();
  }
});

// Close on outside click
document.getElementById('guide-modal').addEventListener('click', function(e) {
  if (e.target === this) {
    closeModal();
  }
});

// Install guide (global)
function openInstallGuide() {
  const modalBody = document.getElementById('modal-body');
  let html = `<div class="guide-header"><h2>Installation Guide</h2></div>`;
  
  html += '<div class="install-content">';
  html += '<p class="install-intro">After downloading, extract the <code>null-jsfx</code> folder to your Reaper Effects directory:</p>';
  html += '<dl class="install-paths">';
  html += '<div class="install-os"><dt>Windows</dt><dd><code>C:/Users/[Username]/AppData/Roaming/REAPER/Effects/null-jsfx/</code></dd></div>';
  html += '<div class="install-os"><dt>macOS</dt><dd><code>~/Library/Application Support/REAPER/Effects/null-jsfx/</code></dd></div>';
  html += '<div class="install-os"><dt>Linux</dt><dd><code>~/.config/REAPER/Effects/null-jsfx/</code></dd></div>';
  html += '</dl>';
  html += '<p class="install-note">Restart Reaper or press <strong>F5</strong> in the FX browser. NULL JSFX will appear under the <strong>JS</strong> category.</p>';
  html += '</div>';
  
  modalBody.innerHTML = html;
  document.getElementById('guide-modal').classList.add('active');
}

console.log('NULL JSFX Guides loaded:', Object.keys(guides).length, 'plugins');