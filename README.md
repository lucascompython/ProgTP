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

## Module 1 Inventory

persist it in `equipamentos.dat`

Keyboard controls in all targets:

- `N` / `P`: select next or previous equipment.
- `A`: add a sample equipment.
- `U`: update the selected equipment.
- `R`: remove the selected equipment. Removal is blocked while pending incidents are marked.
- `S`: cycle the selected equipment state.
- `T`: toggle pending incidents for the selected equipment.
- `1` / `2` / `3` / `4`: list using array, singly linked list, doubly linked list, or reverse doubly linked list.
- `5` / `6`: filter routers or failed equipment.
- `C` / `I` / `M`: search by code, IP, or MAC. Type the value and press Enter.
- `W` / `L`: save or reload the binary inventory where persistence is available.

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

Linting:

```sh
meson compile -C build lint
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

- [ ] make the cursor look "clickable" on web and mobile
- [ ] fix corrupted chars in the preview of the module 4
- [ ] sort by priority on module 4
- [ ] add a nice way to see reports in the app itself
- [ ] add cli --help usage info
- [ ] add docs for usage, and docs for features of each module
- [ ] review requirements
- [x] try to split the huge app.c file into ui files for each module
- [x] maybe structure project better for helper functions like comparing strings ignoring case, validating IP/MAC formats, etc in connectivity.c for example
- [x] see if for example i can use fio's helpers better instead of implementing my own
- [x] fix weird font rendering on native
- [x] add warnings + linting
- [x] support for wasm opt
- [x] move to emscripten for web target
- [x] use provided Clay renderers
