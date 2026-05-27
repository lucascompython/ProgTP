project(
    'facil',
    'c',
    version: '0.8.0',
    license: 'MIT',
    meson_version: '>= 1.3.0',
    default_options: ['c_std=gnu11'],
)

cc = meson.get_compiler('c')

project_args = []
if host_machine.system() == 'windows'
    project_args += '-D_WIN32_WINNT=0x0600'
elif host_machine.system() == 'linux'
    project_args += '-D_GNU_SOURCE'
endif

thread_dep = dependency('threads')
m_dep = cc.find_library('m', required: false)
ws_dep = cc.find_library('ws2_32', required: host_machine.system() == 'windows')

facil_inc = include_directories('.')

facil_lib = static_library(
    'facil_http',
    files('progtp_facil_http.c'),
    include_directories: facil_inc,
    c_args: project_args,
    dependencies: [thread_dep, m_dep, ws_dep],
)

facil_dep = declare_dependency(
    include_directories: facil_inc,
    link_with: facil_lib,
    dependencies: [thread_dep, m_dep, ws_dep],
)
