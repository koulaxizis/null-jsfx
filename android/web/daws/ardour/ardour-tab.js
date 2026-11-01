// Remote for DAWs: the Ardour tab (Ardour and Harrison Mixbus). NULL JSFX • https://nulljsfx.tech • MIT License
//
// The desk is shared (../core/desk.js); this file gives it Ardour's texts and session. The
// protocol is in ardour.js: Ardour's own OSC surface on UDP 3819, nothing to install.
'use strict';
(function () {
  const typeName = { AT: 'Audio', MT: 'MIDI', B: 'Bus', MB: 'MIDI bus', FB: 'Foldback', V: 'VCA', MA: 'Master', MO: 'Monitor' };

  NullDawDesk.register({
    id: 'ardour', title: 'Ardour', order: 20, header: 'Ardour · Mixbus',
    prefsKey: 'ardour-hosts', port: 3819, portLabel: 'OSC port',
    files: ['../mixers/core/osc.js', '../mixers/core/transport.js', '../daws/ardour/ardour.js'],
    connectTitle: 'Connect to Ardour or Mixbus',
    setupTitle: 'Set up Ardour or Mixbus (once)',
    setup: [
      ['Edit > Preferences > Control Surfaces: tick ', 'Open Sound Control (OSC)', '.'],
      ['Double-click it (or Show Protocol Settings): ', 'Port Mode Auto', ', port 3819.'],
      ['Load the NULL JSFX plugins as ', 'VST3 or LV2', '; they open here with their own interface.'],
      ['Put the phone on the same network and type the computer\'s address above. Allow ', 'UDP 3819', ' through its firewall.'],
    ],
    noAnswer: (c) => `No answer from ${c.host}:${c.port}. Is OSC on in Ardour (Preferences > Control Surfaces), and are the phone and the computer on the same network?`,
    noTracks: 'The session has no tracks yet.',
    noPlugins: 'No plugins on this track. Add the NULL JSFX plugins in Ardour (VST3 or LV2) and they show here.',
    followNote: 'Drag the controls as in the plugin. Changes made in Ardour show here within a second.',

    async open(c) {
      const transport = await NullMixerTransport.openShellUdp(0);
      return new NullArdour.Session({ transport, host: c.host, port: c.port });
    },
    kind: (s) => typeName[s.type] || s.type,
    isMaster: (s) => s.type === 'MA',
    hasFx: (s) => s.type !== 'V',
    hasPan: (s) => s.type !== 'V',
    get unity() { return NullArdour.dbToFader(0); },
    dbText(s, session) {
      const pos = s.fader ?? NullArdour.dbToFader(0);
      const dragged = session && Date.now() - (session.touched.get(s.ssid + '/fader') || 0) < 500;
      const db = s.gain != null && !dragged ? s.gain : NullArdour.faderToDb(pos);
      return db > -120 ? (db > 0 ? '+' : '') + db.toFixed(1) + ' dB' : '-∞';
    },
    // v = (dB + 94) / 100; show -60..+6 dB
    meter(s) {
      const db = s.meter * 100 - 94;
      return { h: (db + 60) / 66, hot: db > -0.5 };
    },
  });
})();
