# ideas/audio-output — playing sound on the speakers

> **Under review — not part of the polyir standard library.**

| File | What it is |
|---|---|
| `Audio.poly` | the facade a program imports: `soundPlay(sound)`, `audioPlay(samples, count, rate)`, and `audioStream` with a fill function that makes the sound as it plays; it imports `Stream.poly` and `{os}.poly` (the file for the system it is built on) |
| `Stream.poly` | the half that does not depend on the system: the stream record, the time limit, the volume, how buffers are filled (`streamFeed`) |
| `macos.poly` | macOS: AudioQueue (AudioToolbox) with a callback |
| `linux.poly` | Linux: ALSA (libasound), a blocking write loop |
| `windows.poly` | Windows: waveOut (winmm), three prepared buffers taken in turn |

`examples/` holds `compose.poly` (a piece written to a WAV file), `studio.poly` (stereo, WAV, resampling and an equalizer)
and `live.poly` (a stream made while it plays); they use `../audio` and `../gpu/examples/shared` (pictures, plots).
`tests/StreamTests.poly` checks the system-independent half.

**Needs:** a native build (the VM refuses it) and the system's audio API. macOS AudioQueue has been run on a real device
(`live.poly`, muted). The Linux (ALSA: `-lasound`, `sudo apt install libasound2-dev`) and Windows (waveOut: `winmm`) back
ends are written from the documentation and type-checked, but **untested** on real machines. `live.poly` exits with 10 when there is no audio device.
