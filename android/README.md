# Android host

This directory contains the first native Android host for Open Engine Simulator.

The app uses the SDL3 Android archive (AAR), Gradle, CMake, and the existing
portable simulator/visualization code. The native target is intentionally named
`main`, because SDLActivity loads the application entry point from
`libmain.so`.

## Local prerequisites

- Android SDK 35
- Android NDK 27.2.12479018
- CMake 3.31.6
- JDK 17+
- Gradle 8.9
- `SDL3-3.2.8.aar` in `app/libs/`

The CI workflow downloads the SDL3 Android archive automatically.

## Build

From this directory:

```sh
gradle :app:assembleDebug
```

The debug APK is written to:

```
app/build/outputs/apk/debug/app-debug.apk
```

At first launch the Java activity extracts the packaged simulator assets into
the app's private files directory. This gives the existing C++ scripting,
shader, mesh, font, and impulse-response loaders ordinary filesystem paths
without changing their desktop behavior.
