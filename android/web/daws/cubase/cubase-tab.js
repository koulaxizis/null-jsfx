// Remote for DAWs: the Cubase tab. NULL JSFX • https://nulljsfx.tech • MIT License
//
// Cubase's remote scripts (the MIDI Remote API) speak MIDI only, so this tab reaches our script
// (remote/cubase/nulljsfx/remotefordaws/) over network MIDI (../core/rtpmidi.js): the phone
// invites a network MIDI session named "NULL Remote" on the computer, and the script speaks the
// Bitwig script's protocol in System Exclusive messages, so the session is ../bitwig/bitwig.js.
'use strict';
(function () {
  NullDawDesk.register({
    id: 'cubase', title: 'Cubase', order: 50, header: 'Cubase',
    prefsKey: 'cubase-hosts', port: 5004, portLabel: 'Network MIDI port',
    files: ['../daws/bitwig/bitwig.js', '../daws/core/rtpmidi.js'],
    connectTitle: 'Connect to Cubase',
    setupTitle: 'Set up Cubase (once)',
    setup: [
      ['From NULL-JSFX-JSFX.zip (nulljsfx.tech, Download) take the folder ', 'Remote/cubase/nulljsfx', '.'],
      ['Copy it into ', 'Documents/Steinberg/Cubase/MIDI Remote/Driver Scripts/Local', '.'],
      ['Open a network MIDI session named ', 'NULL Remote', ': on a Mac in Audio MIDI Setup > MIDI Studio > Network; on Windows with the free rtpMIDI (Tobias Erichsen), "Who may connect: Anyone".'],
      ['Restart Cubase. It finds the port and lists ', 'NULL JSFX Remote for DAWs', ' in the MIDI Remote Manager.'],
      ['Load the NULL JSFX plugins as ', 'VST3', ' inserts; they open here with their own interface.'],
      ['Type the computer\'s address above (port 5004 unless the session uses another). Allow ', 'UDP 5004 and 5005', ' through its firewall.'],
    ],
    noAnswer: (c) => `No answer from ${c.host}:${c.port}. Is the network MIDI session "NULL Remote" open on the computer (Audio MIDI Setup on a Mac, rtpMIDI on Windows), is Cubase running, and are the phone and the computer on the same network?`,
    noTracks: 'The project has no tracks yet.',
    noPlugins: 'No inserts on this track. Cubase selects the track you open; add the NULL JSFX plugins (VST3) as inserts and they show here.',
    followNote: 'Drag the controls as in the plugin. Changes made in Cubase show here at once.',

    async open(c) {
      return new NullBitwig.Session({ dial: NullRtpMidi.dial(c.host, c.port, { udp: NullRtpMidi.shellUdp, name: 'NULL Remote' }) });
    },
    kind: () => '', // the MIDI Remote API does not say a channel's type
    isMaster: () => false, // the window holds tracks, groups and FX channels, not the outputs
    hasFx: () => true,
    hasPan: () => true,
    unity: 0.7, // only until Cubase reports the fader (the script learns 0 dB for its resets)
    dbText: (s) => (s.volText ? s.volText + (/dB/.test(s.volText) ? '' : ' dB') : ''),
    meter: (s) => ({ h: s.meter, hot: s.meter > 0.98 }),
  });
})();
