#include "hd2_hid.h"
#include "hd2_hid_ble.h"
#include "hd2_hid_usb.h"

bool hd2_hid_init(HidMode mode) {
    if(mode == HidModeBle) return ble_hid_init();
    return usb_hid_init();
}

void hd2_hid_deinit(HidMode mode) {
    if(mode == HidModeBle) {
        ble_hid_release_all();
        ble_hid_deinit();
    } else {
        usb_hid_deinit();
    }
}

bool hd2_hid_is_ready(HidMode mode) {
    if(mode == HidModeBle) return ble_hid_is_ready();
    return usb_hid_is_ready();
}

void hd2_hid_release_all(HidMode mode) {
    if(mode == HidModeBle) ble_hid_release_all();
    else usb_hid_release_all();
}

void hd2_hid_press_key(HidMode mode, uint16_t keycode) {
    if(mode == HidModeBle) ble_hid_press_key(keycode);
    else usb_hid_press_key(keycode);
}

void hd2_hid_release_key(HidMode mode, uint16_t keycode) {
    if(mode == HidModeBle) ble_hid_release_key(keycode);
    else usb_hid_release_key(keycode);
}

void hd2_hid_start_ble_advertising(void) {
    ble_hid_start_advertising();
}

void hd2_hid_stop_ble_advertising(void) {
    ble_hid_stop_advertising();
}
