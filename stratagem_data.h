#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HID modifier: Left Ctrl in high byte for furi_hal_hid_kb_* (mods = keycode >> 8) */
#define KEY_MOD_LEFT_CTRL  (1u << 8)
/* HID Usage Table Keyboard/Keypad page: W=0x1A A=0x04 S=0x16 D=0x07 */
#define HID_KEY_W  0x1A
#define HID_KEY_A  0x04
#define HID_KEY_S  0x16
#define HID_KEY_D  0x07

/* Stratagem = Ctrl+WASD: W=up, A=left, S=down, D=right */
#define STRAT_UP    (KEY_MOD_LEFT_CTRL | HID_KEY_W)
#define STRAT_DOWN  (KEY_MOD_LEFT_CTRL | HID_KEY_S)
#define STRAT_LEFT  (KEY_MOD_LEFT_CTRL | HID_KEY_A)
#define STRAT_RIGHT (KEY_MOD_LEFT_CTRL | HID_KEY_D)

typedef enum {
    StratDirUp,
    StratDirDown,
    StratDirLeft,
    StratDirRight,
    StratDirCount
} StratagemDir;

static inline uint16_t strat_dir_to_key(StratagemDir d) {
    switch(d) {
    case StratDirUp:    return STRAT_UP;
    case StratDirDown:  return STRAT_DOWN;
    case StratDirLeft:  return STRAT_LEFT;
    case StratDirRight: return STRAT_RIGHT;
    default:            return 0;
    }
}

#define STRATAGEM_MAX_COUNT  96
#define STRATAGEM_MAX_SEQ    10
#define STRATAGEM_NAME_SIZE  32
#define STRATAGEM_CATEGORY_SIZE 20
#define KEYBIND_KEY_COUNT    6

typedef struct {
    const char* name;
    const char* category;
    const StratagemDir* sequence;
    uint8_t length;
} StratagemDef;

extern const StratagemDef* stratagem_list;
extern uint8_t stratagem_count;

extern int8_t stratagem_keybind[KEYBIND_KEY_COUNT];
void stratagem_load_from_file(const char* path);
void stratagem_save_keybinds(const char* path);
int stratagem_index_by_name(const char* name);
extern const char* stratagem_key_names[KEYBIND_KEY_COUNT];

#ifdef __cplusplus
}
#endif
