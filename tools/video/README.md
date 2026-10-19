# Presentation videos

`assets/video/<name>.mp4` (1280x720 H.264, AAC, faststart) and `<name>.jpg` (poster) are the
videos the site's guides and the Hub's plugin page play. They are generated, not edited:

| Script | Makes |
|---|---|
| `make_videos.py [plugin ...]` | one per plugin: the GFX version plays `assets/audio/null-jsfx-loop.flac` live (`gfx_video.cpp`, ysfx), with its interface, a spectrum and a level history before (grey) and after (silver), three factory presets, a before/after and a sweep of the main control. Instruments get a MIDI pattern instead (chords, a bass line, a melody; drum grooves run with the host) and a roll of the notes; MIDI effects get one too, with a NULL instrument playing the notes before and after. `videos.json` changes a plugin's script. |
| `make_rack_video.py` | the Rack: its JACK standalone filmed under Xvfb while `jack_loop.c` plays the loop through it and a script picks presets, opens modules and turns a macro |
| `make_hub_video.py` | the Hub: `hub_tour.cjs` films its page in Chromium with the test stand-in (`hub/tests/mock.cjs`), the loop as a bed |

On main, `.github/workflows/media.yml` uploads the videos to the pre-release `media`; the site,
the plugin pages and the Hub stream them from there (the site's own copy is the fallback).

Every script takes `--hq <dir>` to also write a 1920x1080 version at higher quality (for Ko-fi
posts; not checked in). Run `tools/build_site.py` after adding a plugin's video. Audio is
mastered as a whole to about -16 LUFS, so before and after keep their real level difference.
Needs ffmpeg, numpy, Pillow; the Rack also Xvfb, jackd2, xdotool and libjack; the Hub `cd hub && npm ci`.
