project(
  'sdl3_ttf',
  'c',
  license: 'zlib',
  version: 'undefined',
  meson_version: '>=0.57.0',
)

sdl_ttf_header = import('fs').read('include/SDL3_ttf/SDL_ttf.h')

project_version = {
  'major': sdl_ttf_header.split('#define SDL_TTF_MAJOR_VERSION')[1].split('\n')[0].strip(),
  'minor': sdl_ttf_header.split('#define SDL_TTF_MINOR_VERSION')[1].split('\n')[0].strip(),
  'micro': sdl_ttf_header.split('#define SDL_TTF_MICRO_VERSION')[1].split('\n')[0].strip(),
}
so_version = {
  'major': '0',
  'minor': project_version['minor'],
  'micro': project_version['micro'],
}
dylib_compat_version = {
  'minor': '0',
  'micro': '0',
}
dylib_current_version = {
  'micro': '0',
}

if project_version['minor'].to_int().is_even()
  dylib_compat_version += {
    'major': (100 * project_version['minor'].to_int() + 1).to_string(),
  }
  dylib_current_version += {
    'minor': project_version['micro'],
  }
else
  dylib_compat_version += {
    'major': (100 * project_version['minor'].to_int() + project_version['micro'].to_int() + 1).to_string(),
  }
  dylib_current_version += {
    'minor': '0',
  }
endif

dylib_current_version += {
  'major': dylib_compat_version['major'],
}

compiler = meson.get_compiler('c')

project_arguments = [
  'SDL_BUILD_MAJOR_VERSION=' + project_version['major'],
  'SDL_BUILD_MINOR_VERSION=' + project_version['minor'],
  'SDL_BUILD_MICRO_VERSION=' + project_version['micro'],
]

sdl_dep = dependency(
  'sdl3',
  version: '>=3.2.6',
)

freetype_dep = dependency('freetype2')
thread_dep = dependency(
  'threads',
  required: false,
)

foreach argument : project_arguments
  add_project_arguments(
    '-D' + argument,
    language: 'c',
  )
endforeach

subdir('src')
ttf_lib = library(
  'sdl3_ttf',
  ttf_src,
  include_directories: include_directories('include'),
  dependencies: [sdl_dep, freetype_dep, thread_dep],
  soversion: so_version['major'],
  version: so_version['major'] + '.' + so_version['minor'] + '.' + so_version['micro'],
  darwin_versions: [
    dylib_compat_version['major'] + '.' + dylib_compat_version['minor'] + '.' + dylib_compat_version['micro'],
    dylib_current_version['major'] + '.' + dylib_current_version['minor'] + '.' + dylib_current_version['micro'],
  ],
)

sdl3_ttf_dep = declare_dependency(
  include_directories: include_directories('include'),
  link_with: ttf_lib,
)
