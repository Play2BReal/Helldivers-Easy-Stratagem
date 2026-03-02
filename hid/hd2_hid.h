#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum { HidModeUsb = 0, HidModeBle = 1 } HidMode;

bool hd2_hid_init(HidMode mode);
void hd2_hid_deinit(HidMode mode);
bool hd2_hid_is_ready(HidMode mode);
void hd2_hid_release_all(HidMode mode);
void hd2_hid_press_key(HidMode mode, uint16_t keycode);
void hd2_hid_release_key(HidMode mode, uint16_t keycode);

void hd2_hid_start_ble_advertising(void);
void hd2_hid_stop_ble_advertising(void);
