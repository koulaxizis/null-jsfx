const chainGuides = {

  'vocal-chain': {
    title: 'VOCAL CHAIN',
    subtitle: 'Complete vocal processing',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Set optimal input level before processing' },
      { plugin: 'VOCAL GATE', role: 'Silence background noise between phrases' },
      { plugin: 'DE-ESSER', role: 'Tame harsh sibilance (S, T sounds)' },
      { plugin: 'COMPRESSOR', role: 'Even out dynamic range for consistent level' },
      { plugin: 'CLIPPER', role: 'Catch peaks transparently before tone shaping' },
      { plugin: 'MID PRESENCE', role: 'Bring vocal forward in the mix' },
      { plugin: 'AIR BOOST', role: 'Add sparkle and openness on top' },
      { plugin: 'CEILING LIMIT', role: 'Brickwall protection against clipping' }
    ],
    uses: [
      'Lead and backing vocal tracks',
      'Voice-over and narration recordings',
      'Any vocal needing full polish: gate → dynamics → tone → protection'
    ],
    tips: 'Set the Vocal Gate threshold first — too high and you lose breaths, too low and noise bleeds through. The Clipper before EQ prevents the Mid Presence and Air Boost from causing inter-sample peaks.'
  },

  'drum-chain': {
    title: 'DRUM CHAIN',
    subtitle: 'Punchy drum bus',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Normalize drum bus input level' },
      { plugin: 'LOW CUT', role: 'Remove subsonic rumble and bleed' },
      { plugin: 'TRANSIENT SHAPER', role: 'Enhance attack for punchy drums' },
      { plugin: 'COMPRESSOR', role: 'Glue the drum bus together' },
      { plugin: 'SUB BOOST', role: 'Add weight to kick and toms' },
      { plugin: 'CLIPPER', role: 'Tame transient peaks transparently' },
      { plugin: 'CEILING LIMIT', role: 'Final protection for the bus output' }
    ],
    uses: [
      'Drum bus / drum group processing',
      'Individual drum tracks needing punch',
      'Rock, pop, and electronic drum bus glue'
    ],
    tips: 'Push the Transient Shaper attack slightly positive for snappy kicks. Keep Compressor ratio moderate (2:1–4:1) to preserve dynamics. Use Sub Boost sparingly on full kits — too much mud.'
  },

  'guitar-chain': {
    title: 'GUITAR CHAIN',
    subtitle: 'Rich tone &amp; modulation',
    flow: [
      { plugin: 'LOW CUT', role: 'Remove mud and low-frequency buildup' },
      { plugin: 'NOTCH FILTER', role: 'Eliminate resonant peaks or hum' },
      { plugin: 'DISTORTION', role: 'Add grit and character to clean tones' },
      { plugin: 'HARMONIC DRIVE', role: 'Enhance warmth and harmonic content' },
      { plugin: 'PHASER', role: 'Add sweeping movement and texture' },
      { plugin: 'DELAY', role: 'Create space without washing out the tone' },
      { plugin: 'AIR BOOST', role: 'Restore brightness lost to distortion' }
    ],
    uses: [
      'Electric guitar DI or amp sim tracks',
      'Acoustic guitar enhancement',
      'Creative sound design with string instruments'
    ],
    tips: 'Order matters: filters before distortion ensures clean harmonics. The Phaser adds vintage vibe — lower rates (0.5–1 Hz) sound most musical. Air Boost compensates for high-end loss from the Distortion stage.'
  },

  'master-bus': {
    title: 'MASTER BUS',
    subtitle: 'Professional mastering',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Set target input level for mastering' },
      { plugin: '3-BAND EQ', role: 'Balance low, mid, and high frequencies' },
      { plugin: 'WIDE CUT', role: 'Remove mud and boxiness from the mix' },
      { plugin: 'COMPRESSOR', role: 'Glue mix together with gentle compression' },
      { plugin: 'STEREO WIDTH', role: 'Enhance stereo image, keep bass mono' },
      { plugin: 'CLIPPER', role: 'Gain loudness by clipping transient peaks' },
      { plugin: 'CEILING LIMIT', role: 'True-peak brickwall for delivery format' }
    ],
    uses: [
      'Master bus for final mix delivery',
      'Pre-mastering preparation',
      'Streaming-ready loudness targeting'
    ],
    tips: 'Keep Compressor ratio low (1.5:1–2:1) for transparent glue. Stereo Width low-cut at ~100 Hz keeps bass mono-compatible. Set Ceiling Limit to -1.0 dBTP for streaming platforms, -0.3 dBTP for CD.'
  },

  'cleaning-chain': {
    title: 'CLEANING CHAIN',
    subtitle: 'Audio restoration',
    flow: [
      { plugin: 'CLICK REPAIR', role: 'Remove clicks, pops, and digital glitches' },
      { plugin: 'DC BLOCK', role: 'Eliminate DC offset from the signal' },
      { plugin: 'LOW CUT', role: 'Remove subsonic rumble and handling noise' },
      { plugin: 'HIGH CUT', role: 'Reduce hiss and high-frequency noise' },
      { plugin: 'NOTCH FILTER', role: 'Target specific hum (50/60 Hz) or ringing' },
      { plugin: 'NOISE GATE', role: 'Silence background noise between audio' }
    ],
    uses: [
      'Restoring old or damaged recordings',
      'Cleaning field recordings and podcasts',
      'Vinyl rip restoration and digitization'
    ],
    tips: 'Process in order — clicks first, then DC, then frequency cleanup. Click Repair threshold too aggressive causes artifacts; start conservative. High Cut above 12 kHz to preserve intelligibility. Notch at 50 Hz (EU) or 60 Hz (US) for electrical hum.'
  },

  'podcast-chain': {
    title: 'PODCAST CHAIN',
    subtitle: 'Broadcast voice processing',
    flow: [
      { plugin: 'DC BLOCK', role: 'Clean up any DC offset from interface' },
      { plugin: 'VOCAL GATE', role: 'Remove room noise and breathing gaps' },
      { plugin: '3-BAND EQ', role: 'Shape tone: cut mud, boost clarity' },
      { plugin: 'DE-ESSER', role: 'Control harsh sibilance in speech' },
      { plugin: 'COMPRESSOR', role: 'Even out speech dynamics for consistency' },
      { plugin: 'MID PRESENCE', role: 'Ensure voice cuts through on small speakers' },
      { plugin: 'CLIPPER', role: 'Catch peaks for louder output without pumping' }
    ],
    uses: [
      'Podcast production and post-processing',
      'YouTube voice-over processing',
      'Audiobook narration cleanup and enhancement'
    ],
    tips: 'Vocal Gate with moderate range (-20 to -30 dB) sounds natural — full gating sounds choppy. Compressor: fast attack (5–10 ms), moderate ratio (3:1). Mid Presence around 2–3 kHz improves phone speaker intelligibility.'
  },

  'bass-chain': {
    title: 'BASS CHAIN',
    subtitle: 'Tight low-end &amp; saturation',
    flow: [
      { plugin: 'DC BLOCK', role: 'Remove any DC offset in low frequencies' },
      { plugin: 'LOW CUT', role: 'Remove unnecessary subsonic content' },
      { plugin: 'LADDER FILTER', role: 'Shape tone with warm analog-style filtering' },
      { plugin: 'COMPRESSOR', role: 'Lock bass dynamics for consistent groove' },
      { plugin: 'SUB BOOST', role: 'Add weight and sub-bass extension' },
      { plugin: 'CLIPPER', role: 'Tame peaks and add harmonic saturation' }
    ],
    uses: [
      'Bass guitar tracking and mixing',
      'Synth bass processing and enhancement',
      'Any low-frequency instrument needing control'
    ],
    tips: 'Low Cut at 30–40 Hz removes rumble without thinning the bass. Ladder Filter adds Moog-style warmth — try low-pass at 1–3 kHz. Compressor: slower attack (10–30 ms) preserves transient pick attack. Sub Boost below 60 Hz for felt-not-heard weight.'
  },

  'lofi-chain': {
    title: 'LOFI CHAIN',
    subtitle: 'Vintage character &amp; flutter',
    flow: [
      { plugin: 'LOW CUT', role: 'Remove modern sub-bass clarity for vintage feel' },
      { plugin: 'HIGH CUT', role: 'Dull highs for tape/cassette character' },
      { plugin: 'BIT CRUSH', role: 'Reduce bit depth for digital degradation' },
      { plugin: 'TAPE SATURATION', role: 'Add warm analog harmonic distortion' },
      { plugin: 'VIBRATO', role: 'Simulate tape wow and flutter' },
      { plugin: 'CHORUS', role: 'Thicken and widen the degraded signal' }
    ],
    uses: [
      'Lo-fi hip-hop and chillwave production',
      'Vintage cassette or tape emulation',
      'Creative degradation of clean digital sources'
    ],
    tips: 'High Cut around 6–8 kHz sells the tape illusion. Bit Crush at 12 bits adds grain without destroying the signal. Vibrato rate slow (0.5–2 Hz) with shallow depth mimics tape flutter. Tape Saturation drive at 30–50% is the sweet spot.'
  },

  'widener-chain': {
    title: 'WIDENER CHAIN',
    subtitle: 'Stereo movement &amp; width',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Set input level before stereo processing' },
      { plugin: 'CHORUS', role: 'Thicken and create stereo detuning' },
      { plugin: 'STEREO WIDTH', role: 'Expand stereo image beyond original' },
      { plugin: 'AUTO PAN', role: 'Add rhythmic left-right movement' },
      { plugin: 'PHASER', role: 'Create sweeping stereo textures' },
      { plugin: 'AIR BOOST', role: 'Restore high-end sparkle after modulation' },
      { plugin: 'CEILING LIMIT', role: 'Protect widened signal from peaks' }
    ],
    uses: [
      'Synth pads and ambient textures',
      'Background vocals and guitar ambiences',
      'Any stereo source needing width and movement'
    ],
    tips: 'Auto Pan rate synced to tempo (0.5–2 Hz) feels musical. Stereo Width above 150% can cause mono-compatibility issues — always check with MONO CHECK. Air Boost compensates for high-end loss from Chorus and Phaser modulation.'
  },

  'ambient-chain': {
    title: 'AMBIENT CHAIN',
    subtitle: 'Atmospheric textures',
    flow: [
      { plugin: 'PRE DELAY', role: 'Separate dry signal from reverb tail' },
      { plugin: 'REVERB', role: 'Create large atmospheric space' },
      { plugin: 'CHORUS', role: 'Add detuned thickness and movement' },
      { plugin: 'DELAY', role: 'Create cascading echoes and space' },
      { plugin: 'VIBRATO', role: 'Add subtle pitch movement and life' },
      { plugin: 'STEREO WIDTH', role: 'Push textures wide across the field' }
    ],
    uses: [
      'Ambient music and drone production',
      'Sound design for film and games',
      'Creating atmospheric beds from any source'
    ],
    tips: 'Reverb decay long (60–80%) with high damping for warm tails. Pre Delay at 20–40 ms keeps the source audible inside the reverb. Delay with high feedback (60–80%) creates evolving textures. Vibrato depth shallow (3–5 cents) adds organic life without obvious pitch bends.'
  },

  'punch-chain': {
    title: 'PUNCH CHAIN',
    subtitle: 'Transient impact enhancement',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Optimize input level for transient detection' },
      { plugin: 'TRANSIENT SHAPER', role: 'Boost attack for maximum punch' },
      { plugin: 'COMPRESSOR', role: 'Control dynamics while preserving transients' },
      { plugin: 'CLIPPER', role: 'Shave peaks for louder, punchier output' },
      { plugin: 'SUB BOOST', role: 'Reinforce low-frequency impact' },
      { plugin: 'CEILING LIMIT', role: 'Final brickwall for delivery' }
    ],
    uses: [
      'Drum buses needing aggressive punch',
      'Percussion and loop enhancement',
      'Any source needing more attack and energy'
    ],
    tips: 'Transient Shaper: attack +30 to +50, sustain slightly negative (-10 to -20) for tight punch. Compressor attack slow (15–30 ms) lets transients through. Sub Boost at 40–60 Hz adds chest-thumping weight without muddying the midrange.'
  },

  'room-chain': {
    title: 'ROOM CHAIN',
    subtitle: 'Natural space simulation',
    flow: [
      { plugin: 'PRE DELAY', role: 'Create perceived distance from source' },
      { plugin: 'REVERB', role: 'Simulate acoustic room characteristics' },
      { plugin: '3-BAND EQ', role: 'Shape reverb tone — darken tails, brighten early reflections' },
      { plugin: 'COMPRESSOR', role: 'Control reverb dynamics for consistent ambience' },
      { plugin: 'AIR BOOST', role: 'Restore high-frequency air to the space' },
      { plugin: 'SOFT LIMIT', role: 'Transparent ceiling for natural dynamics' }
    ],
    uses: [
      'Adding realistic room ambience to dry recordings',
      'Post-production dialogue enhancement',
      'Acoustic instrument space simulation'
    ],
    tips: 'Reverb room size 30–50% sounds like a real studio space. Pre Delay at 10–20 ms separates source from room. 3-BAND EQ: cut lows to avoid muddy reverb, gently boost highs for air. Soft Limit keeps the reverb tail controlled without sounding squashed.'
  },

  'vinyl-chain': {
    title: 'VINYL CHAIN',
    subtitle: 'Vintage record character',
    flow: [
      { plugin: 'CLICK REPAIR', role: 'Remove vinyl crackles and pops' },
      { plugin: 'LOW CUT', role: 'Remove turntable rumble' },
      { plugin: 'HIGH CUT', role: 'Replicate limited vinyl frequency response' },
      { plugin: 'TAPE SATURATION', role: 'Add warm analog coloration' },
      { plugin: 'BIT CRUSH', role: 'Simulate vintage digital or sample degradation' },
      { plugin: 'MONO CHECK', role: 'Verify mono compatibility for authentic vinyl' }
    ],
    uses: [
      'Vinyl rip restoration or enhancement',
      'Creating authentic vintage record aesthetics',
      'Lo-fi production requiring vinyl character'
    ],
    tips: 'Click Repair at moderate threshold preserves intentional crackle while removing harsh pops. Low Cut at 40 Hz removes rumble. High Cut at 10–12 kHz mimics vinyl frequency limitations. Mono Check at the end ensures authentic mono-bottom vinyl behavior.'
  },

  'lead-vocal-chain': {
    title: 'LEAD VOCAL CHAIN',
    subtitle: 'Front-and-center vocal processing',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Set optimal level for detailed processing' },
      { plugin: 'DC BLOCK', role: 'Clean any offset from preamps' },
      { plugin: 'VOCAL GATE', role: 'Isolate vocal from background' },
      { plugin: 'NOTCH FILTER', role: 'Remove room resonances and mic coloration' },
      { plugin: 'DE-ESSER', role: 'Control sibilance before compression' },
      { plugin: 'COMPRESSOR', role: 'Tight dynamics for lead vocal presence' },
      { plugin: 'MID PRESENCE', role: 'Carve space for vocal in dense mix' },
      { plugin: 'AIR BOOST', role: 'Add modern sheen and breath' },
      { plugin: 'CLIPPER', role: 'Catch peaks for louder vocal level' },
      { plugin: 'CEILING LIMIT', role: 'Guarantee no clipping on vocal bus' }
    ],
    uses: [
      'Lead vocal in dense modern mixes',
      'Pop, rock, and R&amp;B vocal production',
      'Any vocal needing to sit prominently upfront'
    ],
    tips: 'Notch Filter narrow Q at problematic resonant frequencies (usually 200–500 Hz). De-Esser before Compressor prevents sibilance from triggering compression. Mid Presence at 2–4 kHz creates "forward" vocal. Air Boost at 12 kHz+ adds modern breath and openness.'
  },

  'synth-chain': {
    title: 'SYNTH CHAIN',
    subtitle: 'Synth processing &amp; movement',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Normalize synth input level' },
      { plugin: 'LADDER FILTER', role: 'Shape tone with classic analog filtering' },
      { plugin: 'CHORUS', role: 'Thicken single-oscillator sounds' },
      { plugin: 'PHASER', role: 'Add movement and evolving texture' },
      { plugin: 'DELAY', role: 'Create rhythmic echoes and space' },
      { plugin: 'STEREO WIDTH', role: 'Widen synth to fill the stereo field' },
      { plugin: 'CEILING LIMIT', role: 'Protect output from modulation peaks' }
    ],
    uses: [
      'Synth pad and lead processing',
      'Electronic music production',
      'Transforming simple synth tones into rich textures'
    ],
    tips: 'Ladder Filter resonance adds classic acid character — push for squelchy tones. Chorus rate at 0.5–1 Hz for slow, evolving thickness. Phaser and Delay create movement — sync delay time to tempo for rhythmic results. Stereo Width at 130–160% fills the field without mono issues.'
  },

  'parallel-comp-chain': {
    title: 'PARALLEL COMP CHAIN',
    subtitle: 'New York style compression',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Set input for parallel blend' },
      { plugin: 'TRANSIENT SHAPER', role: 'Preserve attack on parallel path' },
      { plugin: 'COMPRESSOR', role: 'Heavy compression on parallel signal' },
      { plugin: 'SUB BOOST', role: 'Reinforce low end on compressed path' },
      { plugin: 'AIR BOOST', role: 'Restore brightness on parallel path' },
      { plugin: 'CLIPPER', role: 'Control parallel path peaks' },
      { plugin: 'CEILING LIMIT', role: 'Final protection on blended output' }
    ],
    uses: [
      'Drum bus New York compression technique',
      'Parallel vocal processing for upfront sound',
      'Any source needing density without losing transients'
    ],
    tips: 'Set Compressor aggressively on the parallel path: high ratio (8:1+), fast attack, heavy gain reduction. Blend the parallel signal underneath the dry — 20–40% mix is typical. The Transient Shaper on the parallel path ensures the blended result retains punch.'
  },

  'modulation-chain': {
    title: 'MODULATION CHAIN',
    subtitle: 'Movement &amp; depth effects',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Set input level for modulation chain' },
      { plugin: 'CHORUS', role: 'Add detuned thickness and width' },
      { plugin: 'PHASER', role: 'Create sweeping notches and movement' },
      { plugin: 'FLANGER', role: 'Add dramatic jet-sweep character' },
      { plugin: 'AUTO PAN', role: 'Rhythmic left-right oscillation' },
      { plugin: 'VIBRATO', role: 'Subtle pitch modulation for organic feel' }
    ],
    uses: [
      'Creative sound design and experimental effects',
      'Synth and guitar modulation stacking',
      'Transforming static sounds into evolving textures'
    ],
    tips: 'Stack modulators carefully — too many rates clash. Try syncing: Chorus and Phaser at similar rates, Flanger faster, Auto Pan tempo-synced. Vibrato depth very shallow (2–4 cents) adds life without obvious detuning. Less is more with stacked modulation.'
  },

  'saturation-chain': {
    title: 'SATURATION CHAIN',
    subtitle: 'Harmonic distortion stack',
    flow: [
      { plugin: 'GAIN STAGE', role: 'Drive signal into saturation stages' },
      { plugin: 'TAPE SATURATION', role: 'Add warm analog tape harmonics' },
      { plugin: 'HARMONIC DRIVE', role: 'Enhance even-order harmonics for warmth' },
      { plugin: 'DISTORTION', role: 'Add aggressive grit and character' },
      { plugin: 'BIT CRUSH', role: 'Digital degradation for lo-fi texture' },
      { plugin: 'CLIPPER', role: 'Final peak control and loudness boost' }
    ],
    uses: [
      'Adding harmonic richness to sterile digital tracks',
      'Creative distortion and sound design',
      'Bus saturation for glue and character'
    ],
    tips: 'Each stage adds harmonics — keep drive moderate on each (30–50%) to avoid cumulative mud. Order matters: Tape (warmth) → Harmonic Drive (exciter) → Distortion (grit) → Bit Crush (texture). Clipper at the end catches cumulative peaks from all saturation stages.'
  },

  'broadcast-chain': {
    title: 'BROADCAST CHAIN',
    subtitle: 'Radio-ready voice processing',
    flow: [
      { plugin: 'DC BLOCK', role: 'Remove DC offset from broadcast signal' },
      { plugin: 'NOISE GATE', role: 'Silence background noise aggressively' },
      { plugin: 'VOCAL GATE', role: 'Fine-tuned voice-specific gating' },
      { plugin: 'DE-ESSER', role: 'Control sibilance for broadcast clarity' },
      { plugin: 'COMPRESSOR', role: 'Heavy compression for consistent broadcast level' },
      { plugin: 'MID PRESENCE', role: 'Ensure intelligibility on all speakers' },
      { plugin: 'CLIPPER', role: 'Maximize loudness for broadcast standards' },
      { plugin: 'CEILING LIMIT', role: 'Strict brickwall for transmission safety' }
    ],
    uses: [
      'Radio broadcasting and streaming',
      'Live voice processing for events',
      'Podcast and video production requiring broadcast loudness'
    ],
    tips: 'Compressor: aggressive settings (ratio 4:1–6:1, fast attack 3–5 ms) for the consistent level broadcast demands. Noise Gate threshold higher than podcast chain — broadcast tolerates less background noise. Ceiling Limit at -1.0 dBTP for streaming, -0.3 dBTP for traditional broadcast.'
  },

  'creative-fx-chain': {
    title: 'CREATIVE FX CHAIN',
    subtitle: 'Experimental sound design',
    flow: [
      { plugin: 'FILTER', role: 'Shape frequency content before effects' },
      { plugin: 'RING MOD', role: 'Create metallic and inharmonic tones' },
      { plugin: 'FLANGER', role: 'Add dramatic sweeping movement' },
      { plugin: 'DELAY', role: 'Create rhythmic and atmospheric echoes' },
      { plugin: 'REVERB', role: 'Place processed sound in abstract space' },
      { plugin: 'PITCH SHIFTER', role: 'Create harmonies or alien textures' }
    ],
    uses: [
      'Sound design for film, games, and experimental music',
      'Transforming ordinary sources into unrecognizable textures',
      'Creative FX experimentation and discovery'
    ],
    tips: 'Ring Mod with carrier at non-harmonic intervals (e.g., 137 Hz, 311 Hz) creates unsettling metallic tones. Filter in bandpass mode isolates specific frequencies before Ring Mod. Pitch Shifter at ±7 semitones creates alien textures. Delay with 75%+ feedback creates evolving ambient washes.'
  }

};

function openChainGuide(chainId) {
  const guide = chainGuides[chainId];
  if (!guide) {
    console.warn('No chain guide found for:', chainId);
    return;
  }

  const modalBody = document.getElementById('modal-body');
  let html = '<div class="guide-header"><h3>' + guide.title + '</h3><p>' + guide.subtitle + '</p></div>';

  html += '<div class="guide-body">';

  // Signal Flow section (replaces Sliders)
  html += '<div class="guide-section"><h4>Signal Flow</h4><ul class="guide-sliders">';
  guide.flow.forEach(function(step) {
    html += '<li><span class="slider-name">' + step.plugin + '</span><span class="slider-desc">' + step.role + '</span></li>';
  });
  html += '</ul></div>';

  // Best Uses section
  html += '<div class="guide-section"><h4>Best Uses</h4><ul class="use-cases">';
  guide.uses.forEach(function(use) {
    html += '<li>' + use + '</li>';
  });
  html += '</ul></div>';

  // Tips section (replaces Properties)
  html += '<div class="guide-section"><h4>Tips</h4><p style="font-size:12px;color:#999;">' + guide.tips + '</p></div>';

  html += '</div>';

  modalBody.innerHTML = html;
  document.getElementById('guide-modal').classList.add('active');
}

// Close handler reuses existing closeModal() from modals.js
// Keyboard ESC and overlay click already handled by modals.js

console.log('NULL JSFX Chain Guides loaded:', Object.keys(chainGuides).length, 'chains');