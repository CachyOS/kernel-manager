project('cachyos-kernel-manager', 'cpp',
        version: '1.0.5',
        license: 'GPLv3',
        meson_version: '>=0.59.0',
        default_options: ['cpp_std=c++20',
                          'buildtype=debugoptimized',
                          'warning_level=3',
                          'werror=true',
                          'b_ndebug=if-release'])

is_debug_build     = get_option('buildtype').startswith('debug')
is_dev_environment = get_option('devenv')
is_dummy_pkg_impl  = get_option('pkg_dummy_impl')
cc = meson.get_compiler('cpp')
if cc.get_id() == 'clang'
    specific_cc_flags = [
        '-nostdlib++',
        #'-stdlib=libc++',
        '-nodefaultlibs',
    ]
    specific_link_flags = [
        '-fuse-ld=lld',
    ]
    add_global_arguments(cc.get_supported_arguments(specific_cc_flags), language : 'cpp')
    add_global_link_arguments(cc.get_supported_link_arguments(specific_link_flags), language : 'cpp')
endif

if is_debug_build
    add_global_arguments('-D_GLIBCXX_ASSERTIONS', language : 'cpp')
endif
add_global_arguments('-DQT_DISABLE_DEPRECATED_BEFORE=0x050F00', language : 'cpp')

if not is_dev_environment
    add_global_arguments('-DNDEVENV', language : 'cpp')
endif
if is_dummy_pkg_impl
    add_global_arguments('-DPKG_DUMMY_IMPL', language : 'cpp')
endif

qt5 = import('qt5')
prog_python = import('python').find_installation('python3')
qt5_dep = dependency('qt5', modules: ['Widgets'])

# Common dependencies
fmt = dependency('fmt', version : ['>=8.0.0'], fallback : ['fmt', 'fmt_dep'])
libalpm = dependency('libalpm', version : ['>=13.0.0'])
glib = dependency('glib-2.0', version : ['>=2.72.1'])

src_files = files(
    'src/ini.hpp',
    'src/utils.hpp', 'src/utils.cpp',
    'src/kernel.hpp', 'src/kernel.cpp',
    'src/km-window.hpp', 'src/km-window.cpp',
    'src/conf-window.hpp', 'src/conf-window.cpp',
    'src/main.cpp',
)

possible_cc_flags = [
    '-Wshadow',

    '-Wnon-virtual-dtor',

    '-Wold-style-cast',
    '-Wcast-align',
    '-Wunused',
    '-Woverloaded-virtual',

    '-Wpedantic', # non-standard C++
    '-Wconversion', # type conversion that may lose data
    '-Wsign-conversion',
    '-Wnull-dereference',
    '-Wdouble-promotion', # float to double

    '-Wformat=2',
    '-Wimplicit-fallthrough', # fallthrough without an explicit annotation
]

if cc.get_id() == 'gcc'
    possible_cc_flags += [
        '-Wmisleading-indentation',

        '-Wduplicated-cond',
        '-Wduplicated-branches',
        '-Wlogical-op',
        '-Wuseless-cast',

        '-Wsuggest-attribute=cold',
        '-Wsuggest-attribute=format',
        '-Wsuggest-attribute=malloc',
        '-Wsuggest-attribute=noreturn',
        '-Wsuggest-attribute=pure',
        '-Wsuggest-final-methods',
        '-Wsuggest-final-types',
        '-Wdiv-by-zero',
        '-Wanalyzer-double-fclose',
        '-Wanalyzer-double-free',
        '-Wanalyzer-malloc-leak',
        '-Wanalyzer-use-after-free',
    ]
endif

if not is_debug_build
   if cc.get_id() == 'gcc'
      possible_cc_flags += [
         '-flto',
         '-fwhole-program',
      ]
   else
      possible_cc_flags += [
         '-flto=thin',
      ]
   endif

   possible_cc_flags += ['-fdata-sections', '-ffunction-sections']
   possible_link_flags = ['-Wl,--gc-sections']
   add_project_link_arguments(cc.get_supported_link_arguments(possible_link_flags), language : 'cpp')
endif

add_project_arguments(cc.get_supported_arguments(possible_cc_flags), language : 'cpp')

deps = [qt5_dep, fmt, libalpm, glib]
if cc.get_id() == 'clang'
  ranges = dependency('range-v3', version : ['>=0.11.0'])
  deps += [ranges]
endif

prep = qt5.preprocess(
  moc_headers : ['src/km-window.hpp', 'src/conf-window.hpp'] # These need to be fed through the moc tool before use.
)
# XML files that need to be compiled with the uic tol.
prep += qt5.compile_ui(sources : ['src/km-window.ui', 'src/conf-window.ui'])

compile_options_hpp = custom_target(
    'compile_options.hpp',
    output : 'compile_options.hpp',
    input : meson.current_source_dir() + '/src/mkoptions.py',
    command : [prog_python, '@INPUT@', meson.current_build_dir(), meson.current_source_dir()],
)
compile_options_dep = declare_dependency(
    sources : [compile_options_hpp],
)
deps += [compile_options_dep]

src_files += [prep]
executable(
  'cachyos-kernel-manager',
  src_files,
  dependencies: deps,
  include_directories: [include_directories('src')],
  install: true)

summary(
  {
    'Build type': get_option('buildtype'),
  },
  bool_yn: true
)
