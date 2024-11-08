# meson.build

project('versioning_fs', 'c', version : '1.0', default_options : ['c_std=gnu11'])

# Dependencies
fuse_dep = dependency('fuse3')
cjson_dep = dependency('cjson')

# Include directories
inc = include_directories('include')

# Source files
src_files = files(
  'src/main.c',
  'src/file_metadata.c',
  'src/version_info.c',
  'src/metadata_manager.c',
  'src/version_manager.c'
)

# Build executable
executable('myfs', src_files,
  dependencies : [fuse_dep, cjson_dep],
  include_directories : inc,
  install : true,
  install_dir : get_option('prefix') / 'bin'
)

