const guides = {
    'gain-stage': {
        title: 'GAIN STAGE',
        desc: 'Clean Gain & Boost',
        sliders: [
            { name: 'Input-Gain-dB', desc: '-12 to +18dB — Adjust incoming signal level' },
            { name: 'Output-Gain-dB', desc: '-12 to +12dB — Final trim after processing' },
            { name: 'Clip-Indicator', desc: 'Soft limiting ON/OFF for peak protection' },
            { name: 'Input-Pad', desc: 'None / -6dB / -12dB for hot signals' }
        ],
        useCases: [
            { title: 'Quiet vocal', desc: 'Input +12dB, Clip ON' },
            { title: 'Hot synth', desc: 'Pad -6dB, Input 0dB' },
            { title: 'Gain staging', desc: 'Input +6dB, Output -3dB' }
        ]
    },
    'phase-flip': {
        title: 'PHASE FLIP',
        desc: 'Polarity Inverter',
        sliders: [
            { name: 'Left Channel', desc: 'Normal / Inverted polarity' },
            { name: 'Right Channel', desc: 'Normal / Inverted polarity' }
        ],
        useCases: [
            { title: 'Kick multi-mic', desc: 'Flip kick-out phase for alignment' },
            { title: 'DI vs Amp', desc: 'Both inverted after reamp' },
            { title: 'Stereo pair', desc: 'Flip one mic if wired backwards' }
        ]
    },
    'dc-block': {
        title: 'DC BLOCK',
        desc: 'Offset Remover',
        sliders: [
            { name: 'Freq-(Hz)', desc: '10–100 Hz cutoff point' },
            { name: 'Mix-%', desc: '0–100% wet/dry blend' }
        ],
        useCases: [
            { title: 'Analog gear DC bias', desc: '20 Hz, 100%' },
            { title: 'Vinyl transfers', desc: '30 Hz, 100%' }
        ]
    },
    'mono-check': {
        title: 'MONO CHECK',
        desc: 'Compatibility Test',
        sliders: [
            { name: 'Mode', desc: 'Stereo / Mono toggle' },
            { name: 'Trim-(dB)', desc: '-12 to +12dB output trim' }
        ],
        useCases: [
            { title: 'Check mono collapse', desc: 'Switch to Mono, listen for phase cancellation' },
            { title: 'Club playback test', desc: 'Many clubs play in mono — verify compatibility' }
        ]
    },
    'low-cut': {
        title: 'LOW CUT',
        desc: 'High-Pass Filter',
        sliders: [
            { name: 'Cutoff-(Hz)', desc: '20–500 Hz cutoff point' },
            { name: 'Bypass', desc: 'OFF / ON to compare' }
        ],
        useCases: [
            { title: 'Remove rumble from guitars', desc: '80 Hz, bypass to compare' },
            { title: 'Clean up vocals', desc: '100 Hz to remove mic stand vibration' }
        ]
    },
    'high-cut': {
        title: 'HIGH CUT',
        desc: 'Low-Pass Filter',
        sliders: [
            { name: 'Cutoff-(Hz)', desc: '5kHz–20kHz cutoff point' },
            { name: 'Bypass', desc: 'OFF / ON to compare' }
        ],
        useCases: [
            { title: 'Tame harsh cymbals', desc: '12 kHz, 100% mix' },
            { title: 'Remove hiss from old recordings', desc: '15 kHz cleanup' }
        ]
    },
    'click-repair': {
        title: 'CLICK REPAIR',
        desc: 'Artifact Remover',
        sliders: [
            { name: 'Threshold-%', desc: '0–100% detection sensitivity' },
            { name: 'Window-Samples', desc: '1–50 samples analysis window' }
        ],
        useCases: [
            { title: 'Vinyl crackles', desc: 'Threshold 50%, Window 10' },
            { title: 'Digital glitches', desc: 'Threshold 30%, Window 5' }
        ]
    },
    'solo-mute': {
        title: 'SOLO MUTE',
        desc: 'Channel Isolator',
        sliders: [
            { name: 'Mute-L', desc: 'Off / On — mute left channel' },
            { name: 'Mute-R', desc: 'Off / On — mute right channel' },
            { name: 'Mode', desc: 'Stereo (mute) / Solo (listen to one)' }
        ],
        useCases: [
            { title: 'Check L/R balance', desc: 'Solo L, then R to compare levels' }
        ]
    },
    'notch-filter': {
        title: 'NOTCH FILTER',
        desc: 'Surgical EQ',
        sliders: [
            { name: 'Freq-(Hz)', desc: '20–20000 Hz target frequency' },
            { name: 'Gain-(dB)', desc: '-18 to +18dB boost or cut' },
            { name: 'Q-Factor', desc: '1–100 bandwidth (higher = narrower)' }
        ],
        useCases: [
            { title: 'Remove hum at 50Hz', desc: '50 Hz, -12dB, Q 20' }
        ]
    },
    'pre-delay': {
        title: 'PRE DELAY',
        desc: 'Short Thickening',
        sliders: [
            { name: 'Time-(ms)', desc: '1–50 ms delay time' },
            { name: 'Mix-%', desc: '0–100% wet/dry blend' }
        ],
        useCases: [
            { title: 'Vocal thickening', desc: '10 ms, 30% mix' },
            { title: 'Guitar body', desc: '5 ms, 20% mix' }
        ]
    },
    'harmonic-drive': {
        title: 'HARMONIC DRIVE',
        desc: 'Warmth Exciter',
        sliders: [
            { name: 'Amount-%', desc: '0–50% saturation intensity' },
            { name: 'Tone-(dB)', desc: '-6 to +6dB tone adjustment' }
        ],
        useCases: [
            { title: 'Add analog warmth to synths', desc: '30% amount, 0 dB' }
        ]
    },
    'air-boost': {
        title: 'AIR BOOST',
        desc: 'High Shelf Sparkle',
        sliders: [
            { name: 'Freq-(Hz)', desc: '5kHz–20kHz shelf start' },
            { name: 'Gain-(dB)', desc: '-12 to +12dB boost' }
        ],
        useCases: [
            { title: 'Vocal air', desc: '12 kHz, +6 dB' },
            { title: 'Cymbal sparkle', desc: '15 kHz, +4 dB' }
        ]
    },
    'noise-gate': {
        title: 'NOISE GATE',
        desc: 'Floor Suppressor',
        sliders: [
            { name: 'Threshold-(dB)', desc: '-60 to 0 dB trigger point' },
            { name: 'Attack-(ms)', desc: '1–100 ms opening speed' },
            { name: 'Release-(ms)', desc: '50–1000 ms closing speed' }
        ],
        useCases: [
            { title: 'Remove amp noise floor', desc: '-40 dB threshold, 10ms attack' }
        ]
    },
    'sub-boost': {
        title: 'SUB BOOST',
        desc: 'Low Shelf Enhancer',
        sliders: [
            { name: 'Freq-(Hz)', desc: '40–120 Hz shelf point' },
            { name: 'Gain-(dB)', desc: '-12 to +12dB boost' }
        ],
        useCases: [
            { title: 'Add kick body', desc: '80 Hz, +6 dB' }
        ]
    },
    'mid-presence': {
        title: 'MID PRESENCE',
        desc: 'Clarity Booster',
        sliders: [
            { name: 'Freq-(Hz)', desc: '800–4000 Hz target' },
            { name: 'Gain-(dB)', desc: '-6 to +6dB boost/cut' }
        ],
        useCases: [
            { title: 'Make vocal cut through', desc: '2000 Hz, +4 dB' }
        ]
    },
    'ceiling-limit': {
        title: 'CEILING LIMIT',
        desc: 'Brickwall Protector',
        sliders: [
            { name: 'Ceiling-(dB)', desc: '-3.0 to -0.1 dB max level' },
            { name: 'Lookahead-(ms)', desc: '0–5 ms look-ahead' }
        ],
        useCases: [
            { title: 'Prevent clipping before export', desc: '-0.3 dB ceiling, 1ms lookahead' }
        ]
    },
    'channel-balance': {
        title: 'CHANNEL BALANCE',
        desc: 'L/R Level Trim',
        sliders: [
            { name: 'Left-(dB)', desc: '-6 to +6dB left channel trim' },
            { name: 'Right-(dB)', desc: '-6 to +6dB right channel trim' }
        ],
        useCases: [
            { title: 'Fix stereo imbalance', desc: '+2 dB left, -1 dB right' }
        ]
    },
    'wide-cut': {
        title: 'WIDE CUT',
        desc: 'Mud Remover',
        sliders: [
            { name: 'Freq-(Hz)', desc: '100–800 Hz mud frequency' },
            { name: 'Gain-(dB)', desc: '-12 to 0 dB cut' }
        ],
        useCases: [
            { title: 'Remove low-mid mud', desc: '300 Hz, -6 dB' }
        ]
    },
    'tape-saturation': {
        title: 'TAPE SATURATION',
        desc: 'Analog Warmth',
        sliders: [
            { name: 'Drive-%', desc: '0–100% saturation amount' }
        ],
        useCases: [
            { title: 'Vintage vocal warmth', desc: '40% drive' }
        ]
    },
    'bit-crush': {
        title: 'BIT CRUSH',
        desc: 'Lo-Fi Degradation',
        sliders: [
            { name: 'Bits', desc: '1–16 bit resolution' },
            { name: 'Sample-Rate-(Hz)', desc: '1kHz–44.1kHz sample rate' }
        ],
        useCases: [
            { title: '8-bit retro synth', desc: '8 bits, 22kHz' }
        ]
    },
    'tremolo': {
        title: 'TREMOLO',
        desc: 'Amplitude Modulation',
        sliders: [
            { name: 'Rate-(Hz)', desc: '0.1–20 Hz modulation speed' },
            { name: 'Depth-%', desc: '0–100% modulation intensity' }
        ],
        useCases: [
            { title: 'Classic guitar tremolo', desc: '5 Hz, 50% depth' }
        ]
    },
    'compressor': {
        title: 'COMPRESSOR',
        desc: 'Dynamic Control',
        sliders: [
            { name: 'Threshold-dB', desc: '-60 to 0 dB compression start point' },
            { name: 'Ratio', desc: '1:1 to 20:1 — higher = more compression' }
        ],
        useCases: [
            { title: 'Vocal compression', desc: '-20 dB threshold, 4:1 ratio' },
            { title: 'Bus glue', desc: '-30 dB threshold, 2:1 ratio' },
            { title: 'Heavy drum squeeze', desc: '-15 dB threshold, 8:1 ratio' }
        ]
    },
    'expander': {
        title: 'EXPANDER',
        desc: 'Downward Expansion',
        sliders: [
            { name: 'Threshold-dB', desc: '-60 to 0 dB expansion start point' },
            { name: 'Ratio', desc: '1:1 to 4:1 — how much to reduce below threshold' }
        ],
        useCases: [
            { title: 'Reduce noise floor gently', desc: '-40 dB threshold, 2:1 ratio' }
        ]
    },
    'soft-limit': {
        title: 'SOFT LIMIT',
        desc: 'Transparent Ceiling',
        sliders: [
            { name: 'Ceiling-dB', desc: '-3.0 to 0 dB maximum output level' },
            { name: 'Ratio', desc: '1:1 to 10:1 — soft knee compression' }
        ],
        useCases: [
            { title: 'Master bus protection', desc: '-0.5 dB ceiling, 3:1 ratio' }
        ]
    },
    'de-esser': {
        title: 'DE-ESSER',
        desc: 'Sibilance Control',
        sliders: [
            { name: 'Target-(Hz)', desc: '4kHz–10kHz sibilance frequency' },
            { name: 'Reduction-dB', desc: '-18 to 0 dB max attenuation' }
        ],
        useCases: [
            { title: 'Female vocal sibilance', desc: '6000 Hz, -12 dB reduction' }
        ]
    },
    'stereo-width': {
        title: 'STEREO WIDTH',
        desc: 'Mid/Side Spread',
        sliders: [
            { name: 'Width-%', desc: '0% (mono) to 200% (super wide)' }
        ],
        useCases: [
            { title: 'Widen synth pad', desc: '150% width' },
            { title: 'Narrow to mono', desc: '0% for club compatibility' }
        ]
    },
        'auto-pan': {
        title: 'AUTO PAN',
        desc: 'LFO Panning',
        sliders: [
            { name: 'Rate-(Hz)', desc: '0.1–10 Hz modulation speed' },
            { name: 'Depth-%', desc: '0–100% pan oscillation intensity' }
        ],
        useCases: [
            { title: 'Subtle movement', desc: '2 Hz, 30% depth' },
            { title: 'Psychedelic effect', desc: '5 Hz, 80% depth' }
        ]
    },
    'vocal-gate': {
        title: 'VOCAL GATE',
        desc: 'Voice Optimized',
        sliders: [
            { name: 'Threshold-dB', desc: '-60 to 0 dB voice detection level' },
            { name: 'Attack-ms', desc: '1–50 ms opening speed for transients' },
            { name: 'Release-ms', desc: '50–500 ms closing speed between phrases' }
        ],
        useCases: [
            { title: 'Talking voice podcast', desc: '-35 dB, 5ms attack, 150ms release' },
            { title: 'Singing vocal', desc: '-40 dB, 3ms attack, 200ms release' }
        ]
    }
};

function openGuide(id) {
    const guide = guides[id];
    if (!guide) return;
    
    const modalBody = document.getElementById('modal-body');
    modalBody.innerHTML = `
        <div class="guide-header">
            <h3>${guide.title}</h3>
            <p>${guide.desc}</p>
        </div>
        <div class="guide-body">
            <div class="guide-section">
                <h4>Sliders</h4>
                <ul class="guide-sliders">
                    ${guide.sliders.map(s => `
                        <li>
                            <span class="slider-name">${s.name}</span>
                            <span class="slider-desc">${s.desc}</span>
                        </li>
                    `).join('')}
                </ul>
            </div>
            <div class="guide-section">
                <h4>Use Cases</h4>
                <ul class="use-cases">
                    ${guide.useCases.map(u => `
                        <li><strong>${u.title}:</strong> ${u.desc}</li>
                    `).join('')}
                </ul>
            </div>
        </div>
    `;
    
    document.getElementById('guide-modal').classList.add('active');
}

function closeModal() {
    document.getElementById('guide-modal').classList.remove('active');
}

document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeModal();
});

document.getElementById('guide-modal').addEventListener('click', (e) => {
    if (e.target.classList.contains('modal-overlay')) closeModal();
});

document.querySelectorAll('.info-badge').forEach(badge => {
    badge.addEventListener('click', (e) => {
        e.stopPropagation();
    });
});