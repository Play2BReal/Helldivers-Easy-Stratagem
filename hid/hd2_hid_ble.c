#include "hd2_hid_ble.h"
#include <furi.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

#define BLE_DISCONNECT_DELAY_MS 300
#define BLE_ADVERTISE_DELAY_MS  150
#define BLE_DEINIT_DELAY_MS     200

static FuriHalBleProfileBase* g_ble_hid_profile = NULL;
static Bt* g_bt_service = NULL;
static bool g_ble_is_connected = false;

static void ble_connection_status_callback(BtStatus status, void* context) {
    UNUSED(context);
    g_ble_is_connected = (status == BtStatusConnected);
}

bool ble_hid_init(void) {
    if(g_ble_hid_profile && g_bt_service && g_ble_is_connected) return true;
    if(!g_bt_service) {
        g_bt_service = furi_record_open(RECORD_BT);
        if(!g_bt_service) return false;
    }
    if(!g_ble_is_connected) {
        bt_disconnect(g_bt_service);
        furi_delay_ms(BLE_DISCONNECT_DELAY_MS);
    }
    if(!g_ble_hid_profile) {
        g_ble_hid_profile = bt_profile_start(g_bt_service, ble_profile_hid, NULL);
        if(!g_ble_hid_profile) return false;
    }
    bt_set_status_changed_callback(g_bt_service, ble_connection_status_callback, NULL);
    if(!furi_hal_bt_is_active()) {
        furi_delay_ms(BLE_ADVERTISE_DELAY_MS);
        furi_hal_bt_start_advertising();
        furi_delay_ms(BLE_ADVERTISE_DELAY_MS);
    }
    return true;
}

void ble_hid_deinit(void) {
    if(g_bt_service) {
        bt_set_status_changed_callback(g_bt_service, NULL, NULL);
        bt_disconnect(g_bt_service);
        furi_delay_ms(BLE_DEINIT_DELAY_MS);
        if(g_ble_hid_profile) {
            bt_profile_restore_default(g_bt_service);
            g_ble_hid_profile = NULL;
        }
        furi_record_close(RECORD_BT);
        g_bt_service = NULL;
    }
    g_ble_is_connected = false;
}

bool ble_hid_is_ready(void) {
    return (g_ble_hid_profile != NULL && g_bt_service != NULL && g_ble_is_connected);
}

void ble_hid_release_all(void) {
    if(g_ble_hid_profile) ble_profile_hid_kb_release_all(g_ble_hid_profile);
}

void ble_hid_press_key(uint16_t keycode) {
    if(g_ble_hid_profile) ble_profile_hid_kb_press(g_ble_hid_profile, keycode);
}

void ble_hid_release_key(uint16_t keycode) {
    if(g_ble_hid_profile) ble_profile_hid_kb_release(g_ble_hid_profile, keycode);
}

void ble_hid_start_advertising(void) {
    if(g_ble_hid_profile && g_bt_service && g_ble_is_connected) return;
    if(g_ble_hid_profile && g_bt_service) {
        if(!furi_hal_bt_is_active()) furi_hal_bt_start_advertising();
        return;
    }
    ble_hid_init();
}

void ble_hid_stop_advertising(void) {
    ble_hid_deinit();
}
