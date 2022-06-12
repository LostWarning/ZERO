# Project base settings
project('ZERO', 'cpp',
    version : '0.1',
    default_options : ['warning_level=3',
                     'cpp_std=gnu++2a'])

cmplr = meson.get_compiler('cpp')
atomic  = cmplr.find_library('atomic')

atomic_dep = declare_dependency(link_args : ['-latomic'],
                           dependencies : [atomic])

# Project dependencies
thread = dependency('threads')
uring = dependency('liburing')

# Add base folder as an include directory
base_inc_dir = include_directories('src')


subdir('src')
subdir('example')

