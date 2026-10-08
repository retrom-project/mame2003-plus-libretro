/* Exercise the real driver registrations and MAME serializer without ROM files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver.h"
#include "state.h"

static UINT8 rom[2][0x14000];
static int rom_instance;
static UINT8 *mapped_bank;
static int last_irq;
static void test_setbank(int index, UINT8 *base) { if (index == 1) mapped_bank = base; }
#undef cpu_setbank
#define cpu_setbank(index, base) test_setbank(index, base)
#include "../src/drivers/renegade.c"
#include "../src/vidhrdw/renegade_vidhrdw.c"

static struct GameDriver test_driver = { .name = "renegade" };
static struct RunningMachine machine = { .gamedrv = &test_driver, .sample_rate = 48000 };
struct RunningMachine *Machine = &machine;
struct tile_info tile_info;
UINT8 *videoram;
static void quiet_log(enum retro_log_level level, const char *format, ...) {}
retro_log_printf_t log_cb = quiet_log;
void usrintf_showmessage(const char *format, ...) {}
UINT8 *memory_region(int region) { return rom[rom_instance]; }
void cpu_set_irq_line(int cpu, int line, int state) { last_irq = line; }
UINT32 tilemap_scan_rows(UINT32 col, UINT32 row, UINT32 cols, UINT32 rows) { return row * cols + col; }
struct tilemap *tilemap_create(void (*get_info)(int), UINT32 (*scan)(UINT32, UINT32, UINT32, UINT32),
                              int type, int width, int height, int cols, int rows) {
    return (struct tilemap *)rom;
}
void tilemap_set_transparent_pen(struct tilemap *map, int pen) {}
void tilemap_set_scrolldx(struct tilemap *map, int normal, int flipped) {}

extern size_t state_get_dump_size(void);
static int failures;
#define CHECK(test, description) do { if (!(test)) { fprintf(stderr, "FAIL: %s\n", description); failures++; } } while (0)
int main(void) {
    UINT8 *state;
    size_t size;
    int expected_irq;
    state_save_reset();
    state_save_set_current_tag(0);
    init_renegade();
    CHECK(video_start_renegade() == 0, "video start");
    renegade_scroll0_w(0, 0x18);
    renegade_scroll1_w(0, 0x02);
    bank = 0;
    bankswitch_w(0, 1);
#ifdef RETROM_MAME_PLUS
    from_main = 0x51; from_mcu = 0xa7; main_sent = 1; mcu_sent = 1;
    ddr_a = 0x5a; ddr_b = 0xc3; ddr_c = 0x96;
    port_a_in = 0x12; port_b_in = 0x34; port_c_in = 0x56;
    port_a_out = 0x65; port_b_out = 0x43; port_c_out = 0x21;
#else
    memcpy(mcu_buffer, "ABCDEF", MCU_BUFFER_MAX);
    mcu_input_size = 3; mcu_output_byte = 2; mcu_key = 9;
#endif
    size = state_get_dump_size();
    state = calloc(1, size);
    CHECK(state != NULL && size >= 24, "checkpoint allocated");
    state_save_save_begin(state);
    CHECK(state_save_save_continue() == 0, "native save");
    state_save_save_finish();
    renegade_interrupt();
    expected_irq = last_irq;
    /* Cold-instance defaults must not leak into restored machine state. */
    renegade_scrollx = 0; bank = 0; mapped_bank = NULL; rom_instance = 1;
#ifdef RETROM_MAME_PLUS
    from_main = from_mcu = main_sent = mcu_sent = 0;
    ddr_a = ddr_b = ddr_c = 0;
    port_a_in = port_b_in = port_c_in = 0;
    port_a_out = port_b_out = port_c_out = 0;
#else
    memset(mcu_buffer, 0, MCU_BUFFER_MAX);
    mcu_input_size = 0; mcu_output_byte = 0; mcu_key = -1;
#endif
    state_save_reset();
    state_save_set_current_tag(0);
    init_renegade();
    CHECK(video_start_renegade() == 0, "cold video start");
    CHECK(state_save_load_begin(state, size) == 0, "native load begin");
    CHECK(state_save_load_continue() == 0, "native load");
    state_save_load_finish();
    CHECK(renegade_scrollx == 0x218, "background scroll register survives cold restore");
    CHECK(bank == 1, "ROM bank selection survives cold restore");
    CHECK(mapped_bank == rom[1] + 0x10000, "ROM bank pointer rebuilt in the new instance");
#ifdef RETROM_MAME_PLUS
    CHECK(from_main == 0x51 && from_mcu == 0xa7 && main_sent == 1 && mcu_sent == 1, "MCU mailboxes and handshake survive");
    CHECK(ddr_a == 0x5a && ddr_b == 0xc3 && ddr_c == 0x96, "MCU port directions survive");
    CHECK(port_a_in == 0x12 && port_b_in == 0x34 && port_c_in == 0x56, "MCU input latches survive");
    CHECK(port_a_out == 0x65 && port_b_out == 0x43 && port_c_out == 0x21, "MCU output latches survive");
#else
    CHECK(memcmp(mcu_buffer, "ABCDEF", MCU_BUFFER_MAX) == 0, "MCU pending command survives");
    CHECK(mcu_input_size == 3 && mcu_output_byte == 2 && mcu_key == 9, "MCU protocol position survives");
#endif
    renegade_interrupt();
    CHECK(last_irq == expected_irq, "IRQ/NMI phase resumes at the saved boundary");
    free(state);
    state_save_reset();
    if (failures) return 1;
    puts("Renegade checkpoint: scroll, bank, MCU and interrupt phase restored");
    return 0;
}
