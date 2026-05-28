# Practical assignment Programming 1

Shared C/Clay hello-world app for web, SDL3 native, and termbox2 TUI.

## Build

Required: Meson, Ninja, a C compiler, cmake for dependencies and Git for Meson wraps. The web target uses Emscripten and needs `emcc`.

```sh
meson setup build -Dnative=enabled -Dtui=enabled -Dweb=enabled -Dserver=enabled
meson compile -C build
```

For web + server only, no CMake subproject backend is needed:

```sh
meson setup build-web -Dnative=disabled -Dtui=disabled -Dweb=enabled -Dserver=enabled
meson compile -C build-web
```

Release-size build, optimized for small and fast binaries:

```sh
meson setup build-release \
  -Drelease_size=true \
  -Dnative=enabled \
  -Dtui=enabled \
  -Dweb=enabled \
  -Dserver=enabled
meson compile -C build-release
```

In this mode native/server/TUI targets use `-Os`, LTO, `NDEBUG`, section garbage collection, strip-friendly linker flags, and release defaults for Meson subprojects where possible. CMake dependencies are configured as `MinSizeRel`. The wasm target uses `-Oz`; if `wasm-opt` is installed, Meson automatically runs `wasm-opt -Oz` and installs the optimized `index.wasm`.

## Targets

- `progtp-server`: facil.io/cstl HTTP server. Serves `/api/hello` and static web files.
- `progtp-native`: SDL3 desktop app using SDL3 renderer.
- `progtp-tui`: termbox2 terminal app using termbox2 renderer. Press `q`, `Esc`, or `Ctrl-C` to quit.
- `index.html` + `index.wasm`: web app using Clay's HTML renderer.

## Running

Local mode runs the command directly:

```sh
./build/progtp-native
./build/progtp-tui
```

Remote mode calls the HTTP server with libcurl:

```sh
./build/progtp-server --port 8000 --public build
./build/progtp-native --remote http://localhost:8000
./build/progtp-tui --remote http://localhost:8000
```

The web target is served by the same server:

```sh
./build/progtp-server --port 8000 --public build
```

Open `http://localhost:8000/index.html`.

## Shared App

The app layout is shared. `src/app/app.c` builds one Clay render command list for every target.

## Packagefiles

`subprojects/packagefiles` contains local Meson overlay files copied into fetched git subprojects.

Current overlays:

- `clay`: header-only dependency exposing `clay_dep` and `clay_root`.
- `termbox2`: header-only dependency exposing `termbox2_dep`.
- `yyjson`: builds upstream `src/yyjson.c` and exposes feature switches. This project disables incremental reader, utils, and non-standard JSON.
- `facil`: builds cstl with `FIO_HTTP` instead of `FIO_EVERYTHING`.
- `sdl3_ttf`: builds SDL_ttf with SDL3 and FreeType only.
- `stb`: header-only dependency used by Clay's termbox2 renderer.

## TODO:

- [ ] fix weird font rendering on native
- [ ] add cli --help usage info
- [x] add warnings + linting
- [x] support for wasm opt
- [x] move to emscripten for web target
- [x] use provided Clay renderers
