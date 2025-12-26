/****************************************************************************
 *  libretro.c
 *
 *  Genesis Plus GX highscore port (based on libretro port)
 *
 *  Copyright Eke-Eke (2007-2022)
 *
 *  Copyright Daniel De Matteis (2012-2016)
 *
 *  Copyright Alice Mikhaylenko (2025)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

#include "genesis-plus-gx-highscore.h"

#include "shared.h"

#define MAX_PLAYERS 2

static GenesisPlusGXCore *core;

struct _GenesisPlusGXCore
{
  HsCore parent_instance;

  HsSoftwareContext *context;
  gint8 *frame_buffer;

  gint16 *audio_buffer;

  guint32 buttons[MAX_PLAYERS];
  gboolean pause_pressed;
  gboolean light_phaser_fire;
  double light_phaser_x;
  double light_phaser_y;

  char *save_path;

  gboolean bios_missing;

  guint32 bram_crc[2];

  int colorburst_phase;

  gboolean fm_audio;
  gboolean enable_light_phaser;
};

static uint8_t bram_format[0x40] =
{
  0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x00,0x00,0x00,0x00,0x40,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x53,0x45,0x47,0x41,0x5f,0x43,0x44,0x5f,0x52,0x4f,0x4d,0x00,0x01,0x00,0x00,0x00,
  0x52,0x41,0x4d,0x5f,0x43,0x41,0x52,0x54,0x52,0x49,0x44,0x47,0x45,0x5f,0x5f,0x5f
};

#define SOUND_FREQUENCY 44100
#define MAX_WIDTH 348
#define MAX_HEIGHT 576

#define CHUNK_SIZE 0x10000

t_config config;

char GG_ROM[256];
char AR_ROM[256];
char SK_ROM[256];
char SK_UPMEM[256];
char MD_BIOS[256];
char GG_BIOS[256];
char CD_BIOS_EU[] = "cd-bios-eu";
char CD_BIOS_US[] = "cd-bios-us";
char CD_BIOS_JP[] = "cd-bios-jp";
char MS_BIOS_EU[256];
char MS_BIOS_JP[256];
char MS_BIOS_US[256];

static void genesis_plus_gx_game_gear_core_init (HsGameGearCoreInterface *iface);
static void genesis_plus_gx_master_system_core_init (HsMasterSystemCoreInterface *iface);
static void genesis_plus_gx_mega_drive_core_init (HsMegaDriveCoreInterface *iface);
static void genesis_plus_gx_mega_cd_core_init (HsMegaCdCoreInterface *iface);
static void genesis_plus_gx_sg1000_core_init (HsSg1000CoreInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GenesisPlusGXCore, genesis_plus_gx_core, HS_TYPE_CORE,
                               G_IMPLEMENT_INTERFACE (HS_TYPE_GAME_GEAR_CORE, genesis_plus_gx_game_gear_core_init)
                               G_IMPLEMENT_INTERFACE (HS_TYPE_MASTER_SYSTEM_CORE, genesis_plus_gx_master_system_core_init)
                               G_IMPLEMENT_INTERFACE (HS_TYPE_MEGA_DRIVE_CORE, genesis_plus_gx_mega_drive_core_init)
                               G_IMPLEMENT_INTERFACE (HS_TYPE_MEGA_CD_CORE, genesis_plus_gx_mega_cd_core_init)
                               G_IMPLEMENT_INTERFACE (HS_TYPE_SG1000_CORE, genesis_plus_gx_sg1000_core_init))

void
ROMCheatUpdate (void)
{
}

void
error (char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  hs_core_log_valist (HS_CORE (core), HS_LOG_CRITICAL, fmt, ap);
  va_end(ap);
}

int
load_archive (char *filename, unsigned char *buffer, int max_size, char *extension)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *data = NULL;
  gsize size;
  const char *effective_path;
  g_autoptr (GFile) file = NULL;
  gboolean is_bios = FALSE;

  core->bios_missing = FALSE;

  hs_core_reset_used_firmware (HS_CORE (core));

  if (!g_strcmp0 (filename, CD_BIOS_US)) {
    effective_path = hs_core_query_firmware_path (HS_CORE (core), HS_MEGA_CD_FIRMWARE_NORTH_AMERICA);
    is_bios = TRUE;
  } else if (!g_strcmp0 (filename, CD_BIOS_JP)) {
    effective_path = hs_core_query_firmware_path (HS_CORE (core), HS_MEGA_CD_FIRMWARE_JAPAN);
    is_bios = TRUE;
  } else if (!g_strcmp0 (filename, CD_BIOS_EU)) {
    effective_path = hs_core_query_firmware_path (HS_CORE (core), HS_MEGA_CD_FIRMWARE_EUROPE);
    is_bios = TRUE;
  } else {
    effective_path = filename;
  }

  if (extension) {
    memcpy (extension, &effective_path[strlen (effective_path) - 3], 3);
    extension[3] = 0;
  }

  if (!effective_path) {
    if (is_bios)
      core->bios_missing = TRUE;

    return 0;
  }

  file = g_file_new_for_path (effective_path);

  if (!g_file_query_exists (file, NULL)) {
    if (is_bios)
      core->bios_missing = TRUE;

    return 0;
  }

  if (!g_file_load_contents (file, NULL, &data, &size, NULL, &error)) {
    hs_core_log (HS_CORE (core), HS_LOG_CRITICAL, "Failed to load file %s: %s", effective_path, error->message);
    return 0;
  }

  /* size limit */
  if (size > MAXROMSIZE) {
    hs_core_log (HS_CORE (core), HS_LOG_CRITICAL, "File %s is too large: %lu, maximum %d", effective_path, size, max_size);
    return 0;
  }

  size = MIN (size, max_size);

  memcpy (buffer, data, size);

  return size;
}

static const int GG_BUTTON_MAP[] = {
  INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT,
  INPUT_BUTTON1, INPUT_BUTTON2,
  INPUT_START,
};

static const int SMS_BUTTON_MAP[] = {
  INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT,
  INPUT_BUTTON1, INPUT_BUTTON2
};

static const int MD_BUTTON_MAP[] = {
  INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT,
  INPUT_A, INPUT_B, INPUT_C,
  INPUT_X, INPUT_Y, INPUT_Z,
  INPUT_START, INPUT_MODE
};

static void
update_gamepad_gg (int player, uint8 device)
{
  int buttons = 0;

  if (player != 0)
    return;

  for (int btn = 0; btn < HS_GAME_GEAR_N_BUTTONS; btn++) {
    if (core->buttons[player] & 1 << btn)
      buttons |= GG_BUTTON_MAP[btn];
  }

  input.pad[player * 4] = buttons;
}

static void
update_gamepad_md (int player, uint8 device)
{
  int buttons = 0;

  for (int btn = 0; btn < HS_MEGA_DRIVE_N_BUTTONS; btn++) {
    gboolean is_6b_button = btn == HS_MEGA_DRIVE_BUTTON_X ||
                            btn == HS_MEGA_DRIVE_BUTTON_Y ||
                            btn == HS_MEGA_DRIVE_BUTTON_Z ||
                            btn == HS_MEGA_DRIVE_BUTTON_MODE;

    if (input.dev[player] == DEVICE_PAD3B && is_6b_button)
      continue;

    if (core->buttons[player] & 1 << btn)
      buttons |= MD_BUTTON_MAP[btn];
  }

  input.pad[player * 4] = buttons;
}

static void
update_gamepad_sms (int player, uint8 device)
{
  int buttons = 0;

  for (int btn = 0; btn < HS_MASTER_SYSTEM_N_BUTTONS; btn++) {
    if (core->buttons[player] & 1 << btn)
      buttons |= SMS_BUTTON_MAP[btn];
  }

  if (player == 0 && core->pause_pressed)
    buttons |= INPUT_START;

  input.pad[player * 4] = buttons;
}

void
osd_input_update (void)
{
  HsPlatform platform = hs_core_get_platform (HS_CORE (core));
  HsPlatform base_platform = hs_platform_get_base_platform (platform);
  int player = 0;
  int buttons = 0;

  for (int i = 0; i < MAX_INPUTS; i++) {
    switch (input.dev[i]) {
      case DEVICE_PAD2B:
      case DEVICE_PAD3B:
      case DEVICE_PAD6B:
        if (base_platform == HS_PLATFORM_MEGA_DRIVE)
          update_gamepad_md (i, input.dev[i]);
        else if (base_platform == HS_PLATFORM_GAME_GEAR)
          update_gamepad_gg (i, input.dev[i]);
        else
          update_gamepad_sms (i, input.dev[i]);

        player++;
        break;
      case DEVICE_LIGHTGUN:
        input.analog[i][0] = (int16) (core->light_phaser_x * bitmap.viewport.w);
        input.analog[i][1] = (int16) (core->light_phaser_y * bitmap.viewport.h);

        if (core->light_phaser_fire)
          buttons |= INPUT_A;

        input.pad[i] = buttons;
        break;
      case NO_DEVICE:
        break;
      default:
        g_assert_not_reached ();
    }
  }
}

static gboolean
update_fm_audio (GenesisPlusGXCore *self)
{
  HsPlatform platform = hs_core_get_platform (HS_CORE (self));

  if (platform != HS_PLATFORM_MASTER_SYSTEM) {
    config.ym2413 = 0;
    return FALSE;
  }

  gboolean was_fm_audio = !!config.ym2413;

  if (was_fm_audio == self->fm_audio)
    return FALSE;

  config.ym2413 = self->fm_audio ? 1 : 0;

  return TRUE;
}

static void
set_defaults (GenesisPlusGXCore *self)
{
  HsPlatform platform = hs_core_get_platform (HS_CORE (self));

  config.psg_preamp     = 150;
  config.fm_preamp      = 100;
  config.cdda_volume    = 100;
  config.pcm_volume     = 100;
  config.hq_fm          = 1; /* high-quality FM resampling (slower) */
  config.hq_psg         = 1; /* high-quality PSG resampling (slower) */
  config.filter         = 0; /* no filter */
  config.lp_range       = 0x9999; /* 0.6 in 0.16 fixed point */
  config.low_freq       = 880;
  config.high_freq      = 5000;
  config.lg             = 100;
  config.mg             = 100;
  config.hg             = 100;
  config.ym2612         = YM2612_DISCRETE;
  config.mono           = 0; /* STEREO output */
#ifdef HAVE_YM3438_CORE
  config.ym3438         = 0;
#endif

  /* system options */
  config.system         = 0; /* AUTO */
  config.region_detect  = 0; /* AUTO */
  config.vdp_mode       = 0; /* AUTO */
  config.master_clock   = 0; /* AUTO */
  config.force_dtack    = 0;
  config.addr_error     = 1;
  config.bios           = 0;
  config.lock_on        = 0;
  config.add_on         = HW_ADDON_AUTO;
  config.no_sprite_limit = 0;
  config.enhanced_vscroll = 0;
  config.enhanced_vscroll_limit = 8;

  /* video options */
  if (platform == HS_PLATFORM_GAME_GEAR)
    config.overscan = 0; // no overscan
  else
    config.overscan = 3; // full overscan
  config.aspect_ratio = 0;
  config.render = 1;

  input.system[0] = SYSTEM_GAMEPAD;
  input.system[1] = SYSTEM_GAMEPAD;
  for (int i = 0; i < MAX_INPUTS; i++)
    config.input[i].padtype = DEVICE_PAD2B | DEVICE_PAD3B | DEVICE_PAD6B;
}

static gboolean
load_save_ram (GenesisPlusGXCore  *self, GError **error)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *data = NULL;
  gsize size;

  if (!sram.on)
    return TRUE;

  file = g_file_new_for_path (self->save_path);
  if (!g_file_query_exists (file, NULL))
    return TRUE;

  if (!g_file_load_contents (file, NULL, &data, &size, NULL, error))
    return FALSE;

  // Genesis Plus GX saves are padded with 0xFF, BlastEm saves aren't
  // If the save isn't padded, do it here so that we have interoperability
  gboolean padded = TRUE;
  for (gsize i = 0; i < size; i += 2) {
    guint8 byte = data[i];
    if (byte != 0xFF) {
      padded = FALSE;
      break;
    }
  }

  int max_size = padded ? 0x10000 : (0x10000 >> 1);
  if (size > max_size) {
    hs_core_log (HS_CORE (self), HS_LOG_WARNING, "SRAM file is too large: %lu, expected < %d", size, max_size);
    size = max_size;
  }

  if (padded) {
    memcpy (sram.sram, data, size);
  } else {
    for (gsize i = 0; i < size; i++) {
      sram.sram[i * 2] = 0xFF;
      sram.sram[i * 2 + 1] = data[i];
    }
  }

  return TRUE;
}

static gboolean
load_backup_ram (GenesisPlusGXCore  *self, GError **error)
{
  g_autoptr (GFile) save_dir = g_file_new_for_path (self->save_path);

  if (!g_file_query_exists (save_dir, NULL) &&
      !g_file_make_directory_with_parents (save_dir, NULL, error)) {
    return FALSE;
  }

  g_autoptr (GFile) system_file = g_file_get_child (save_dir, "system.brm");

  if (g_file_query_exists (system_file, NULL)) {
    g_autoptr (GFileInputStream) stream = g_file_read (system_file, NULL, error);
    if (!stream)
      return FALSE;

    if (!g_input_stream_read (G_INPUT_STREAM (stream), scd.bram, 0x2000, NULL, error))
      return FALSE;

    if (!g_input_stream_close (G_INPUT_STREAM (stream), NULL, error))
      return FALSE;

    self->bram_crc[0] = crc32 (0, scd.bram, 0x2000);
  } else {
    /* force internal backup RAM format (does not use previous region backup RAM) */
    scd.bram[0x1fff] = 0;
  }

  /* check if internal backup RAM is correctly formatted */
  if (memcmp (scd.bram + 0x2000 - 0x20, bram_format + 0x20, 0x20)) {
    /* clear internal backup RAM */
    memset (scd.bram, 0x00, 0x2000 - 0x40);

    /* internal Backup RAM size fields */
    bram_format[0x10] = bram_format[0x12] = bram_format[0x14] = bram_format[0x16] = 0x00;
    bram_format[0x11] = bram_format[0x13] = bram_format[0x15] = bram_format[0x17] = (sizeof (scd.bram) / 64) - 3;

    /* format internal backup RAM */
    memcpy (scd.bram + 0x2000 - 0x40, bram_format, 0x40);

    /* clear CRC to force file saving (in case previous region backup RAM was also formatted) */
    self->bram_crc[0] = 0;
  }

  if (!scd.cartridge.id)
    return TRUE;

  g_autoptr (GFile) cart_file = g_file_get_child (save_dir, "cart.brm");

  if (g_file_query_exists (cart_file, NULL)) {
    g_autoptr (GFileInputStream) stream = g_file_read (cart_file, NULL, error);
    if (!stream)
      return FALSE;

    int file_size = scd.cartridge.mask + 1;
    int done = 0;

    /* Read into buffer (2k blocks) */
    while (file_size > CHUNK_SIZE) {
      if (!g_input_stream_read (G_INPUT_STREAM (stream), scd.cartridge.area + done, CHUNK_SIZE, NULL, error))
        return FALSE;

      done += CHUNK_SIZE;
      file_size -= CHUNK_SIZE;
    }

    /* Read remaining bytes */
    if (file_size) {
      if (!g_input_stream_read (G_INPUT_STREAM (stream), scd.cartridge.area + done, file_size, NULL, error))
        return FALSE;
    }

    if (!g_input_stream_close (G_INPUT_STREAM (stream), NULL, error))
      return FALSE;

    /* update CRC */
    self->bram_crc[1] = crc32 (0, scd.cartridge.area, scd.cartridge.mask + 1);
  }

  /* check if cartridge backup RAM is correctly formatted */
  if (memcmp (scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, bram_format + 0x20, 0x20)) {
    /* clear cartridge backup RAM */
    memset (scd.cartridge.area, 0x00, scd.cartridge.mask + 1);

    /* Cartridge Backup RAM size fields */
    bram_format[0x10] = bram_format[0x12] = bram_format[0x14] = bram_format[0x16] = (((scd.cartridge.mask + 1) / 64) - 3) >> 8;
    bram_format[0x11] = bram_format[0x13] = bram_format[0x15] = bram_format[0x17] = (((scd.cartridge.mask + 1) / 64) - 3) & 0xff;

    /* format cartridge backup RAM */
    memcpy (scd.cartridge.area + scd.cartridge.mask + 1 - 0x40, bram_format, 0x40);
  }

  return TRUE;
}

static gboolean
save_backup_ram (GenesisPlusGXCore  *self, GError **error)
{
  g_autoptr (GFile) save_dir = g_file_new_for_path (self->save_path);

  if (!g_file_query_exists (save_dir, NULL) &&
      !g_file_make_directory_with_parents (save_dir, NULL, error)) {
    return FALSE;
  }

  /* verify that internal backup RAM has been modified */
  if (crc32 (0, scd.bram, 0x2000) != self->bram_crc[0]) {
    /* check if it is correctly formatted before saving */
    if (!memcmp (scd.bram + 0x2000 - 0x20, bram_format + 0x20, 0x20)) {
      g_autoptr (GFile) system_file = g_file_get_child (save_dir, "system.brm");

      if (!g_file_replace_contents (system_file, (char *) scd.bram, 0x2000, NULL, FALSE,
                                    G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, error)) {
        return FALSE;
      }

      /* update CRC */
      self->bram_crc[0] = crc32 (0, scd.bram, 0x2000);
    }
  }

  /* verify that cartridge backup RAM has been modified */
  if (scd.cartridge.id && (crc32 (0, scd.cartridge.area, scd.cartridge.mask + 1) != self->bram_crc[1])) {
    /* check if it is correctly formatted before saving */
    if (!memcmp (scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, bram_format + 0x20, 0x20)) {
      g_autoptr (GFile) cart_file = g_file_get_child (save_dir, "cart.brm");
      g_autoptr (GFileOutputStream) stream =
        g_file_replace (cart_file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, error);

      if (!stream)
        return FALSE;

      int file_size = scd.cartridge.mask + 1;
      int done = 0;

      /* Write to file (2k blocks) */
      while (file_size > CHUNK_SIZE) {
        if (!g_output_stream_write (G_OUTPUT_STREAM (stream), scd.cartridge.area + done, CHUNK_SIZE, NULL, error))
          return FALSE;

        done += CHUNK_SIZE;
        file_size -= CHUNK_SIZE;
      }

      /* Write remaining bytes */
      if (file_size) {
        if (!g_output_stream_write (G_OUTPUT_STREAM (stream), scd.cartridge.area + done, file_size, NULL, error))
          return FALSE;
      }

      if (!g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, error))
        return FALSE;

      /* update CRC */
      self->bram_crc[1] = crc32 (0, scd.cartridge.area, scd.cartridge.mask + 1);
    }
  }

  return TRUE;
}

static gboolean
finish_init (GenesisPlusGXCore *self, GError **error)
{
  HsPlatform platform = hs_core_get_platform (HS_CORE (self));
  HsPlatform base_platform = hs_platform_get_base_platform (platform);

  if (platform != HS_PLATFORM_MEGA_CD && !load_save_ram (self, error))
    return FALSE;

  system_reset ();

  if (platform == HS_PLATFORM_MEGA_CD && !load_backup_ram (self, error))
    return FALSE;

  io_init ();
  input_reset ();

  if (base_platform == HS_PLATFORM_GAME_GEAR) {
    config.input[0].padtype = DEVICE_PAD2B;
    input.system[0] = SYSTEM_GAMEPAD;

    input.system[1] = NO_SYSTEM;
  } else if (base_platform == HS_PLATFORM_MEGA_DRIVE) {
    for (int i = 0; i < HS_MEGA_DRIVE_MAX_PLAYERS; i++) {
      config.input[i].padtype = DEVICE_PAD6B;
      input.system[i] = SYSTEM_GAMEPAD;
    }
  } else {
    if (self->enable_light_phaser) {
      input.system[0] = SYSTEM_LIGHTPHASER;
      input.system[1] = NO_SYSTEM;
    } else {
      for (int i = 0; i < HS_MASTER_SYSTEM_MAX_PLAYERS; i++) {
        config.input[i].padtype = DEVICE_PAD2B;
        input.system[i] = SYSTEM_GAMEPAD;
      }
    }
  }

  old_system[0] = input.system[0];
  old_system[1] = input.system[1];

  return TRUE;
}

static gboolean
genesis_plus_gx_core_load_rom (HsCore      *core,
                               const char **rom_paths,
                               int          n_rom_paths,
                               const char  *save_path,
                               GError     **error)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);
  HsPlatform platform = hs_core_get_platform (core);

  self->context = hs_core_create_software_context (HS_CORE (self), MAX_WIDTH, MAX_HEIGHT, HS_PIXEL_FORMAT_B8G8R8X8);
  self->frame_buffer = g_new0 (gint8, MAX_WIDTH * MAX_HEIGHT * 4);

  memset(&bitmap, 0, sizeof (bitmap));
  bitmap.width = MAX_WIDTH;
  bitmap.height = MAX_HEIGHT;
  bitmap.pitch = MAX_WIDTH * 4;
  bitmap.data = (guchar *) self->frame_buffer;

  self->audio_buffer = g_new0 (gint16, 3068);

  set_defaults (self);
  update_fm_audio (self);

  // TODO clear disk interface

  self->bios_missing = TRUE;
  if (!load_rom ((char *) rom_paths[0])) {
    if (self->bios_missing) {
      const char *region_name;
      switch (region_code) {
        case REGION_USA:
          region_name = "US";
          break;
        case REGION_EUROPE:
          region_name = "EU";
          break;
        default:
          region_name = "JP";
          break;
      }

      g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_MISSING_FIRMWARE, "Missing Sega CD %s BIOS", region_name);
      return FALSE;
    }

    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to load ROM");
    return FALSE;
  }

  if (platform == HS_PLATFORM_MEGA_CD)
    g_assert (cdd.loaded);

  audio_init (SOUND_FREQUENCY, 0);
  system_init ();

  g_set_str (&self->save_path, save_path);

  if (!finish_init (self, error))
    return FALSE;

  return TRUE;
}

static gboolean
genesis_plus_gx_core_reset (HsCore *core, gboolean hard, GError **error)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);

  gen_reset (hard);

  if (hard) {
    update_fm_audio (self);
    system_init ();

    if (!finish_init (self, error))
      return FALSE;
  }

  return TRUE;
}

static void
genesis_plus_gx_core_poll_input (HsCore *core, HsInputState *input_state)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);
  HsPlatform platform = hs_core_get_platform (core);
  HsPlatform base_platform = hs_platform_get_base_platform (platform);

  if (base_platform == HS_PLATFORM_GAME_GEAR)
    self->buttons[0] = input_state->game_gear.buttons;

  if (base_platform == HS_PLATFORM_MASTER_SYSTEM) {
    for (int i = 0; i < HS_MASTER_SYSTEM_MAX_PLAYERS; i++)
      self->buttons[i] = input_state->master_system.pad_buttons[i];

    self->pause_pressed = input_state->master_system.pause_button;

    self->light_phaser_x = input_state->master_system.light_phaser_x;
    self->light_phaser_y = input_state->master_system.light_phaser_y;
    self->light_phaser_fire = input_state->master_system.light_phaser_fire;
  }

  if (base_platform == HS_PLATFORM_MEGA_DRIVE) {
    for (int i = 0; i < HS_MEGA_DRIVE_MAX_PLAYERS; i++)
      self->buttons[i] = input_state->mega_drive.pad_buttons[i];
  }

  if (base_platform == HS_PLATFORM_SG1000) {
    for (int i = 0; i < HS_SG1000_MAX_PLAYERS; i++)
      self->buttons[i] = input_state->sg1000.pad_buttons[i];

    self->pause_pressed = input_state->sg1000.pause_button;
  }
}

static void
genesis_plus_gx_core_run_frame (HsCore *core)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);
  HsPlatform platform = hs_core_get_platform (core);
  gboolean was_interlaced = interlaced;

  switch (platform) {
    case HS_PLATFORM_MEGA_DRIVE:
      system_frame_gen (0);
      break;
    case HS_PLATFORM_MEGA_CD:
      system_frame_scd (0);
      break;
    case HS_PLATFORM_GAME_GEAR:
    case HS_PLATFORM_MASTER_SYSTEM:
    case HS_PLATFORM_SG1000:
      system_frame_sms (0);
      break;
    default:
      g_assert_not_reached ();
  }

  int height_multiplier = (was_interlaced && interlaced) ? 2 : 1;
  hs_software_context_set_area (self->context,
                                &HS_RECTANGLE_INIT (0, 0,
                                                    bitmap.viewport.w + bitmap.viewport.x * 2,
                                                    (bitmap.viewport.h + bitmap.viewport.y * 2) * height_multiplier));

  if (platform != HS_PLATFORM_GAME_GEAR) {
    hs_software_context_set_overscan (self->context,
                                      &HS_BORDER_INIT (bitmap.viewport.x, bitmap.viewport.y * height_multiplier));
  }

  // Treat the first field after switching to interlacing as progressive, but with double rowstride
  // to avoid showing the (still incomplete and filled with garbage data!) second field
  // Once the second field has been filled in, we'll switch frontend to interlacing too
  if (interlaced && !was_interlaced)
    hs_software_context_set_row_stride (self->context, MAX_WIDTH * 4 * 2);
  else
    hs_software_context_set_row_stride (self->context, MAX_WIDTH * 4);

  HsInterlacingMode mode;

  if (was_interlaced && interlaced) {
    if (odd_frame)
      mode = HS_INTERLACING_EVEN_FIELD;
    else
      mode = HS_INTERLACING_ODD_FIELD;
  } else {
    mode = HS_INTERLACING_NONE;
  }

  hs_software_context_set_interlacing (self->context, mode);

  hs_software_context_set_colorburst_phase (self->context, self->colorburst_phase);

  if (mode != HS_INTERLACING_ODD_FIELD && vdp_pal)
    self->colorburst_phase ^= 1;
  else if (!vdp_pal)
    self->colorburst_phase = 0;

  int n_lines = bitmap.viewport.h + bitmap.viewport.y * 2;

  if (interlaced)
    n_lines *= 2;

  memcpy (hs_software_context_acquire_framebuffer (self->context),
          bitmap.data,
          MAX_WIDTH * n_lines * 4);

  hs_software_context_release_framebuffer (self->context);

  int samples = audio_update (self->audio_buffer);
  hs_core_play_samples (core, self->audio_buffer, samples * 2);
}

static void
genesis_plus_gx_core_stop (HsCore *core)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);
  HsPlatform platform = hs_core_get_platform (core);
  g_autoptr (GError) error = NULL;

  if (platform == HS_PLATFORM_MEGA_CD && !save_backup_ram (self, &error))
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to save backup RAM: %s", error->message);

  audio_shutdown ();

  g_clear_object (&self->context);
  g_clear_pointer (&self->frame_buffer, g_free);
  g_clear_pointer (&self->audio_buffer, g_free);
  g_clear_pointer (&self->save_path, g_free);
}

static gboolean
genesis_plus_gx_core_reload_save (HsCore      *core,
                                  const char  *save_path,
                                  GError     **error)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);

  system_init ();

  g_set_str (&self->save_path, save_path);

  if (!finish_init (self, error))
    return FALSE;

  return TRUE;
}

static int
get_sram_size (void)
{
  for (int i = 0xFFFF; i >= 0; i--) {
    if (sram.sram[i] != 0xff)
      return i + 1;
  }

  return 0;
}

static gboolean
genesis_plus_gx_core_sync_save (HsCore  *core,
                                GError **error)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);
  HsPlatform platform = hs_core_get_platform (core);

  if (platform == HS_PLATFORM_MEGA_CD) {
    g_assert (!sram.on);

    if (!save_backup_ram (self, error))
      return FALSE;
  } else {
    if (!sram.on)
      return TRUE;

    int size = get_sram_size ();
    if (size == 0)
      return TRUE;

    if (!g_file_set_contents (self->save_path, (char *) sram.sram, size, error))
      return FALSE;
  }

  return TRUE;
}

static void
genesis_plus_gx_core_save_state (HsCore          *core,
                                 const char      *path,
                                 HsStateCallback  callback)
{
  g_autofree char *data = g_new0 (char, STATE_SIZE);
  int size = state_save ((guint8 *) data);
  GError *error = NULL;

  if (!g_file_set_contents (path, data, size, &error)) {
    callback (core, &error);
    return;
  }

  callback (core, NULL);
}

static void
genesis_plus_gx_core_load_state (HsCore          *core,
                                 const char      *path,
                                 HsStateCallback  callback)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);
  g_autofree char *data = NULL;
  gsize size;
  GError *error = NULL;

  if (!g_file_get_contents (path, &data, &size, &error)) {
    callback (core, &error);
    return;
  }

  if (size > STATE_SIZE) {
    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Too large savestate size: %lu, expected %d", size, STATE_SIZE);
    callback (core, &error);
    return;
  }

  if (update_fm_audio (self)) {
    system_init ();

    if (!finish_init (self, &error)) {
      callback (core, &error);
      return;
    }
  }

  if (!state_load ((guint8 *) data)) {
    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to load state");
    callback (core, &error);
    return;
  }

  callback (core, NULL);
}

static double
genesis_plus_gx_core_get_frame_rate (HsCore *core)
{
  return (double) system_clock / (double) lines_per_frame / (double) MCYCLES_PER_LINE;
}

static double
genesis_plus_gx_core_get_aspect_ratio (HsCore *core)
{

  int width = bitmap.viewport.w + bitmap.viewport.x * 2;
  int height = bitmap.viewport.h + bitmap.viewport.y * 2;
  double par;

  if (hs_core_get_platform (core) == HS_PLATFORM_GAME_GEAR) {
    par = 6.0 / 5.0;
  } else {
    gboolean is_h40 = bitmap.viewport.w == 320; /* Could be read directly from the register as well. */
    double dotrate = system_clock / (is_h40 ? 8.0 : 10.0);
    double videosamplerate = vdp_pal ? 14750000.0 : 135000000.0 / 11.0;

    par = videosamplerate / dotrate / 2.0;
  }

  return par * ((double) width / (double) height);
}

static double
genesis_plus_gx_core_get_sample_rate (HsCore *core)
{
  return SOUND_FREQUENCY;
}

static HsRegion
genesis_plus_gx_core_get_region (HsCore *core)
{
  return vdp_pal ? HS_REGION_PAL : HS_REGION_NTSC;
}

static void
genesis_plus_gx_core_finalize (GObject *object)
{
  G_OBJECT_CLASS (genesis_plus_gx_core_parent_class)->finalize (object);

  core = NULL;
}

static void
genesis_plus_gx_core_class_init (GenesisPlusGXCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HsCoreClass *core_class = HS_CORE_CLASS (klass);

  object_class->finalize = genesis_plus_gx_core_finalize;

  core_class->load_rom = genesis_plus_gx_core_load_rom;
  core_class->reset = genesis_plus_gx_core_reset;
  core_class->poll_input = genesis_plus_gx_core_poll_input;
  core_class->run_frame = genesis_plus_gx_core_run_frame;
  core_class->stop = genesis_plus_gx_core_stop;

  core_class->reload_save = genesis_plus_gx_core_reload_save;
  core_class->sync_save = genesis_plus_gx_core_sync_save;

  core_class->save_state = genesis_plus_gx_core_save_state;
  core_class->load_state = genesis_plus_gx_core_load_state;

  core_class->get_frame_rate = genesis_plus_gx_core_get_frame_rate;
  core_class->get_aspect_ratio = genesis_plus_gx_core_get_aspect_ratio;

  core_class->get_sample_rate = genesis_plus_gx_core_get_sample_rate;

  core_class->get_region = genesis_plus_gx_core_get_region;
}

static void
genesis_plus_gx_core_init (GenesisPlusGXCore *self)
{
  g_assert (!core);

  core = self;
}

static void
genesis_plus_gx_game_gear_core_init (HsGameGearCoreInterface *iface)
{
}

static void
genesis_plus_gx_master_system_core_set_enable_fm_audio (HsMasterSystemCore *core,
                                                        gboolean            enable_fm_audio)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);

  self->fm_audio = enable_fm_audio;
}

static void
genesis_plus_gx_master_system_core_set_enable_light_phaser (HsMasterSystemCore *core,
                                                            gboolean            enable_light_phaser)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);

  self->enable_light_phaser = enable_light_phaser;
}

static void
genesis_plus_gx_master_system_core_init (HsMasterSystemCoreInterface *iface)
{
  iface->set_enable_fm_audio = genesis_plus_gx_master_system_core_set_enable_fm_audio;
  iface->set_enable_light_phaser = genesis_plus_gx_master_system_core_set_enable_light_phaser;
}

static void
genesis_plus_gx_mega_drive_core_init (HsMegaDriveCoreInterface *iface)
{
}

static void
genesis_plus_gx_mega_cd_core_init (HsMegaCdCoreInterface *iface)
{
}

static void
genesis_plus_gx_sg1000_core_init (HsSg1000CoreInterface *iface)
{
}

GType
hs_get_core_type (void)
{
  return GENESIS_PLUS_GX_TYPE_CORE;
}
