project('genesis-plus-gx-highscore', 'c',
  version : '1.7.5',
)

commit = run_command('git', 'describe', '--always', '--dirty', '--match', '', check: false).stdout().strip()
version = meson.project_version()

cc = meson.get_compiler('c')

genesis_plus_gx_source = [
  'genesis-plus-gx-highscore.c',
  'scrc32.c',

  '../core/genesis.c',
  '../core/io_ctrl.c',
  '../core/loadrom.c',
  '../core/mem68k.c',
  '../core/membnk.c',
  '../core/memz80.c',
  '../core/state.c',
  '../core/system.c',
  '../core/vdp_ctrl.c',
  '../core/vdp_render.c',
  '../core/cart_hw/areplay.c',
  '../core/cart_hw/eeprom_93c.c',
  '../core/cart_hw/eeprom_i2c.c',
  '../core/cart_hw/eeprom_spi.c',
  '../core/cart_hw/flash_cfi.c',
  '../core/cart_hw/ggenie.c',
  '../core/cart_hw/md_cart.c',
  '../core/cart_hw/megasd.c',
  '../core/cart_hw/sms_cart.c',
  '../core/cart_hw/sram.c',
  '../core/cart_hw/svp/ssp16.c',
  '../core/cart_hw/svp/svp.c',
  '../core/cart_hw/yx5200.c',
  '../core/cd_hw/cd_cart.c',
  '../core/cd_hw/cdc.c',
  '../core/cd_hw/cdd.c',
  '../core/cd_hw/gfx.c',
  '../core/cd_hw/pcm.c',
  '../core/cd_hw/scd.c',
  '../core/input_hw/activator.c',
  '../core/input_hw/gamepad.c',
  '../core/input_hw/graphic_board.c',
  '../core/input_hw/input.c',
  '../core/input_hw/lightgun.c',
  '../core/input_hw/mouse.c',
  '../core/input_hw/paddle.c',
  '../core/input_hw/smash.c',
  '../core/input_hw/sportspad.c',
  '../core/input_hw/teamplayer.c',
  '../core/input_hw/terebi_oekaki.c',
  '../core/input_hw/xe_1ap.c',
  '../core/m68k/m68kcpu.c',
  '../core/m68k/s68kcpu.c',
  '../core/ntsc/md_ntsc.c',
  '../core/ntsc/sms_ntsc.c',
  '../core/sound/blip_buf.c',
  '../core/sound/eq.c',
  '../core/sound/psg.c',
  '../core/sound/sound.c',
  '../core/sound/ym2413.c',
  '../core/sound/ym2612.c',
  '../core/sound/ym3438.c',
  '../core/z80/z80.c',
]

genesis_plus_gx_deps = [
  dependency('highscore-1'),
  dependency('gio-2.0'),
  dependency('vorbisfile'),
  dependency('flac'),
  dependency('zlib'),
  dependency('libzstd'),
  dependency('libchdr'),
  cc.find_library('m', required: false),
]

genesis_plus_gx_c_args = [
  '-fvisibility=hidden',
  '-DUSE_32BPP_RENDERING',
  '-DLSB_FIRST',
  '-DBYTE_ODER=LITTE_ENDIAN',
  '-DHAVE_ZLIB',
  '-DMAXROMSIZE=33554432',
  '-DNDEBUG',
  '-DUSE_LIBVORBIS',
  '-DUSE_LIBCHDR',
  '-DHAVE_LROUND',
  '-DHAVE_STDINT_H',
  '-DZ7_ST',
  '-DHAVE_SYS_PARAM_H',
  '-DHAVE_YM3438_CORE',
  '-Wno-dangling-pointer',
  '-Wno-unused-result',
]

genesis_plus_gx_include_dirs = [
  include_directories('.'),
  include_directories('../core'),
  include_directories('../core/cart_hw'),
  include_directories('../core/cart_hw/svp'),
  include_directories('../core/cd_hw'),
  include_directories('../core/cd_hw/libchdr/include'),
  include_directories('../core/cd_hw/libchdr/deps/lzma-24.05/include'),
  include_directories('../core/input_hw'),
  include_directories('../core/m68k'),
  include_directories('../core/ntsc'),
  include_directories('../core/sound'),
  include_directories('../core/sound/minimp3'),
  include_directories('../core/z80'),
]

core_dir = get_option('core_install_dir')
if core_dir == ''
  libdir = get_option('prefix') / get_option('libdir')
  core_dir = libdir / 'highscore' / 'cores' / 'genesis-plus-gx'
endif

shared_library('genesis-plus-gx-highscore',
  genesis_plus_gx_source,
  dependencies : genesis_plus_gx_deps,
  c_args : genesis_plus_gx_c_args,
  include_directories : genesis_plus_gx_include_dirs,
  name_prefix: '',
  install_dir : core_dir,
  install : true
)

descriptor = configuration_data()
descriptor.set('version', version)
descriptor.set('commit', commit)
configure_file(
  input: 'genesis-plus-gx.highscore.in',
  output: 'genesis-plus-gx.highscore',
  configuration: descriptor,
  install_dir: core_dir,
  install: true
)

install_data(
  'genesis-plus-gx.license',
  install_dir: core_dir,
)
