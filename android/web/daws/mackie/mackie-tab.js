// Remote for DAWs: the Mackie Control tab (beta). NULL JSFX • https://nulljsfx.tech • MIT License
//
// For the DAWs without a tab of their own: the phone plays a Mackie Control surface over network
// MIDI (mackie.js, ../core/rtpmidi.js), so the DAW's own Mackie Control support drives it and no
// script of ours is needed. Eight tracks at a time, the master fader and the transport; no
// plugins (the other tabs show the NULL JSFX plugins). Not tried with a DAW yet (Bible C26).
'use strict';
(function () {
  NullDawDesk.register({
    id: 'mackie', title: 'Mackie', order: 70, header: 'Mackie Control · beta',
    prefsKey: 'mackie-hosts', port: 5004, portLabel: 'Network MIDI port',
    files: ['../daws/core/rtpmidi.js', '../daws/mackie/mackie.js'],
    connectTitle: 'Connect to a DAW (Mackie Control)',
    setupTitle: 'Set up the DAW (once)',
    setup: [
      ['For the DAWs without a tab here: Logic Pro, Studio One, Pro Tools, Cakewalk, Reason, Digital Performer and others that support a ', 'Mackie Control', ' surface. Beta: not yet tried with every DAW.'],
      ['Open a network MIDI session named ', 'NULL Remote', ': on a Mac in Audio MIDI Setup > MIDI Studio > Network; on Windows with the free rtpMIDI (Tobias Erichsen), "Who may connect: Anyone".'],
      ['In the DAW add a ', 'Mackie Control', ' (not Mackie Control XT, not HUI) with NULL Remote as its MIDI input and output: Logic Pro: Control Surfaces > Setup > New > Install; Studio One: Studio One > Options > External Devices > Add; Pro Tools: Setup > Peripherals > MIDI Controllers; Cakewalk: Edit > Preferences > Control Surfaces; Reason: Preferences > Control Surfaces > Add.'],
      ['Type the computer\'s address above (port 5004 unless the session uses another) and connect; the DAW then fills the strips. Allow ', 'UDP 5004 and 5005', ' through its firewall.'],
      ['Mackie Control shows the mixer and the transport only. For the NULL JSFX plugins with their own interface use the ', 'REAPER, Ardour, Bitwig, Ableton, Cubase or FL Studio', ' tab.'],
    ],
    noAnswer: (c) => `No answer from ${c.host}:${c.port}. Is the network MIDI session "NULL Remote" open on the computer, is a Mackie Control set up in the DAW with NULL Remote as its input and output, and are the phone and the computer on the same network?`,
    noTracks: 'The DAW shows no tracks on this bank.',
    noPlugins: 'Mackie Control does not show plugins. Use the tab of your DAW for the NULL JSFX plugins.',
    followNote: '',

    async open(c) {
      return new NullMackie.Session({ host: c.host, port: c.port, udp: NullRtpMidi.shellUdp, name: 'NULL Remote' });
    },
    kind: () => '',
    isMaster: (s) => s.ssid === 'master',
    hasFx: () => false,
    hasPan: (s) => s.ssid !== 'master',
    hasButtons: (s) => s.ssid !== 'master', // the master fader has no buttons on a Mackie Control
    unity: 0.75,
    dbText: () => '', // the DAW does not send levels in dB to a Mackie Control
    meter: (s) => ({ h: s.meter, hot: s.meter >= 1 }),
  });
})();
