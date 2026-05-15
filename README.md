# Pratical assignment Programming 1

## Build

You'll need to have [meson](https://mesonbuild.com/) and [ninja](https://ninja-build.org/) installed.

```sh
meson setup build # dev
# OR
meson setup build --buildtype release --strip # release

meson compile -C build # compile
meson compile -C build run # run
```
