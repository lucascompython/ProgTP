project(
    'ProgTP',
    'c',
    meson_version: '>= 1.3.0',
    version: '0.1',
    default_options: ['warning_level=3'],
)

dependencies = []

sources = [
    'progtp.c',
]

exe = executable(
    'ProgTP',
    sources,
    dependencies: dependencies,
    install: true,
)

run_target('run', command: [exe])

test('basic', exe)
