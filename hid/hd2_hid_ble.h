#pragma once

#include <stdbool.h>
#include <stdint.h>

bool ble_hid_init(void);
void ble_hid_deinit(void);
bool ble_hid_is_ready(void);
void ble_hid_release_all(void);
void ble_hid_press_key(uint16_t keycode);
void ble_hid_release_key(uint16_t keycode);
void ble_hid_start_advertising(void);
void ble_hid_stop_advertising(void);
