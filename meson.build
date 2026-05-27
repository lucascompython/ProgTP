project(
    'ProgTP',
    'c',
    meson_version: '>= 1.3.0',
    version: '0.1',
    default_options: ['warning_level=3', 'c_std=gnu11'],
)

cmake = import('cmake')
fs = import('fs')

clay_sp = subproject('clay')
clay_dep = clay_sp.get_variable('clay_dep')
clay_root = clay_sp.get_variable('clay_root')

common_sources = files('src/app.c')
common_deps = [clay_dep]
platform_targets = []
protocol_sources = files('src/protocol.c')
client_sources = files('src/command_client.c')

native_opt = get_option('native')
tui_opt = get_option('tui')
web_opt = get_option('web')
server_opt = get_option('server')

needs_client_runtime = not native_opt.disabled() or not tui_opt.disabled()
needs_json = needs_client_runtime or not server_opt.disabled()

if needs_json
    yyjson_sp = subproject(
        'yyjson',
        default_options: [
            'incr-reader=disabled',
            'utils=disabled',
            'non-standard-json=disabled',
        ],
    )
    yyjson_dep = yyjson_sp.get_variable('yyjson_dep')
endif

if needs_client_runtime
    curl_options = cmake.subproject_options()
    curl_options.add_cmake_defines({
        'BUILD_CURL_EXE': 'OFF',
        'BUILD_EXAMPLES': 'OFF',
        'BUILD_LIBCURL_DOCS': 'OFF',
        'BUILD_MISC_DOCS': 'OFF',
        'BUILD_SHARED_LIBS': 'OFF',
        'BUILD_STATIC_LIBS': 'ON',
        'BUILD_TESTING': 'OFF',
        'CURL_BROTLI': 'OFF',
        'CURL_DISABLE_INSTALL': 'ON',
        'CURL_ENABLE_SSL': 'OFF',
        'CURL_USE_GSASL': 'OFF',
        'CURL_USE_GSSAPI': 'OFF',
        'CURL_USE_LIBPSL': 'OFF',
        'CURL_USE_LIBSSH': 'OFF',
        'CURL_USE_LIBSSH2': 'OFF',
        'CURL_ZLIB': 'OFF',
        'CURL_ZSTD': 'OFF',
        'ENABLE_CURL_MANUAL': 'OFF',
        'HTTP_ONLY': 'ON',
        'PICKY_COMPILER': 'OFF',
        'USE_NGHTTP2': 'OFF',
        'USE_NGTCP2': 'OFF',
        'USE_QUICHE': 'OFF',
        'USE_LIBIDN2': 'OFF',
    })
    curl_sp = cmake.subproject('curl', options: curl_options)
    curl_dep = curl_sp.dependency('libcurl_static')
endif

if not server_opt.disabled()
    facil_sp = subproject('facil', required: server_opt)
endif

if not server_opt.disabled() and facil_sp.found()
    facil_dep = facil_sp.get_variable('facil_dep')
    server_exe = executable(
        'progtp-server',
        ['src/server.c', protocol_sources],
        dependencies: [facil_dep, yyjson_dep],
        include_directories: include_directories('src'),
        install: true,
    )
    platform_targets += server_exe
    run_target('run-server', command: [server_exe, '--public', meson.current_build_dir()])
endif

if not native_opt.disabled()
    sdl3_options = cmake.subproject_options()
    sdl3_options.add_cmake_defines({
        'SDL_ALSA': 'OFF',
        'SDL_CAMERA': 'OFF',
        'SDL_EXAMPLES': 'OFF',
        'SDL_GPU': 'OFF',
        'SDL_HAPTIC': 'OFF',
        'SDL_HIDAPI': 'OFF',
        'SDL_INSTALL': 'OFF',
        'SDL_JACK': 'OFF',
        'SDL_JOYSTICK': 'OFF',
        'SDL_OSS': 'OFF',
        'SDL_PIPEWIRE': 'OFF',
        'SDL_POWER': 'OFF',
        'SDL_PULSEAUDIO': 'OFF',
        'SDL_RENDER_GPU': 'OFF',
        'SDL_SENSOR': 'OFF',
        'SDL_SHARED': 'ON',
        'SDL_STATIC': 'OFF',
        'SDL_TEST_LIBRARY': 'OFF',
        'SDL_TESTS': 'OFF',
        'SDL_VULKAN': 'OFF',
    })
    sdl3_sp = cmake.subproject('sdl3', options: sdl3_options, required: native_opt)
    sdl3_dep = sdl3_sp.dependency('SDL3-shared')
    meson.override_dependency('sdl3', sdl3_dep)
    sdl3_ttf_sp = subproject('sdl3_ttf', required: native_opt)
endif

if not native_opt.disabled() and sdl3_sp.found() and sdl3_ttf_sp.found()
    sdl3_ttf_dep = sdl3_ttf_sp.get_variable('sdl3_ttf_dep')
    native_exe = executable(
        'progtp-native',
        ['src/native_sdl3.c', common_sources, protocol_sources, client_sources],
        dependencies: common_deps + [sdl3_dep, sdl3_ttf_dep, yyjson_dep, curl_dep],
        include_directories: include_directories('src'),
        install: true,
    )
    platform_targets += native_exe
    run_target('run-native', command: [native_exe])
endif

if not tui_opt.disabled()
    termbox2_sp = subproject('termbox2', required: tui_opt)
endif

if not tui_opt.disabled() and termbox2_sp.found()
    tui_exe = executable(
        'progtp-tui',
        ['src/tui_termbox2.c', common_sources, protocol_sources, client_sources],
        c_args: ['-D_DEFAULT_SOURCE'],
        dependencies: common_deps + [
            termbox2_sp.get_variable('termbox2_dep'),
            yyjson_dep,
            curl_dep,
        ],
        include_directories: include_directories('src'),
        install: true,
    )
    platform_targets += tui_exe
    run_target('run-tui', command: [tui_exe])
endif

wasm_cc = find_program(get_option('wasm_cc'), required: web_opt)

if wasm_cc.found()
    wasm = custom_target(
        'progtp-web-wasm',
        input: 'src/web.c',
        output: 'index.wasm',
        command: [
            wasm_cc,
            '-Os',
            '-DPROGTP_WEB',
            '-fno-builtin',
            '-mbulk-memory',
            '--target=wasm32',
            '-nostdlib',
            '-I' + meson.current_source_dir() / 'src',
            '-I' + clay_root,
            '-Wl,--strip-all',
            '-Wl,--no-entry',
            '-Wl,--export=UpdateDrawFrame',
            '-Wl,--export=GetWebCommandBuffer',
            '-Wl,--export=GetWebCommandSize',
            '-Wl,--export=GetCommandLabelBuffer',
            '-Wl,--export=GetCommandLabelCapacity',
            '-Wl,--export-memory',
            '-Wl,--initial-memory=16777216',
            '-o', '@OUTPUT@',
            '@INPUT@',
        ],
        build_by_default: true,
        install: true,
        install_dir: get_option('datadir') / 'progtp-web',
    )
    web_html = fs.copyfile(
        'src/index.html',
        'index.html',
        install: true,
        install_dir: get_option('datadir') / 'progtp-web',
    )
    platform_targets += [wasm, web_html]
endif

if platform_targets.length() == 0
    error('No targets enabled. Enable native, tui, or web and allow Meson to fetch the required wraps.')
endif
