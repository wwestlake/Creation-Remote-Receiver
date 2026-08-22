# Creation Remote Receiver

The desktop half of **Creation Remote** (Creation-Suite issue [#65](https://github.com/wwestlake/Creation-Suite/issues/65)): a minimal app whose only job is catching photos/video/audio sent from the Creation Remote Android app and depositing them into the right Creation Suite project.

It is not a creative app. It has no standing open project — on receiving an asset it opens the target project by ID, writes the asset, and closes again, a brief self-contained transaction per asset rather than a persistent lock. The creative apps (Station, Movie, Texture, ...) never need remote-receiving logic themselves; they just find the asset already in the project's catalog next time they open it.

## Status

Scaffold only. Project discovery, the pairing QR flow, and the WebRTC receive path are tracked separately:

- [CR-M1](https://github.com/wwestlake/Creation-Suite/issues/82) — this app
- [CR-M2/M4](https://github.com/wwestlake/Creation-Suite/issues/83) — pairing + signaling protocol (lives in the `djehuti` repo)
- [CR-M5](https://github.com/wwestlake/Creation-Suite/issues/84) — the Android client

See `docs/architecture/Creation-Remote-Protocol.md` in the Creation-Suite umbrella repo for the wire contract this app implements against once CR-M2/M4 lands.

## Build

From the Creation-Suite umbrella repo root:

```powershell
.\scripts\Build-Suite.ps1 -Configuration Debug -Targets remote
```

Or standalone:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR=$env:JUCE_DIR
cmake --build build --config Debug
```

Requires `JUCE_DIR` pointed at the same JUCE checkout the rest of the suite uses. See the umbrella repo's `docs/SUITE_BUILD_BOOTSTRAP.md` for the full prerequisite list.
