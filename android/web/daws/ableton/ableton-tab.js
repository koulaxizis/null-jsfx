// Remote for DAWs: the Ableton Live tab. NULL JSFX • https://nulljsfx.tech • MIT License
//
// The desk is shared (../core/desk.js) and so is the session: our control surface for Live
// (remote/ableton/NullJSFX, a MIDI Remote Script in Python) speaks the protocol of the Bitwig
// controller script, so this tab talks to it through ../bitwig/bitwig.js. Live has no network
// remote of its own, so the control surface is installed once.
'use strict';
(function () {
  NullDawDesk.register({
    id: 'ableton', title: 'Ableton', order: 40, header: 'Ableton Live',
    prefsKey: 'ableton-hosts', port: 8819, portLabel: 'TCP port',
    files: ['../daws/bitwig/bitwig.js'],
    connectTitle: 'Connect to Ableton Live',
    setupTitle: 'Set up Ableton Live (once)',
    setup: [
      ['From NULL-JSFX-JSFX.zip (nulljsfx.tech, Download) take the folder ', 'Remote/ableton/NullJSFX', '.'],
      ['Copy it into Live\'s User Library, folder ', 'Remote Scripts', ' (create it if it is not there), then restart Live.'],
      ['Live: Settings > Link, Tempo & MIDI > Control Surface: ', 'NullJSFX', ' (Input and Output: None).'],
      ['Load the NULL JSFX plugins as ', 'VST3', '; they open here with their own interface.'],
      ['Put the phone on the same network and type the computer\'s address above. Allow ', 'TCP 8819', ' through its firewall.'],
    ],
    noAnswer: (c) => `No answer from ${c.host}:${c.port}. Is NullJSFX chosen as a Control Surface in Live (Settings > Link, Tempo & MIDI), and are the phone and the computer on the same network?`,
    noTracks: 'The Set has no tracks yet.',
    noPlugins: 'No devices on this track. Add the NULL JSFX plugins in Live (VST3) and they show here.',
    followNote: 'Drag the controls as in the plugin. Changes made in Live show here within a tenth of a second.',

    async open(c) {
      return new NullBitwig.Session({ dial: NullBitwig.shellDial(c.host, c.port) });
    },
    kind: (s) => s.type,
    isMaster: (s) => s.type === 'Master',
    hasFx: () => true,
    hasPan: () => true,
    unity: 0.85,
    dbText: (s) => s.volText || '',
    // Live's own meter scale, 0..1
    meter: (s) => ({ h: s.meter, hot: s.meter > 0.98 }),
  });
})();
