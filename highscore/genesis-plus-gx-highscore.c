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

static GenesisPlusGXCore *core;

struct _GenesisPlusGXCore
{
  HsCore parent_instance;

  HsSoftwareContext *context;
  gint8 *frame_buffer;

  gint16 *audio_buffer;

  guint32 pad_buttons[HS_MEGA_DRIVE_MAX_PLAYERS];
  char *save_path;
};

#define SOUND_FREQUENCY 44100
#define MAX_WIDTH 348
#define MAX_HEIGHT 576

t_config config;

char GG_ROM[256];
char AR_ROM[256];
char SK_ROM[256];
char SK_UPMEM[256];
char MD_BIOS[256];
char GG_BIOS[256];
char MS_BIOS_EU[256];
char MS_BIOS_JP[256];
char MS_BIOS_US[256];
char CD_BIOS_EU[256];
char CD_BIOS_US[256];
char CD_BIOS_JP[256];
char CD_BRAM_JP[256];
char CD_BRAM_US[256];
char CD_BRAM_EU[256];
char CART_BRAM[256];

static void genesis_plus_gx_mega_drive_core_init (HsMegaDriveCoreInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GenesisPlusGXCore, genesis_plus_gx_core, HS_TYPE_CORE,
                               G_IMPLEMENT_INTERFACE (HS_TYPE_MEGA_DRIVE_CORE, genesis_plus_gx_mega_drive_core_init))

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

  if (extension) {
    memcpy (extension, &filename[strlen (filename) - 3], 3);
    extension[3] = 0;
  }

  if (!g_file_get_contents (filename, &data, &size, &error)) {
    hs_core_log (HS_CORE (core), HS_LOG_CRITICAL, "Failed to load file %s: %s", filename, error->message);
    return 0;
  }

  /* size limit */
  if (size > MAXROMSIZE) {
    hs_core_log (HS_CORE (core), HS_LOG_CRITICAL, "File %s is too large: %lu, maximum %d", filename, size, max_size);
    return 0;
  }

  size = MIN (size, max_size);

  memcpy (buffer, data, size);
  return size;
}

static const int BUTTON_MAP[] = {
  INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT,
  INPUT_A, INPUT_B, INPUT_C,
  INPUT_X, INPUT_Y, INPUT_Z,
  INPUT_START, INPUT_MODE
};

void
osd_input_update (void)
{
  for (int i = 0; i < HS_MEGA_DRIVE_MAX_PLAYERS; i++) {
    int buttons = 0;

    for (HsMegaDriveButton btn = 0; btn < HS_MEGA_DRIVE_N_BUTTONS; btn++) {
      gboolean is_6b_button = btn == HS_MEGA_DRIVE_BUTTON_X ||
                              btn == HS_MEGA_DRIVE_BUTTON_Y ||
                              btn == HS_MEGA_DRIVE_BUTTON_Z ||
                              btn == HS_MEGA_DRIVE_BUTTON_MODE;

      if (input.dev[i] == DEVICE_PAD3B && is_6b_button)
        continue;

      if (core->pad_buttons[i] & 1 << btn)
        buttons |= BUTTON_MAP[btn];
    }

    input.pad[i] = buttons;
  }
}

static void
set_defaults (void)
{
  config.psg_preamp     = 150;
  config.fm_preamp      = 100;
  config.cdda_volume    = 100;
  config.pcm_volume     = 100;
  config.hq_fm          = 1; /* high-quality FM resampling (slower) */
  config.hq_psg         = 1; /* high-quality PSG resampling (slower) */
  config.filter         = 1; /* no filter */
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
  config.overscan = 3; // full overscan
  config.aspect_ratio = 0;
  config.render = 1;
}

static gboolean
load_save (GenesisPlusGXCore  *self,
           const char         *save_path,
           GError            **error)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *data = NULL;
  gsize size;

  g_set_str (&self->save_path, save_path);

  if (!sram.on)
    return TRUE;

  file = g_file_new_for_path (save_path);
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

  // load backup ram for cd

  return TRUE;
}

static void
finish_init (GenesisPlusGXCore *self)
{
  system_reset ();

  for (int i = 0; i < HS_MEGA_DRIVE_MAX_PLAYERS; i++) {
    config.input[i].padtype = DEVICE_PAD6B;
    input.system[i] = SYSTEM_GAMEPAD;
  }

  io_init ();
  input_reset ();
}

static gboolean
genesis_plus_gx_core_load_rom (HsCore      *core,
                               const char **rom_paths,
                               int          n_rom_paths,
                               const char  *save_path,
                               GError     **error)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);

  self->context = hs_core_create_software_context (HS_CORE (self), MAX_WIDTH, MAX_HEIGHT, HS_PIXEL_FORMAT_B8G8R8X8);
  self->frame_buffer = g_new0 (gint8, MAX_WIDTH * MAX_HEIGHT * 4);

  memset(&bitmap, 0, sizeof (bitmap));
  bitmap.width = MAX_WIDTH;
  bitmap.height = MAX_HEIGHT;
  bitmap.pitch = MAX_WIDTH * 4;
  bitmap.data = (guchar *) self->frame_buffer;

  self->audio_buffer = g_new0 (gint16, 3068);

  set_defaults ();

  // TODO load bios
  // TODO clear disk interface
  // TODO multiple CDs

  if (!load_rom ((char *) rom_paths[0])) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to load ROM");
    return FALSE;
  }

  audio_init (SOUND_FREQUENCY, 0);
  system_init ();

  if (!load_save (self, save_path, error))
    return FALSE;

  finish_init (self);

  return TRUE;
}

static gboolean
genesis_plus_gx_core_reset (HsCore *core, gboolean hard, GError **error)
{
  gen_reset (hard);

  return TRUE;
}

static void
genesis_plus_gx_core_poll_input (HsCore *core, HsInputState *input_state)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);

  for (int i = 0; i < HS_MEGA_DRIVE_MAX_PLAYERS; i++)
    self->pad_buttons[i] = input_state->mega_drive.pad_buttons[i];
}

static void
genesis_plus_gx_core_run_frame (HsCore *core)
{
  GenesisPlusGXCore *self = GENESIS_PLUS_GX_CORE (core);
  gboolean was_interlaced = interlaced;

//  if (hs_core_get_platform (core) == HS_PLATFORM_SEGA_CD) TODO
//  system_frame_scd (0);
// else
  system_frame_gen (0);

  int height_multiplier = (was_interlaced && interlaced) ? 2 : 1;
  hs_software_context_set_area (self->context,
                                &HS_RECTANGLE_INIT (0, 0,
                                                    bitmap.viewport.w + bitmap.viewport.x * 2,
                                                    (bitmap.viewport.h + bitmap.viewport.y * 2) * height_multiplier));
  hs_software_context_set_overscan (self->context,
                                    &HS_BORDER_INIT (bitmap.viewport.x, bitmap.viewport.y * height_multiplier));

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

  int  n_lines = bitmap.viewport.h + bitmap.viewport.y * 2;

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

  if (!load_save (self, save_path, error))
    return FALSE;

  finish_init (self);

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

  if (!sram.on)
    return TRUE;

  int size = get_sram_size ();
  if (size == 0)
    return TRUE;

  if (!g_file_set_contents (self->save_path, (char *) sram.sram, size, error))
    return FALSE;

// if (system_hw == SYSTEM_MCD)
//   bram_save();

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
  gboolean is_h40 = bitmap.viewport.w == 320; /* Could be read directly from the register as well. */
  double dotrate = system_clock / (is_h40 ? 8.0 : 10.0);
  double videosamplerate = vdp_pal ? 14750000.0 : 135000000.0 / 11.0;

  int width = bitmap.viewport.w + bitmap.viewport.x * 2;
  int height = bitmap.viewport.h + bitmap.viewport.y * 2;

  return (videosamplerate / dotrate) * ((double) width / ((double) height * 2.0));
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
genesis_plus_gx_mega_drive_core_init (HsMegaDriveCoreInterface *iface)
{
}

GType
hs_get_core_type (void)
{
  return GENESIS_PLUS_GX_TYPE_CORE;
}
