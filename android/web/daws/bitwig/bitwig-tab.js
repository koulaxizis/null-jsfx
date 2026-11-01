// Remote for DAWs: the Bitwig Studio tab. NULL JSFX • https://nulljsfx.tech • MIT License
//
// The desk is shared (../core/desk.js); this file gives it Bitwig's texts and session. Bitwig
// has no network remote of its own, so it needs our controller script, once
// (remote/bitwig/NullJSFX/NullJSFX.control.js; the protocol is in bitwig.js).
'use strict';
(function () {
  NullDawDesk.register({
    id: 'bitwig', title: 'Bitwig', order: 30, header: 'Bitwig Studio',
    prefsKey: 'bitwig-hosts', port: 8818, portLabel: 'TCP port',
    files: ['../daws/bitwig/bitwig.js'],
    connectTitle: 'Connect to Bitwig Studio',
    setupTitle: 'Set up Bitwig Studio (once)',
    setup: [
      ['From NULL-JSFX-JSFX.zip (nulljsfx.tech, Download) take the folder ', 'Remote/bitwig/NullJSFX', '.'],
      ['Copy it into ', 'Documents/Bitwig Studio/Controller Scripts', '.'],
      ['Bitwig: Settings > Controllers > Add controller > ', 'NULL JSFX > Remote for DAWs', '.'],
      ['Load the NULL JSFX plugins as ', 'VST3 or CLAP', '; they open here with their own interface.'],
      ['Put the phone on the same network and type the computer\'s address above. Allow ', 'TCP 8818', ' through its firewall.'],
    ],
    noAnswer: (c) => `No answer from ${c.host}:${c.port}. Is the controller "NULL JSFX Remote for DAWs" added in Bitwig (Settings > Controllers), and are the phone and the computer on the same network?`,
    noTracks: 'The project has no tracks yet.',
    noPlugins: 'No devices on this track. Add the NULL JSFX plugins in Bitwig (VST3 or CLAP) and they show here.',
    followNote: 'Drag the controls as in the plugin. Changes made in Bitwig show here at once.',

    async open(c) {
      return new NullBitwig.Session({ dial: NullBitwig.shellDial(c.host, c.port) });
    },
    kind: (s) => s.type,
    isMaster: (s) => s.type === 'Master',
    hasFx: () => true,
    hasPan: () => true,
    unity: 0.7937,
    dbText: (s) => s.volText || '',
    // Bitwig's own meter scale, 0..1
    meter: (s) => ({ h: s.meter, hot: s.meter > 0.98 }),
  });
})();
