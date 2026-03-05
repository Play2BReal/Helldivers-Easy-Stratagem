#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

#include "stratagem_data.h"
#include "hid/hd2_hid.h"

#define KEY_PRESS_MS   30
#define BETWEEN_KEY_MS 20
#define MAX_LIST_ITEMS 128
#define MAX_CATEGORIES 16

typedef enum {
    ListItemHeader,
    ListItemStratagem,
    ListItemConfigureKeybinds,
    ListItemEnterScreen,
} ListItemType;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    VariableItemList* list;
    View* stratagem_screen_view;
    View* keybind_config_view;
    View* ble_wait_view;
    VariableItemList* keybind_picker_list;
    NotificationApp* notifications;
    HidMode hid_mode;
    uint16_t list_item_count;
    uint8_t list_item_type[MAX_LIST_ITEMS];
    int8_t list_item_stratagem_index[MAX_LIST_ITEMS];
    int8_t keybind_picker_key;
    uint8_t keybind_config_selected;
} HelldiversStratagemApp;

typedef enum {
    ViewSubmenu,
    ViewList,
    ViewBleWait,
    ViewStratagemScreen,
    ViewKeybindConfig,
    ViewKeybindPicker,
} ViewId;

static void send_stratagem(HelldiversStratagemApp* app, const StratagemDef* def) {
    hd2_hid_press_key(app->hid_mode, KEY_MOD_LEFT_CTRL);
    furi_delay_ms(KEY_PRESS_MS);
    for(uint8_t i = 0; i < def->length; i++) {
        uint16_t key = strat_dir_to_key(def->sequence[i]);
        if(!key) continue;
        uint8_t key_only = (uint8_t)(key & 0xFF);
        hd2_hid_press_key(app->hid_mode, key_only);
        furi_delay_ms(KEY_PRESS_MS);
        hd2_hid_release_key(app->hid_mode, key_only);
        if(i + 1 < def->length) furi_delay_ms(BETWEEN_KEY_MS);
    }
    hd2_hid_release_all(app->hid_mode);
}

static void list_enter_callback(void* ctx, uint32_t index) {
    HelldiversStratagemApp* app = ctx;
    if(index >= app->list_item_count) return;
    switch((ListItemType)app->list_item_type[index]) {
    case ListItemHeader:
        break;
    case ListItemStratagem: {
        int si = app->list_item_stratagem_index[index];
        if(si < 0 || (uint8_t)si >= stratagem_count) break;
        if(!hd2_hid_is_ready(app->hid_mode)) {
            notification_message(app->notifications, &sequence_single_vibro);
            return;
        }
        send_stratagem(app, &stratagem_list[(uint8_t)si]);
        notification_message(app->notifications, &sequence_success);
        break;
    }
    case ListItemConfigureKeybinds:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewKeybindConfig);
        break;
    case ListItemEnterScreen:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewStratagemScreen);
        break;
    }
}

static HelldiversStratagemApp* g_app;

static uint32_t list_previous_callback(void* ctx) {
    UNUSED(ctx);
    if(g_app) hd2_hid_deinit(g_app->hid_mode);
    return ViewSubmenu;
}

static void stratagem_screen_draw_callback(Canvas* canvas, void* _model) {
    HelldiversStratagemApp* app = *(HelldiversStratagemApp**)_model;
    if(!app) return;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Stratagem keys");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "Tap = send  Hold Back = exit");
}

static bool stratagem_screen_input_callback(InputEvent* event, void* _app) {
    HelldiversStratagemApp* app = _app;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewList);
        return true;
    }
    if(event->type == InputTypeShort) {
        int key_index = (int)event->key;
        if(key_index >= 0 && key_index < KEYBIND_KEY_COUNT && stratagem_keybind[key_index] >= 0) {
            if(!hd2_hid_is_ready(app->hid_mode)) {
                notification_message(app->notifications, &sequence_single_vibro);
                return true;
            }
            uint8_t idx = (uint8_t)stratagem_keybind[key_index];
            if(idx < stratagem_count) {
                send_stratagem(app, &stratagem_list[idx]);
                notification_message(app->notifications, &sequence_success);
            }
            return true;
        }
        if(event->key == InputKeyBack) return true;
    }
    return false;
}

static uint32_t stratagem_screen_previous_callback(void* ctx) {
    UNUSED(ctx);
    return ViewList;
}

#define KEYBIND_ROW_H  10
#define KEYBIND_TOP    14
#define KEYBIND_LEFT   2
#define KEYBIND_WIDTH  124
#define KEYBIND_VISIBLE_ROWS 4

static void keybind_config_draw_callback(Canvas* canvas, void* _model) {
    HelldiversStratagemApp* app = *(HelldiversStratagemApp**)_model;
    if(!app) return;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Up/Down=move  OK=change");
    int selected = app->keybind_config_selected;
    int first = (selected >= KEYBIND_VISIBLE_ROWS) ? (selected - KEYBIND_VISIBLE_ROWS + 1) : 0;
    for(int vi = 0; vi < KEYBIND_VISIBLE_ROWS && first + vi < KEYBIND_KEY_COUNT; vi++) {
        int i = first + vi;
        int y = KEYBIND_TOP + vi * KEYBIND_ROW_H;
        bool sel = (i == selected);
        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, KEYBIND_LEFT, y - 1, KEYBIND_WIDTH, KEYBIND_ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, KEYBIND_LEFT + 2, y + KEYBIND_ROW_H - 3, stratagem_key_names[i]);
        const char* bound = "None";
        if(stratagem_keybind[i] >= 0 && (uint8_t)stratagem_keybind[i] < stratagem_count)
            bound = stratagem_list[(uint8_t)stratagem_keybind[i]].name;
        canvas_draw_str_aligned(canvas, KEYBIND_LEFT + KEYBIND_WIDTH - 4, y + KEYBIND_ROW_H - 3, AlignRight, AlignBottom, bound);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back=Exit");
}

static bool keybind_config_input_callback(InputEvent* event, void* _app) {
    HelldiversStratagemApp* app = _app;
    bool need_redraw = false;
    if(event->key == InputKeyBack && (event->type == InputTypeShort || event->type == InputTypeLong)) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewList);
        return true;
    }
    if(event->key == InputKeyUp && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->keybind_config_selected = app->keybind_config_selected == 0 ?
            KEYBIND_KEY_COUNT - 1 : app->keybind_config_selected - 1;
        need_redraw = true;
    }
    if(event->key == InputKeyDown && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->keybind_config_selected = (app->keybind_config_selected + 1) % KEYBIND_KEY_COUNT;
        need_redraw = true;
    }
    if(need_redraw) {
        view_get_model(app->keybind_config_view);
        view_commit_model(app->keybind_config_view, true);
        return true;
    }
    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        app->keybind_picker_key = app->keybind_config_selected;
        static char picker_header[32];
        snprintf(picker_header, sizeof(picker_header), "Bind %s to:", stratagem_key_names[app->keybind_config_selected]);
        variable_item_list_set_header(app->keybind_picker_list, picker_header);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewKeybindPicker);
        return true;
    }
    return false;
}

static uint32_t keybind_config_previous_callback(void* ctx) {
    UNUSED(ctx);
    return ViewList;
}

static void keybind_picker_enter_callback(void* ctx, uint32_t index) {
    HelldiversStratagemApp* app = ctx;
    int k = app->keybind_picker_key;
    if(k < 0 || k >= KEYBIND_KEY_COUNT) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewKeybindConfig);
        return;
    }
    if(index >= stratagem_count) {
        stratagem_keybind[k] = -1;
    } else {
        stratagem_keybind[k] = (int8_t)index;
    }
    stratagem_save_keybinds(NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewKeybindConfig);
}

static uint32_t keybind_picker_previous_callback(void* ctx) {
    UNUSED(ctx);
    return ViewKeybindConfig;
}

static void ble_wait_draw_callback(Canvas* canvas, void* _model) {
    UNUSED(_model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Bluetooth");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Pair \"Control\" on device");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "OK when done  Back = cancel");
}

static bool ble_wait_input_callback(InputEvent* event, void* ctx) {
    HelldiversStratagemApp* app = ctx;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyBack) {
        hd2_hid_stop_ble_advertising();
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        return true;
    }
    if(event->key == InputKeyOk) {
        if(hd2_hid_is_ready(app->hid_mode)) {
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewList);
        } else {
            notification_message(app->notifications, &sequence_single_vibro);
        }
        return true;
    }
    return false;
}

static uint32_t ble_wait_previous_callback(void* ctx) {
    UNUSED(ctx);
    hd2_hid_stop_ble_advertising();
    return ViewSubmenu;
}

static void submenu_callback(void* ctx, uint32_t index) {
    HelldiversStratagemApp* app = ctx;
    app->hid_mode = (index == 0) ? HidModeUsb : HidModeBle;
    if(app->hid_mode == HidModeBle) {
        if(!hd2_hid_init(HidModeBle)) {
            notification_message(app->notifications, &sequence_error);
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewBleWait);
    } else {
        if(!hd2_hid_init(HidModeUsb)) {
            notification_message(app->notifications, &sequence_error);
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewList);
    }
}

static ViewDispatcher* g_view_dispatcher;

static uint32_t submenu_exit_callback(void* ctx) {
    UNUSED(ctx);
    if(g_view_dispatcher) view_dispatcher_stop(g_view_dispatcher);
    return VIEW_IGNORE;
}

static void build_list_with_categories(HelldiversStratagemApp* app) {
    const char* categories[MAX_CATEGORIES];
    uint8_t ncat = 0;
    for(uint8_t i = 0; i < stratagem_count && ncat < MAX_CATEGORIES; i++) {
        const char* c = stratagem_list[i].category;
        uint8_t j;
        for(j = 0; j < ncat; j++)
            if(c && categories[j] && strcmp(categories[j], c) == 0) break;
        if(j == ncat) categories[ncat++] = c;
    }
    uint16_t idx = 0;
    char cat_label[STRATAGEM_CATEGORY_SIZE + 4];
    for(uint8_t c = 0; c < ncat && idx < MAX_LIST_ITEMS; c++) {
        app->list_item_type[idx] = ListItemHeader;
        app->list_item_stratagem_index[idx] = -1;
        snprintf(cat_label, sizeof(cat_label), "[%s]", categories[c] ? categories[c] : "General");
        variable_item_list_add(app->list, cat_label, 1, NULL, NULL);
        idx++;
        for(uint8_t i = 0; i < stratagem_count && idx < MAX_LIST_ITEMS; i++) {
            const char* cat = stratagem_list[i].category;
            if(!cat || !categories[c] || strcmp(cat, categories[c]) != 0) continue;
            app->list_item_type[idx] = ListItemStratagem;
            app->list_item_stratagem_index[idx] = (int8_t)i;
            variable_item_list_add(app->list, stratagem_list[i].name, 1, NULL, NULL);
            idx++;
        }
    }
    app->list_item_type[idx] = ListItemConfigureKeybinds;
    app->list_item_stratagem_index[idx] = -1;
    variable_item_list_add(app->list, "Configure keybinds", 1, NULL, NULL);
    idx++;
    app->list_item_type[idx] = ListItemEnterScreen;
    app->list_item_stratagem_index[idx] = -1;
    variable_item_list_add(app->list, "Enter Stratagem screen", 1, NULL, NULL);
    idx++;
    app->list_item_count = idx;
}

int32_t helldivers_stratagem_app(void* p) {
    UNUSED(p);
    stratagem_load_from_file(NULL);

    HelldiversStratagemApp* app = malloc(sizeof(HelldiversStratagemApp));
    memset(app, 0, sizeof(HelldiversStratagemApp));
    app->keybind_picker_key = -1;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "USB", 0, submenu_callback, app);
    submenu_add_item(app->submenu, "Bluetooth", 1, submenu_callback, app);
    submenu_set_header(app->submenu, "Connect");

    app->list = variable_item_list_alloc();
    variable_item_list_set_enter_callback(app->list, list_enter_callback, app);
    variable_item_list_set_header(app->list, "OK send  Back exit");
    build_list_with_categories(app);

    View* list_view = variable_item_list_get_view(app->list);
    view_set_previous_callback(list_view, list_previous_callback);

    app->stratagem_screen_view = view_alloc();
    view_set_context(app->stratagem_screen_view, app);
    view_set_draw_callback(app->stratagem_screen_view, stratagem_screen_draw_callback);
    view_set_input_callback(app->stratagem_screen_view, stratagem_screen_input_callback);
    view_set_previous_callback(app->stratagem_screen_view, stratagem_screen_previous_callback);
    view_allocate_model(
        app->stratagem_screen_view,
        ViewModelTypeLocking,
        sizeof(HelldiversStratagemApp*));
    with_view_model(
        app->stratagem_screen_view,
        HelldiversStratagemApp** app_pp,
        { *app_pp = app; },
        false);

    app->keybind_config_view = view_alloc();
    view_set_context(app->keybind_config_view, app);
    view_set_draw_callback(app->keybind_config_view, keybind_config_draw_callback);
    view_set_input_callback(app->keybind_config_view, keybind_config_input_callback);
    view_set_previous_callback(app->keybind_config_view, keybind_config_previous_callback);
    view_allocate_model(
        app->keybind_config_view,
        ViewModelTypeLocking,
        sizeof(HelldiversStratagemApp*));
    with_view_model(
        app->keybind_config_view,
        HelldiversStratagemApp** app_pp,
        { *app_pp = app; },
        false);

    app->ble_wait_view = view_alloc();
    view_set_context(app->ble_wait_view, app);
    view_set_draw_callback(app->ble_wait_view, ble_wait_draw_callback);
    view_set_input_callback(app->ble_wait_view, ble_wait_input_callback);
    view_set_previous_callback(app->ble_wait_view, ble_wait_previous_callback);

    app->keybind_picker_list = variable_item_list_alloc();
    variable_item_list_set_enter_callback(app->keybind_picker_list, keybind_picker_enter_callback, app);
    variable_item_list_set_header(app->keybind_picker_list, "Select stratagem");
    for(uint8_t i = 0; i < stratagem_count; i++)
        variable_item_list_add(app->keybind_picker_list, stratagem_list[i].name, 1, NULL, NULL);
    variable_item_list_add(app->keybind_picker_list, "Unbind", 1, NULL, NULL);
    View* keybind_picker_view = variable_item_list_get_view(app->keybind_picker_list);
    view_set_previous_callback(keybind_picker_view, keybind_picker_previous_callback);

    View* submenu_view = submenu_get_view(app->submenu);
    view_set_previous_callback(submenu_view, submenu_exit_callback);
    g_view_dispatcher = app->view_dispatcher;

    view_dispatcher_add_view(app->view_dispatcher, ViewSubmenu, submenu_view);
    view_dispatcher_add_view(app->view_dispatcher, ViewList, list_view);
    view_dispatcher_add_view(app->view_dispatcher, ViewBleWait, app->ble_wait_view);
    view_dispatcher_add_view(app->view_dispatcher, ViewStratagemScreen, app->stratagem_screen_view);
    view_dispatcher_add_view(app->view_dispatcher, ViewKeybindConfig, app->keybind_config_view);
    view_dispatcher_add_view(app->view_dispatcher, ViewKeybindPicker, keybind_picker_view);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
    g_app = app;
    view_dispatcher_run(app->view_dispatcher);
    g_app = NULL;
    g_view_dispatcher = NULL;

    if(app->hid_mode == HidModeUsb) {
        hd2_hid_deinit(HidModeUsb);
    } else if(app->hid_mode == HidModeBle) {
        hd2_hid_stop_ble_advertising();
    }

    view_dispatcher_remove_view(app->view_dispatcher, ViewKeybindPicker);
    view_dispatcher_remove_view(app->view_dispatcher, ViewKeybindConfig);
    view_dispatcher_remove_view(app->view_dispatcher, ViewStratagemScreen);
    view_dispatcher_remove_view(app->view_dispatcher, ViewBleWait);
    view_dispatcher_remove_view(app->view_dispatcher, ViewList);
    view_dispatcher_remove_view(app->view_dispatcher, ViewSubmenu);
    variable_item_list_free(app->keybind_picker_list);
    view_free(app->keybind_config_view);
    view_free(app->ble_wait_view);
    view_free(app->stratagem_screen_view);
    variable_item_list_free(app->list);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
