#pragma once

#include <stdbool.h>
#include <stdint.h>

bool usb_hid_init(void);
void usb_hid_deinit(void);
bool usb_hid_is_ready(void);
void usb_hid_release_all(void);
void usb_hid_press_key(uint16_t keycode);
void usb_hid_release_key(uint16_t keycode);
