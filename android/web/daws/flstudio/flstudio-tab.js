// Remote for DAWs: the FL Studio tab. NULL JSFX • https://nulljsfx.tech • MIT License
//
// FL Studio's MIDI scripts speak MIDI only, so this tab reaches our script
// (remote/flstudio/NULL JSFX/device_NULLJSFX.py) over network MIDI (../core/rtpmidi.js), as the
// Cubase tab does: the script speaks the Bitwig script's protocol in System Exclusive messages,
// so the session is ../bitwig/bitwig.js. The window holds FL's mixer: the master, then the
// inserts up to the last one in use.
'use strict';
(function () {
  NullDawDesk.register({
    id: 'flstudio', title: 'FL Studio', order: 60, header: 'FL Studio',
    prefsKey: 'flstudio-hosts', port: 5004, portLabel: 'Network MIDI port',
    files: ['../daws/bitwig/bitwig.js', '../daws/core/rtpmidi.js'],
    connectTitle: 'Connect to FL Studio',
    setupTitle: 'Set up FL Studio (once)',
    setup: [
      ['From NULL-JSFX-JSFX.zip (nulljsfx.tech, Download) take the folder ', 'Remote/flstudio/NULL JSFX', '.'],
      ['Copy it into ', 'Documents/Image-Line/FL Studio/Settings/Hardware', '.'],
      ['Open a network MIDI session named ', 'NULL Remote', ': on a Mac in Audio MIDI Setup > MIDI Studio > Network; on Windows with the free rtpMIDI (Tobias Erichsen), "Who may connect: Anyone".'],
      ['FL Studio: Options > MIDI settings. Select the input NULL Remote, enable it, set Controller type to ', 'NULL JSFX Remote for DAWs (user)', ' and give it a port number; give the output NULL Remote the same port number.'],
      ['Load the NULL JSFX plugins as ', 'VST3', ' in the mixer\'s effect slots; they open here with their own interface.'],
      ['Type the computer\'s address above (port 5004 unless the session uses another). Allow ', 'UDP 5004 and 5005', ' through its firewall.'],
    ],
    noAnswer: (c) => `No answer from ${c.host}:${c.port}. Is the network MIDI session "NULL Remote" open on the computer (Audio MIDI Setup on a Mac, rtpMIDI on Windows), is FL Studio running with the script set as its controller type, and are the phone and the computer on the same network?`,
    noTracks: 'The mixer has no tracks yet.',
    noPlugins: 'No effects on this mixer track. Add the NULL JSFX plugins (VST3) to its slots and they show here.',
    followNote: 'Drag the controls as in the plugin. Changes made in FL Studio show here at once.',

    async open(c) {
      return new NullBitwig.Session({ dial: NullRtpMidi.dial(c.host, c.port, { udp: NullRtpMidi.shellUdp, name: 'NULL Remote' }) });
    },
    kind: (s) => s.type,
    isMaster: (s) => s.type === 'Master',
    hasFx: () => true,
    hasPan: () => true,
    noBypass: true, // FL Studio switches all effect slots of a track at once
    unity: 0.8, // FL's 0 dB on its mixer faders
    dbText: (s) => s.volText || '',
    meter: (s) => ({ h: s.meter, hot: s.meter > 0.98 }),
  });
})();
