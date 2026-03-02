#include "hd2_hid_usb.h"
#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#define HID_INIT_DELAY_MS       25
#define HID_SETTLE_DELAY_MS     100
#define HID_CONNECT_TIMEOUT_MS 5000
#define HID_CONNECT_RETRY_MS    100

static FuriHalUsbInterface* g_usb_previous_config = NULL;

bool usb_hid_init(void) {
    g_usb_previous_config = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&usb_hid, NULL)) return false;
    uint8_t retries = HID_CONNECT_TIMEOUT_MS / HID_CONNECT_RETRY_MS;
    for(uint8_t i = 0; i < retries; i++) {
        if(furi_hal_hid_is_connected()) return true;
        furi_delay_ms(HID_CONNECT_RETRY_MS);
    }
    return false;
}

void usb_hid_deinit(void) {
    furi_hal_hid_kb_release_all();
    furi_delay_ms(HID_INIT_DELAY_MS);
    if(g_usb_previous_config) {
        furi_hal_usb_set_config(g_usb_previous_config, NULL);
        g_usb_previous_config = NULL;
    } else {
        furi_hal_usb_unlock();
    }
    furi_delay_ms(HID_SETTLE_DELAY_MS);
}

bool usb_hid_is_ready(void) {
    return furi_hal_hid_is_connected();
}

void usb_hid_release_all(void) {
    furi_hal_hid_kb_release_all();
}

void usb_hid_press_key(uint16_t keycode) {
    furi_hal_hid_kb_press(keycode);
}

void usb_hid_release_key(uint16_t keycode) {
    furi_hal_hid_kb_release(keycode);
}
