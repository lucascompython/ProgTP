project(
    'ProgTP',
    'c',
    meson_version: '>= 1.3.0',
    version: '0.1',
    default_options: ['warning_level=3', 'c_std=gnu11'],
)

cmake = import('cmake')
fs = import('fs')
cc = meson.get_compiler('c')
release_size = get_option('release_size')

project_warnings = cc.get_supported_arguments([
    '-Wshadow',
    '-Wformat=2',
    '-Wundef',
    '-Wstrict-prototypes',
])
add_project_arguments(project_warnings, language: 'c')
server_warning_suppressions = cc.get_supported_arguments(['-Wno-unused-function'])
static_subproject_options = ['default_library=static']
release_subproject_options = []

if release_size
    release_c_arg_candidates = [
        '-DNDEBUG',
        '-flto',
        '-ffunction-sections',
        '-fdata-sections',
        '-fno-unwind-tables',
        '-fno-asynchronous-unwind-tables',
    ]
    release_link_arg_candidates = [
        '-flto',
        '-Wl,--gc-sections',
        '-Wl,--as-needed',
        '-s',
    ]
    if get_option('optimization') != 's'
        release_c_arg_candidates = ['-Os'] + release_c_arg_candidates
        release_link_arg_candidates = ['-Os'] + release_link_arg_candidates
    endif
    release_c_args = cc.get_supported_arguments(release_c_arg_candidates)
    release_link_args = cc.get_supported_link_arguments(release_link_arg_candidates)
    add_project_arguments(release_c_args, language: 'c')
    add_project_link_arguments(release_link_args, language: 'c')
    release_subproject_options = static_subproject_options + [
        'optimization=s',
        'debug=false',
        'b_lto=true',
        'b_ndebug=true',
    ]
else
    release_subproject_options = static_subproject_options
endif

src_inc = include_directories(
    'src',
    'src/app',
    'src/client',
    'src/domain/connectivity',
    'src/domain/incidents',
    'src/domain/inventory',
    'src/protocol',
)

clay_sp = subproject('clay')
clay_dep = clay_sp.get_variable('clay_dep').as_system('system')
clay_root = clay_sp.get_variable('clay_root')

common_sources = files('src/app/app.c')
common_deps = [clay_dep]
platform_targets = []
inventory_sources = files('src/domain/inventory/equipment_inventory.c')
incident_sources = files('src/domain/incidents/incident_store.c')
connectivity_model_sources = files('src/domain/connectivity/connectivity_model.c')
connectivity_sources = files('src/domain/connectivity/connectivity.c') + connectivity_model_sources + incident_sources
protocol_sources = files('src/protocol/protocol.c') + inventory_sources
client_sources = files('src/client/command_client.c')
lint_sources = common_sources + protocol_sources + connectivity_sources

native_opt = get_option('native')
tui_opt = get_option('tui')
web_opt = get_option('web')
server_opt = get_option('server')

needs_client_runtime = not native_opt.disabled() or not tui_opt.disabled()
needs_json = needs_client_runtime or not server_opt.disabled() or not web_opt.disabled()

if needs_client_runtime
    lint_sources += client_sources
endif

if needs_json
    yyjson_sp = subproject(
        'yyjson',
        default_options: release_subproject_options + [
            'warning_level=0',
            'incr-reader=disabled',
            'utils=disabled',
            'non-standard-json=disabled',
        ],
    )
    yyjson_dep = yyjson_sp.get_variable('yyjson_dep').as_system('system')
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
    if release_size
        curl_options.add_cmake_defines({'CMAKE_BUILD_TYPE': 'MinSizeRel'})
    endif
    curl_sp = cmake.subproject('curl', options: curl_options)
    curl_dep = curl_sp.dependency('libcurl_static').as_system('system')
endif

if not server_opt.disabled()
    facil_sp = subproject('facil', default_options: release_subproject_options + ['warning_level=0'], required: server_opt)
endif

if not server_opt.disabled() and facil_sp.found()
    facil_dep = facil_sp.get_variable('facil_dep').as_system('system')
    server_exe = executable(
        'progtp-server',
        ['src/server/server.c', protocol_sources, connectivity_sources],
        c_args: server_warning_suppressions,
        dependencies: [facil_dep, yyjson_dep],
        include_directories: src_inc,
        install: true,
    )
    platform_targets += server_exe
    lint_sources += files('src/server/server.c')
    run_target('run-server', command: [server_exe, '--public', meson.current_build_dir()])
endif

if not native_opt.disabled()
    sdl3_options = cmake.subproject_options()
    sdl3_options.add_cmake_defines({
        'SDL_ALSA': 'OFF',
        'SDL_AUDIO': 'OFF',
        'SDL_DIALOG': 'OFF',
        'SDL_CAMERA': 'OFF',
        'SDL_EXAMPLES': 'OFF',
        'SDL_GPU': 'ON',
        'SDL_HAPTIC': 'OFF',
        'SDL_HIDAPI': 'OFF',
        'SDL_INSTALL': 'OFF',
        'SDL_JACK': 'OFF',
        'SDL_JOYSTICK': 'OFF',
        'SDL_OSS': 'OFF',
        'SDL_PIPEWIRE': 'OFF',
        'SDL_POWER': 'OFF',
        'SDL_PULSEAUDIO': 'OFF',
        'SDL_RENDER_GPU': 'ON',
        'SDL_SENSOR': 'OFF',
        'SDL_SHARED': 'OFF',
        'SDL_STATIC': 'ON',
        'SDL_TEST_LIBRARY': 'OFF',
        'SDL_TESTS': 'OFF',
    })
    if release_size
        sdl3_options.add_cmake_defines({'CMAKE_BUILD_TYPE': 'MinSizeRel'})
    endif
    sdl3_sp = cmake.subproject('sdl3', options: sdl3_options, required: native_opt)
    sdl3_dep = sdl3_sp.dependency('SDL3-static').as_system('system')
    meson.override_dependency('sdl3', sdl3_dep)

    sdl3_image_options = cmake.subproject_options()
    sdl3_image_options.add_cmake_defines({
        'BUILD_SHARED_LIBS': 'OFF',
        'SDLIMAGE_AVIF': 'OFF',
        'SDLIMAGE_BMP': 'OFF',
        'SDLIMAGE_DEPS_SHARED': 'OFF',
        'SDLIMAGE_GIF': 'OFF',
        'SDLIMAGE_INSTALL': 'OFF',
        'SDLIMAGE_JPG': 'OFF',
        'SDLIMAGE_JXL': 'OFF',
        'SDLIMAGE_LBM': 'OFF',
        'SDLIMAGE_PNG': 'OFF',
        'SDLIMAGE_PNM': 'OFF',
        'SDLIMAGE_QOI': 'OFF',
        'SDLIMAGE_SAMPLES': 'OFF',
        'SDLIMAGE_SVG': 'OFF',
        'SDLIMAGE_TGA': 'OFF',
        'SDLIMAGE_TESTS': 'OFF',
        'SDLIMAGE_TIF': 'OFF',
        'SDLIMAGE_VENDORED': 'OFF',
        'SDLIMAGE_WEBP': 'OFF',
        'SDLIMAGE_XCF': 'OFF',
        'SDLIMAGE_XPM': 'OFF',
        'SDLIMAGE_XV': 'OFF',
    })
    if release_size
        sdl3_image_options.add_cmake_defines({'CMAKE_BUILD_TYPE': 'MinSizeRel'})
    endif
    sdl3_image_sp = cmake.subproject('sdl3_image', options: sdl3_image_options, required: native_opt)
    sdl3_image_dep = sdl3_image_sp.dependency('SDL3_image-static').as_system('system')

    sdl3_ttf_sp = subproject('sdl3_ttf', default_options: release_subproject_options + ['warning_level=0'], required: native_opt)
endif

if not native_opt.disabled() and sdl3_sp.found() and sdl3_ttf_sp.found()
    sdl3_ttf_dep = sdl3_ttf_sp.get_variable('sdl3_ttf_dep').as_system('system')
    native_exe = executable(
        'progtp-native',
        [
            'src/platform/native/native_sdl3.c',
            common_sources,
            protocol_sources,
            client_sources,
            connectivity_sources,
        ],
        dependencies: common_deps + [sdl3_dep, sdl3_ttf_dep, sdl3_image_dep, yyjson_dep, curl_dep],
        include_directories: src_inc,
        install: true,
    )
    platform_targets += native_exe
    lint_sources += files('src/platform/native/native_sdl3.c')
    run_target('run-native', command: [native_exe])
endif

if not tui_opt.disabled()
    termbox2_sp = subproject('termbox2', default_options: release_subproject_options + ['warning_level=0'], required: tui_opt)
    stb_sp = subproject('stb', required: tui_opt)
    stb_dep = stb_sp.get_variable('stb_dep').as_system('system')
    stb_m_dep = cc.find_library('m', required: false)
endif

if not tui_opt.disabled() and termbox2_sp.found()
    tui_exe = executable(
        'progtp-tui',
        [
            'src/platform/tui/tui_termbox2.c',
            common_sources,
            protocol_sources,
            client_sources,
            connectivity_sources,
        ],
        c_args: ['-D_DEFAULT_SOURCE'],
        dependencies: common_deps + [
            termbox2_sp.get_variable('termbox2_dep').as_system('system'),
            stb_dep,
            stb_m_dep,
            yyjson_dep,
            curl_dep,
        ],
        include_directories: src_inc,
        install: true,
    )
    platform_targets += tui_exe
    lint_sources += files('src/platform/tui/tui_termbox2.c')
    run_target('run-tui', command: [tui_exe])
endif

web_cc = find_program(get_option('web_cc'), required: web_opt)
wasm_opt = find_program('wasm-opt', required: false)
web_env = environment()
web_env.set('EM_CACHE', meson.current_build_dir() / 'emscripten-cache')

if web_cc.found()
    wasm_c_optimization = '-Os'
    wasm_binary_output = 'index.wasm'
    wasm_binary_setting = []
    if release_size
        wasm_c_optimization = '-Oz'
        if wasm_opt.found()
            wasm_binary_output = 'index.raw.wasm'
            wasm_binary_setting = ['-sWASM_BINARY_FILE=index.raw.wasm']
        endif
    endif

    wasm_compile = custom_target(
        'progtp-web',
        input: [
            'src/platform/web/web.c',
            'src/app/app.c',
            'src/domain/inventory/equipment_inventory.c',
            'src/domain/connectivity/connectivity_model.c',
            'src/protocol/protocol.c',
        ],
        output: ['index.js', wasm_binary_output],
        command: [
            web_cc,
            wasm_c_optimization,
            '-DPROGTP_WEB',
            '-DNDEBUG',
            '-flto',
            '-mbulk-memory',
            '--no-entry',
            '-sWASM=1',
            '-sMODULARIZE=1',
            '-sEXPORT_NAME=createProgTPModule',
            '-sENVIRONMENT=web',
            '-sEXPORTED_FUNCTIONS=["_InitializeClay","_UpdateDrawFrame","_GetCommandLabelBuffer","_GetCommandLabelCapacity","_GetRenderCommandsBuffer","_GetInventoryJsonBuffer","_GetInventoryJsonCapacity","_ImportInventoryJson","_ExportInventoryJson","_IsInventoryDirty","_GetInventoryVersion","_MarkInventoryClean","_ExportConnectivityRequestJson","_ImportConnectivityResultJson","_FailConnectivityRequest"]',
            '-sEXPORTED_RUNTIME_METHODS=["HEAPU8"]',
            '-sFILESYSTEM=0',
            '-sINITIAL_MEMORY=67108864',
            '-sMALLOC=emmalloc',
            '-ffunction-sections',
            '-fdata-sections',
            '-I' + meson.current_source_dir() / 'src',
            '-I' + meson.current_source_dir() / 'src/app',
            '-I' + meson.current_source_dir() / 'src/domain/inventory',
            '-I' + meson.current_source_dir() / 'src/domain/connectivity',
            '-I' + meson.current_source_dir() / 'src/protocol',
            '-I' + meson.current_source_dir() / 'subprojects/yyjson/src',
            '-I' + clay_root,
            '-Wl,--strip-all',
            '-Wl,--gc-sections',
            '-o', '@OUTPUT0@',
            '-Wl,--lto-O3'
        ] + wasm_binary_setting + [
            '@INPUT0@',
            '@INPUT1@',
            '@INPUT2@',
            '@INPUT3@',
            '@INPUT4@',
            meson.current_source_dir() / 'subprojects/yyjson/src/yyjson.c',
        ],
        env: web_env,
        build_by_default: true,
        install: true,
        install_dir: get_option('datadir') / 'progtp-web',
    )
    wasm = wasm_compile
    if release_size and wasm_opt.found()
        wasm = custom_target(
            'progtp-web-wasm-opt',
            input: wasm_compile[1],
            output: 'index.wasm',
            command: [
                wasm_opt,
                '-Oz',
                '--enable-bulk-memory',
                '--enable-bulk-memory-opt',
                '--enable-nontrapping-float-to-int',
                '--strip-debug',
                '--strip-producers',
                '@INPUT@',
                '-o', '@OUTPUT@',
            ],
            build_by_default: true,
            install: true,
            install_dir: get_option('datadir') / 'progtp-web',
        )
    endif
    web_html = fs.copyfile(
        'src/platform/web/index.html',
        'index.html',
        install: true,
        install_dir: get_option('datadir') / 'progtp-web',
    )
    platform_targets += [wasm, web_html]
    lint_sources += files('src/platform/web/web.c')
endif

if platform_targets.length() == 0
    error('No targets enabled. Enable native, tui, or web and allow Meson to fetch the required wraps.')
endif

clang_tidy = find_program('clang-tidy', required: false)
if clang_tidy.found() and lint_sources.length() > 0
    run_target(
        'lint',
        command: [clang_tidy, '-p', meson.current_build_dir()] + lint_sources,
    )
endif
