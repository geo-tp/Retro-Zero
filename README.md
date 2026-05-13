# Retro-Zero

### **In Progress**

Retro-Zero is a  Libretro-based emulator frontend designed for the **Cardputer Zero**.


## Supported systems

| Name | Core | Supported extensions |
|---|---|---|
| Nintendo Entertainment System | QuickNES | `.nes`, `.fds`, `.unf`, `.unif` |
| Super Nintendo | Snes9x | `.sfc`, `.smc`, `.fig`, `.swc`, `.bs`, `.st` |
| Game Boy | Gearboy | `.gb` |
| Game Boy Color | Gearboy | `.gbc`, `.gb` |
| Game Boy Advance | mGBA | `.gba`, `.gbc`, `.gb`, `.zip` |
| Nintendo 64 | Mupen64Plus GLES2 | `.n64`, `.v64`, `.z64`, `.bin` |
| Mega Drive / Genesis | Genesis Plus GX | `.md`, `.gen`, `.bin`, `.smd` |
| Master System | Genesis Plus GX | `.sms` |
| Game Gear | Genesis Plus GX | `.gg` |
| Dreamcast | Flycast | `.cdi`, `.gdi`, `.chd` |
| Neo Geo | FBNeo | `.zip` |
| Neo Geo Pocket | Mednafen NGP | `.ngp`, `.ngc` |
| FBNeo Arcade | FBNeo | `.zip` |
| MAME 2010 | MAME 2010 | `.zip` |
| PlayStation | PCSX ReARMed | `.cue`, `.bin`, `.chd`, `.pbp`, `.iso`, `.m3u` |
| PC Engine | Mednafen PCE Fast | `.pce`, `.sgx` |
| MSX / MSX2 | blueMSX | `.rom`, `.mx1`, `.mx2` |
| Atari 2600 | Stella 2014 | `.a26`, `.bin` |
| Atari 7800 | ProSystem | `.a78`, `.bin` |
| Atari Lynx | Handy | `.lnx` |
| WonderSwan | Mednafen WonderSwan | `.ws`, `.wsc` |


## Emulator cores

The emulator cores used by this project are prebuilt Libretro `.so` cores for the **Cardputer Zero / ARM64 Linux** environment.

Cores are **not all required to be present upfront**. Each core is downloaded on demand the first time it is needed, if the corresponding `.so` file is not already available locally.

If needed, cores can also be added manually by placing the `.so` files in:

```txt
/home/pi/retrozero/cores
```

For example:

```txt
/home/pi/retrozero/cores/quicknes_libretro.so
/home/pi/retrozero/cores/snes9x_libretro.so
/home/pi/retrozero/cores/mupen64plus_libretro.so
```

## ROM directories

By default, ROMs are expected under:

| System | Directory |
|---|---|
| NES | `/home/pi/roms/nes` |
| SNES | `/home/pi/roms/snes` |
| GB | `/home/pi/roms/gb` |
| GBC | `/home/pi/roms/gbc` |
| GBA | `/home/pi/roms/gba` |
| N64 | `/home/pi/roms/n64` |
| Mega Drive | `/home/pi/roms/md` |
| Master System | `/home/pi/roms/sms` |
| Game Gear | `/home/pi/roms/gg` |
| Dreamcast | `/home/pi/roms/dc` |
| Neo Geo | `/home/pi/roms/neogeo` |
| FBNeo | `/home/pi/roms/fbn` |
| MAME | `/home/pi/roms/mame` |
| PS1 | `/home/pi/roms/ps1` |
| PC Engine | `/home/pi/roms/pce` |
| MSX | `/home/pi/roms/msx` |
| Atari 2600 | `/home/pi/roms/a2600` |
| Atari 7800 | `/home/pi/roms/a7800` |
| Atari Lynx | `/home/pi/roms/lynx` |
| WonderSwan | `/home/pi/roms/ws` |

You can use the **ROM Upload** features to directly import ROM to your device.


## Saves

Save files are stored under:

```txt
/home/pi/retrozero/saves/
```

Each system has its own save subdirectory.

Example:

```txt
/home/pi/retrozero/saves/n64
/home/pi/retrozero/saves/ps1
/home/pi/retrozero/saves/gba
```

---