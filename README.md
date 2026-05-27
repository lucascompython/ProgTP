# Practical assignment Programming 1

Shared C/Clay hello-world app for web, SDL3 native, and termbox2 TUI.

## Build

Required: Meson, Ninja, a C compiler, cmake for dependencies and Git for Meson wraps. The web target also needs `clang` with `wasm32` support.

```sh
meson setup build -Dnative=enabled -Dtui=enabled -Dweb=enabled -Dserver=enabled
meson compile -C build
```

For web + server only, no CMake subproject backend is needed:

```sh
meson setup build-web -Dnative=disabled -Dtui=disabled -Dweb=enabled -Dserver=enabled
meson compile -C build-web
```

## Targets

- `progtp-server`: facil.io/cstl HTTP server. Serves `/api/hello` and static web files.
- `progtp-native`: SDL3 desktop app. Uses SDL_ttf for text, with no SDL_image.
- `progtp-tui`: termbox2 terminal app. Press `q`, `Esc`, or `Ctrl-C` to quit.
- `index.html` + `index.wasm`: web app using the same Clay layout and Fetch API.

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

## Shared App

the app layout is shared. `src/app.c` builds one Clay render command list for every target.

## Packagefiles

`subprojects/packagefiles` contains local Meson overlay files copied into fetched git subprojects.

Current overlays:

- `clay`: header-only dependency exposing `clay_dep` and `clay_root`.
- `termbox2`: header-only dependency exposing `termbox2_dep`.
- `yyjson`: builds upstream `src/yyjson.c` and exposes feature switches. This project disables incremental reader, utils, and non-standard JSON.
- `facil`: builds cstl with `FIO_HTTP` instead of `FIO_EVERYTHING`.
- `sdl3_ttf`: builds SDL_ttf with SDL3 and FreeType only.

## TODO:

- [ ] fix weird font rendering on native
- [ ] add cli --help usage info
- [ ] add warnings + linting
- [ ] support for wasm opt
