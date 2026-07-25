const guides = {

  // ─── UTILITY ───
    'channel-utility': {
    title: 'CHANNEL UTILITY',
    subtitle: 'All-in-One Channel Tool',
    sliders: [
      { name: 'Input Pad', range: 'None / -6 / -12 dB', desc: 'Fixed input attenuation' },
      { name: 'Input Gain', range: '-12 to +18 dB', desc: 'Input level adjustment' },
      { name: 'Phase Left', range: 'Normal / Inverted', desc: 'Left channel polarity' },
      { name: 'Phase Right', range: 'Normal / Inverted', desc: 'Right channel polarity' },
      { name: 'DC Block Freq', range: '10 to 100 Hz', desc: 'DC offset filter cutoff' },
      { name: 'DC Block Mix', range: '0 to 100%', desc: 'Dry/wet blend for DC filter' },
      { name: 'Balance Left', range: '-6 to +6 dB', desc: 'Left channel trim' },
      { name: 'Balance Right', range: '-6 to +6 dB', desc: 'Right channel trim' },
      { name: 'Solo L', range: 'Off / On', desc: 'Isolate left channel (mutes right)' },
      { name: 'Solo R', range: 'Off / On', desc: 'Isolate right channel (mutes left)' },
      { name: 'Mono Check', range: 'Stereo / Mono', desc: 'Sum L+R to check phase cancellation' },
      { name: 'Output Gain', range: '-12 to +12 dB', desc: 'Final output level' },
      { name: 'Hard Limit', range: 'Off / On', desc: 'Clip protection at -0.09 dBFS' }
    ],
    uses: ['Gain staging at the start of a chain', 'Fix phase issues with multi-mic setups', 'Remove DC offset from recordings', 'Trim L/R channel imbalance', 'Check mono compatibility', 'Solo individual channels for monitoring'],
    properties: 'Signal chain: Pad → Gain → Phase → DC Block → Balance → Solo → Mono → Output → Hard Limit'
  },

  'stereo-width': {
    title: 'STEREO WIDTH',
    subtitle: 'Mid/Side Stereo Imaging',
    sliders: [
      { name: 'Width', range: '0 to 200%', desc: 'Stereo width (0=mono, 100=neutral, 200=super-wide)' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' },
      { name: 'Crossover', range: '0 to 500 Hz', desc: 'Frequency cutoff for selective widening (0=full range, 200Hz+=bass stays mono)' }
    ],
    uses: ['Widen mono sources for stereo placement', 'Narrow stereo tracks for mono compatibility', 'Enhance stereo imaging on mixes', 'Keep bass frequencies mono for tighter low-end', 'Create mid-side processing workflows'],
    properties: 'Signal chain: Input → Mid/Side Encoding → Crossover Split (optional) → Width Adjustment → Side/Mid Decoding → Mix → Output. Crossover at 0Hz applies width to full spectrum. Above 0Hz, low frequencies remain mono while only highs are widened. One-pole filter for frequency split.'
},

  // ─── EQ / FILTERS ───
    '5-band-parametric-eq': {
    title: '5-BAND PARAMETRIC EQ',
    subtitle: 'Flexible Multi-Filter EQ',
    sliders: [
      { name: 'Low Freq', range: '20–300 Hz', desc: 'Band 1 frequency center' },
      { name: 'Low Gain', range: '−18 to +18 dB', desc: 'Band 1 boost/cut' },
      { name: 'Low Q', range: '0.1–10', desc: 'Band 1 bandwidth' },
      { name: 'Low Type', range: '8 filter types', desc: 'Low Shelf, High Shelf, Peaking, LP, HP, Notch, BP, All Pass' },
      { name: 'Low Mode', range: 'Active/Bypass/Solo', desc: 'Band 1 operation mode' },
      { name: 'Low Mid Freq', range: '300–1000 Hz', desc: 'Band 2 frequency center' },
      { name: 'Low Mid Gain', range: '−18 to +18 dB', desc: 'Band 2 boost/cut' },
      { name: 'Low Mid Q', range: '0.1–10', desc: 'Band 2 bandwidth' },
      { name: 'Low Mid Type', range: '8 filter types', desc: 'Low Shelf, High Shelf, Peaking, LP, HP, Notch, BP, All Pass' },
      { name: 'Low Mid Mode', range: 'Active/Bypass/Solo', desc: 'Band 2 operation mode' },
      { name: 'Mid High Freq', range: '1000–5000 Hz', desc: 'Band 3 frequency center' },
      { name: 'Mid High Gain', range: '−18 to +18 dB', desc: 'Band 3 boost/cut' },
      { name: 'Mid High Q', range: '0.1–10', desc: 'Band 3 bandwidth' },
      { name: 'Mid High Type', range: '8 filter types', desc: 'Low Shelf, High Shelf, Peaking, LP, HP, Notch, BP, All Pass' },
      { name: 'Mid High Mode', range: 'Active/Bypass/Solo', desc: 'Band 3 operation mode' },
      { name: 'High Freq', range: '5000–20000 Hz', desc: 'Band 4 frequency center' },
      { name: 'High Gain', range: '−18 to +18 dB', desc: 'Band 4 boost/cut' },
      { name: 'High Q', range: '0.1–10', desc: 'Band 4 bandwidth' },
      { name: 'High Type', range: '8 filter types', desc: 'Low Shelf, High Shelf, Peaking, LP, HP, Notch, BP, All Pass' },
      { name: 'High Mode', range: 'Active/Bypass/Solo', desc: 'Band 4 operation mode' },
      { name: 'Ultra High Freq', range: '10000–24000 Hz', desc: 'Band 5 frequency center' },
      { name: 'Ultra High Gain', range: '−18 to +18 dB', desc: 'Band 5 boost/cut' },
      { name: 'Ultra High Q', range: '0.1–10', desc: 'Band 5 bandwidth' },
      { name: 'Ultra High Type', range: '8 filter types', desc: 'Low Shelf, High Shelf, Peaking, LP, HP, Notch, BP, All Pass' },
      { name: 'Ultra High Mode', range: 'Active/Bypass/Solo', desc: 'Band 5 operation mode' }
    ],
    uses: ['Surgical frequency corrections', 'Tonal shaping across the full spectrum', 'Mastering-grade EQ workflow', 'Per-band isolation for problem solving'],
    properties: 'RBJ biquad coefficients, 8 filter types per band, per-band Bypass/Solo, automatic sample-rate recalculation'
  },

  'filter': {
  title: 'FILTER',
  subtitle: 'TPT State-Variable Lowpass',
  sliders: [
    { name: 'Cutoff', range: '20 to 20000 Hz', desc: 'Filter cutoff frequency (logarithmic scale)' },
    { name: 'Resonance', range: '0.1 to 10', desc: 'Q factor - higher values increase resonance peak near cutoff' },
    { name: 'Gain', range: '−12 to +12 dB', desc: 'Output level adjustment' }
  ],
  uses: [
    'Clean lowpass filtering with resonance',
    'Remove high-frequency noise or harshness',
    'Create wah-like effects with high Q values',
    'Smooth transients while preserving dynamics',
    'Subtractive synthesis tone shaping'
  ],
  properties: 'TPT (Trapezoidal Integrator) State-Variable Filter implementation with zero-delay feedback correction. Formula: v3 = (input - ic2 - (R+g)*ic1) / denom. Pure lowpass output at 12dB/oct rolloff. Resonance controlled via Q = 1/(2*R) relationship.'
},

  // ─── SATURATION ───
  'tape-saturation': {
    title: 'TAPE SATURATION',
    subtitle: 'Vintage Warmth & Glue',
    sliders: [
      { name: 'Drive', range: '0 to 100%', desc: 'Saturation intensity' },
      { name: 'Tone', range: '−12 to +12 dB', desc: 'Tilt EQ: +treble/−bass or −treble/+bass' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Add vintage tape warmth to tracks', 'Glue mix elements together', 'Subtle harmonic enhancement on buses', 'Smooth transient peaks naturally'],
    properties: 'Signal chain: Input → Drive Gain → Polynomial Tanh Saturation → Makeup → Tilt EQ (1.5kHz crossover) → Mix → Output'
  },

  'harmonic-drive': {
    title: 'HARMONIC DRIVE',
    subtitle: 'Even-Order Exciter',
    sliders: [
      { name: 'Drive', range: '0 to 100%', desc: 'Harmonic blend amount' },
      { name: 'Tone', range: '−12 to +12 dB', desc: 'Tilt EQ: +treble/−bass or −treble/+bass' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Excite dull tracks with harmonics', 'Add tube-style warmth', 'Enhance presence and clarity', 'Parallel saturation processing'],
    properties: 'Signal chain: Input → Polynomial Tanh (2x) → Dry/Wet Blend → Tilt EQ (1.5kHz crossover) → Mix → Output'
  },

  'distortion': {
    title: 'DISTORTION',
    subtitle: 'Aggressive Dual-Stage Overdrive',
    sliders: [
      { name: 'Drive', range: '0 to 100%', desc: 'Pre-gain intensity (up to 50x)' },
      { name: 'Tone', range: '−12 to +12 dB', desc: 'Tilt EQ: +treble/−bass or −treble/+bass' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Aggressive guitar-style overdrive', 'Add grit and character to synths', 'Hard clip drum transients', 'Creative sound destruction'],
    properties: 'Signal chain: Input → Pre-Gain → Cubic Soft Clip → Hard Clip (0.8 threshold) → Tilt EQ (1.5kHz crossover) → Mix → Output'
  },

  'bit-crush': {
    title: 'BIT CRUSH',
    subtitle: 'Lo-Fi Quantization & Downsampling',
    sliders: [
      { name: 'Drive', range: '0 to 100%', desc: 'Pre-quantization gain' },
      { name: 'Bit Depth', range: '1 to 16 bits', desc: 'Quantization resolution' },
      { name: 'Sample Rate', range: '1k to 44.1kHz', desc: 'Downsampling frequency' },
      { name: 'Tone', range: '−12 to +12 dB', desc: 'Tilt EQ: +treble/−bass or −treble/+bass' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Lo-fi retro degradation', 'Drum bitcrushing for texture', 'Synth voice deterioration', 'Creative sample rate reduction effects'],
    properties: 'Signal chain: Input → Pre-Gain → Bit Quantization → Hold-and-Sample Downsampling → Tilt EQ (1.5kHz crossover) → Mix → Output'
  },

  // ─── TIME MODULATION ───
  'chorus': {
    title: 'CHORUS',
    subtitle: 'Stereo Thickening & Detune',
    sliders: [
      { name: 'Rate', range: '0.1 to 20 Hz', desc: 'LFO frequency' },
      { name: 'Depth', range: '0 to 10 ms', desc: 'Modulation delay depth' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Thicken vocals and guitars', 'Add stereo width to mono sources', 'Create lush ambient textures', 'Simulate ensemble playing'],
    properties: 'Signal chain: Input → LFO → Modulated Delay (3ms base) → Linear Interpolation → Mix → Output'
  },

  'flanger': {
    title: 'FLANGER',
    subtitle: 'Jet Sweep with Feedback',
    sliders: [
      { name: 'Rate', range: '0.1 to 5 Hz', desc: 'LFO frequency' },
      { name: 'Depth', range: '1 to 20 ms', desc: 'Modulation delay depth' },
      { name: 'Feedback', range: '0 to 100%', desc: 'Resonant feedback amount' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Classic jet-plane sweep effects', 'Add motion to static sounds', 'Create metallic resonances', 'Enhance drum overheads'],
    properties: 'Signal chain: Input → Feedback Loop → LFO → Modulated Delay (1ms base) → Mix → Output'
  },

  'phaser': {
    title: 'PHASER',
    subtitle: 'Moving Notch Sweep',
    sliders: [
      { name: 'Rate', range: '0.1 to 5 Hz', desc: 'LFO frequency' },
      { name: 'Stages', range: '1 to 8', desc: 'Allpass filter count' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Add swirling motion to guitars', 'Create psychedelic vocal effects', 'Enhance synth pads', 'Classic 70s style phase sweeping'],
    properties: 'Signal chain: Input → LFO → Cascaded Allpass Filters → Mix → Output'
  },

  'vibrato': {
    title: 'VIBRATO',
    subtitle: 'Pitch Modulation Wobble',
    sliders: [
      { name: 'Rate', range: '0.1 to 15 Hz', desc: 'LFO frequency' },
      { name: 'Depth', range: '0 to 20 ms', desc: 'Pitch deviation amount' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Add natural pitch variation', 'Create tape wow and flutter', 'Simulate string instrument vibrato', 'Subtle detune for thickness'],
    properties: 'Signal chain: Input → LFO → Modulated Delay (0ms base) → Linear Interpolation → Mix → Output'
  },

  'tremolo': {
    title: 'TREMOLO',
    subtitle: 'Rhythmic Amplitude Modulation',
    sliders: [
      { name: 'Rate', range: '0.1 to 20 Hz', desc: 'LFO frequency' },
      { name: 'Depth', range: '0 to 100%', desc: 'Amplitude variation depth' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Add rhythmic pulsing to guitars', 'Create vintage amp tremolo', 'Enhance ambient textures', 'Rhythmic gating effects'],
    properties: 'Signal chain: Input → LFO → Amplitude Modulation → Mix → Output'
  },

    'auto-pan': {
    title: 'AUTO PAN',
    subtitle: 'Stereo Movement & Width',
    sliders: [
      { name: 'Rate', range: '0.1 to 10 Hz', desc: 'LFO frequency (ignored when Tempo Sync ON)' },
      { name: 'Depth', range: '0 to 100%', desc: 'Pan excursion amount' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' },
      { name: 'Tempo Sync', range: 'OFF / ON', desc: 'Enable project tempo synchronization' },
      { name: 'Subdivision', range: '1/2 / 1/4 / 1/8 / 1/16', desc: 'Note division when Tempo Sync ON' }
    ],
    uses: ['Add stereo movement to mono sources', 'Create wide rhythmic effects synced to project tempo', 'Enhance static pad sounds', 'Simulate rotary speaker movement', 'LFO synchronized with drum patterns'],
    properties: 'Signal chain: Input → LFO → Complementary L/R Gain → Mix → Output. When Tempo Sync OFF: Rate slider controls Hz directly. When Tempo Sync ON: Rate calculated from project tempo × Subdivision (1/2=×0.5, 1/4=×1, 1/8=×2, 1/16=×4). Uses tempo variable (read-only in @block/@sample).'
  },

  'ring-mod': {
    title: 'RING MOD',
    subtitle: 'Metallic Carrier Modulation',
    sliders: [
      { name: 'Rate', range: '0.1 to 20 Hz', desc: 'Base modulation frequency' },
      { name: 'Carrier Freq', range: '100 to 2000 Hz', desc: 'Carrier oscillator frequency' },
      { name: 'Depth', range: '0 to 100%', desc: 'Modulation depth' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Create metallic bell tones', 'Generate alien dissonant textures', 'Sound design for sci-fi effects', 'Classic drum voice transformation'],
    properties: 'Signal chain: Input → Sine Carrier Oscillator → Signal Multiplication → Mix → Output'
  },

  // ─── DYNAMICS ───
  'gate': {
    title: 'GATE',
    subtitle: 'Noise & Vocal Gate',
    sliders: [
      { name: 'Threshold', range: '−60 to 0 dB', desc: 'Gate opening threshold' },
      { name: 'Attack', range: '1 to 100 ms', desc: 'Gate opening speed' },
      { name: 'Release', range: '50 to 1000 ms', desc: 'Gate closing speed' },
      { name: 'Sidechain HPF', range: '0 to 500 Hz', desc: 'High-pass filter for detection (0=full-range noise gate, 80Hz+=vocal gate)' }
    ],
    uses: ['Remove background noise between phrases', 'Clean up vocal recordings', 'Gate drum close-mics', 'Reduce headphone bleed on vocal takes'],
    properties: 'Signal chain: Input → Sidechain HPF Detection → Threshold Comparison → Attack/Release Smoothing → Gain → Output. Sidechain HPF at 0Hz = standard noise gate; raise to 80Hz+ for vocal-specific detection.'
  },

  'dynamics': {
    title: 'DYNAMICS',
    subtitle: 'Compressor & Expander',
    sliders: [
      { name: 'Mode', range: 'Compress / Expand', desc: 'Processing direction (Compress = reduce above threshold, Expand = reduce below threshold)' },
      { name: 'Threshold', range: '−60 to 0 dB', desc: 'Processing threshold' },
      { name: 'Ratio', range: '1 to 20', desc: 'Gain reduction ratio' },
      { name: 'Knee', range: '0 to 100%', desc: 'Soft knee width (0=hard, 100=very soft)' },
      { name: 'Attack', range: '0 to 100 ms', desc: 'Gain reduction onset speed' },
      { name: 'Release', range: '0 to 1000 ms', desc: 'Gain recovery speed' },
      { name: 'Makeup', range: '−20 to +20 dB', desc: 'Output compensation gain' }
    ],
    uses: ['Control dynamic range of vocals and instruments', 'Add punch to drums with compression', 'Reduce noise floor with expansion', 'Glue mix elements together', 'Parallel compression workflows'],
    properties: 'Signal chain: Input → Envelope Detection → Mode (Compress above threshold / Expand below threshold) → Ratio + Knee → Attack/Release Smoothing → Makeup → Output'
},

  'limiter': {
    title: 'LIMITER',
    subtitle: 'Limiter & Clipper & Brickwall',
    sliders: [
      { name: 'Mode', range: 'Limiter / Clipper / Brickwall', desc: 'Processing algorithm' },
      { name: 'Threshold', range: '−30 to 0 dB', desc: 'Level where limiting begins (Limiter & Clipper only — Brickwall uses Ceiling directly)' },
      { name: 'Ceiling', range: '−6 to −0.1 dB', desc: 'Absolute maximum output level' },
      { name: 'Release', range: '0 to 1000 ms', desc: 'Gain recovery speed (Limiter only — Clipper and Brickwall are instant)' },
      { name: 'Lookahead', range: '0 to 10 ms', desc: 'Pre-detection time for transparent peak catching (Limiter & Brickwall only)' }
    ],
    uses: [
      'Mastering peak protection with transparent limiting',
      'Drum bus crunch and harmonic saturation with Clipper',
      'Absolute brickwall ceiling for broadcast compliance',
      'Parallel limiting with dry/wet mix'
    ],
    properties: 'Three algorithms: Limiter (MGA-style envelope smoothing with auto makeup gain = ceiling/threshold, release-controlled recovery), Clipper (soft-clip curve above threshold with hard ceiling safety), Brickwall (instant gain reduction = ceiling/peak, zero overshoot). All modes apply hard clip at ceiling as safety. Makeup gain is automatic: ceiling ÷ threshold.'
},

  // ─── DELAY ───
  'delay': {
    title: 'DELAY',
    subtitle: 'Echo & Pre-Delay',
    sliders: [
      { name: 'Time', range: '0 to 2000 ms', desc: 'Delay time (short=pre-delay, long=echo)' },
      { name: 'Feedback', range: '0 to 100%', desc: 'Repeat decay (0=single slap, high=cascading echoes)' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend (100%=pre-delay mode)' }
    ],
    uses: ['Slapback echo for vocals and guitars', 'Pre-delay for reverb placement (short time, 0 feedback, 100% mix)', 'Cascading feedback delays', 'Stereo echo effects'],
    properties: 'Signal chain: Input → Circular Buffer → Read Position (Time) → Feedback Loop → Mix → Output. For pre-delay: set Time short (1-50ms), Feedback=0, Mix=100%'
  },

  // ─── SPECIALIZED ───
  'click-repair': {
    title: 'CLICK REPAIR',
    subtitle: 'Audio Restoration',
    sliders: [
      { name: 'Threshold', range: '−60 to 0 dB', desc: 'Click detection sensitivity' },
      { name: 'Window Size', range: '1 to 20 samples', desc: 'Interpolation window' }
    ],
    uses: ['Remove vinyl crackle and pops', 'Clean up digital click artifacts', 'Repair corrupted audio samples', 'Restore archived recordings'],
    properties: 'Signal chain: Input → Click Detection → Adaptive Interpolation → Output'
  },

  'deesser': {
    title: 'DEESSER',
    subtitle: 'Sibilance Reduction',
    sliders: [
      { name: 'Threshold', range: '−60 to 0 dB', desc: 'Sibilance detection level' },
      { name: 'Frequency', range: '2000 to 12000 Hz', desc: 'Sibilance center frequency' },
      { name: 'Bandwidth', range: '0.1 to 5 octaves', desc: 'Detection range width' },
      { name: 'Reduction', range: '0 to 24 dB', desc: 'Maximum sibilance attenuation' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Tame harsh vocal sibilants (S, T, Ch)', 'Reduce cymbal bleed on close-mic tracks', 'Smooth high-frequency harshness', 'Dynamic frequency-specific compression'],
    properties: 'Signal chain: Input → Bandpass Detection (Freq + Bandwidth) → Threshold Comparison → Gain Reduction (up to Reduction dB) → Mix → Output'
  },

  'pitch-shifter': {
    title: 'PITCH SHIFTER',
    subtitle: 'Real-Time Pitch Shift',
    sliders: [
      { name: 'Octave', range: '−2 to +2', desc: 'Octave transposition (−2=two octaves down, +2=two octaves up)' },
      { name: 'Semitones', range: '−12 to +12', desc: 'Semitone transposition within the octave' },
      { name: 'Fine', range: '−50 to +50 cents', desc: 'Microtuning offset for precise pitch adjustment' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Harmony generation', 'Octave doubling effects', 'Corrective pitch adjustment', 'Creative sound design'],
    properties: 'Signal chain: Input → Circular Buffer → Variable Read Rate → Linear Interpolation → Mix → Output. Total pitch shift = (Octave × 12) + Semitones + (Fine / 100). Uses delay-based pitch shifting with linear interpolation.'
},
  'reverb': {
    title: 'REVERB',
    subtitle: 'Space & Ambience',
    sliders: [
      { name: 'Size', range: '0 to 100%', desc: 'Room/space size' },
      { name: 'Decay', range: '0.1 to 10 s', desc: 'Reverb tail length' },
      { name: 'Predelay', range: '0 to 200 ms', desc: 'Initial delay before reflections' },
      { name: 'Damping', range: '0 to 100%', desc: 'High-frequency absorption' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Add natural room ambience', 'Create lush hall spaces', 'Simulate plate reverb for vocals', 'Enhance stereo depth on dry tracks'],
    properties: 'Signal chain: Input → Predelay → Diffusion Network → Schroeder Allpass Filters → Feedback Delay Network → Damping → Mix → Output'
  },

  'transient-shaper': {
    title: 'TRANSIENT SHAPER',
    subtitle: 'Attack & Sustain Control',
    sliders: [
      { name: 'Attack', range: '−20 to +20 dB', desc: 'Transient emphasis/reduction' },
      { name: 'Sustain', range: '−20 to +20 dB', desc: 'Tail emphasis/reduction' },
      { name: 'Mix', range: '0 to 100%', desc: 'Dry/wet blend' }
    ],
    uses: ['Add punch to drum transients', 'Reduce room bleed on drum tracks', 'Emphasize pluck on guitar attacks', 'Enhance sustain on sustained notes'],
    properties: 'Signal chain: Input → Transient Detection → Attack Envelope Gain → Sustain Envelope Gain → Mix → Output. Threshold-free design: operates on all material dynamically.'
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