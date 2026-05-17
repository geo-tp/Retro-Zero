## Emulator cores

The emulator cores provided in this project are prebuilt Libretro `.so` cores for the Cardputer Zero / ARM64 Linux environment.

The bundled cores are dated **May 10, 2026**. All of them have been tested and confirmed working on the **Cardputer Zero CM0**, with two intentional exceptions:

- **Flycast / Dreamcast** uses an older core on purpose. This version was selected because it provides better performance and compatibility with the current Cardputer Zero rendering pipeline.
- **Mupen / Nintendo 64** uses a an older core specific ARM64 + GLES2-compatible.
- **PPSSPP** : uses a custom fixed core to avoid segfaults: GL push-buffer uploads are capped/segmented to avoid huge VC4 stream-buffer flushes, The VC4 path suppresses the "Video memory FULL" OSD render after OOM and logs to console instead, Texture/FBO/draw upload OOM paths now drop the current render task safely.

The goal is not necessarily to use the newest possible upstream core for every system, but to provide the best tested balance between:

- performance,
- compatibility with ARM64,
- compatibility with GLES2,
- stability on the Cardputer Zero display pipeline

Because of this, some cores may intentionally differ from the latest Libretro buildbot versions when a newer build performs worse or is incompatible with the device.