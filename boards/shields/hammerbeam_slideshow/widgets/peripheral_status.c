/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>

#include "peripheral_status.h"

/* All 30 artworks are defined in widgets/art.c. The HAMMERBEAM_IMGS macro
 * below lists every image; CONFIG_HAMMERBEAM_SLIDESHOW_MASK (bit N-1 = image
 * N) decides at build time which entries land in the real animation array,
 * so excluded artwork is garbage collected from the firmware. */
#define HAMMERBEAM_IMGS(XX)                                                                        \
    XX(1)  XX(2)  XX(3)  XX(4)  XX(5)  XX(6)                                                       \
    XX(7)  XX(8)  XX(9)  XX(10) XX(11) XX(12)                                                      \
    XX(13) XX(14) XX(15) XX(16) XX(17) XX(18)                                                      \
    XX(19) XX(20) XX(21) XX(22) XX(23) XX(24)                                                      \
    XX(25) XX(26) XX(27) XX(28) XX(29) XX(30)

#define HAMMERBEAM_ENTRY(n)                                                                       \
    (CONFIG_HAMMERBEAM_SLIDESHOW_MASK & (1U << ((n) -1))) ? &hammerbeam##n : NULL,

/* Bit N-1 of CONFIG_HAMMERBEAM_SLIDESHOW_MASK selects artwork N (1 = include) */
static const lv_img_dsc_t *anim_imgs_all[] = { HAMMERBEAM_IMGS(HAMMERBEAM_ENTRY) };

static const lv_img_dsc_t *anim_imgs[30];
static size_t anim_imgs_count = 0;

static void anim_imgs_build(void) {
    anim_imgs_count = 0;
    for (int i = 0; i < ARRAY_SIZE(anim_imgs_all); i++) {
        if (anim_imgs_all[i]) {
            anim_imgs[anim_imgs_count++] = anim_imgs_all[i];
        }
    }
    if (anim_imgs_count == 0) {
        /* Mask excluded everything: fall back to the first image. */
        anim_imgs[0] = anim_imgs_all[0];
        anim_imgs_count = 1;
    }
}

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct peripheral_status_state {
    bool connected;
};

static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Draw battery
    draw_battery(canvas, state);

    // Draw output status
    lv_canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc,
                        state->connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    widget->state.battery = state.level;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    return (struct battery_status_state) {
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    /* On the central, the split peripheral status event never fires, so derive
     * the connection state from the central's own USB/BLE connection. */
    return (struct peripheral_status_state){
        .connected = zmk_usb_is_powered() || zmk_ble_active_profile_is_connected()};
#else
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
#endif
}

static void set_connection_status(struct zmk_widget_status *widget,
                                  struct peripheral_status_state state) {
    widget->state.connected = state.connected;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    anim_imgs_build();

    lv_obj_t * art = lv_animimg_create(widget->obj);
    lv_obj_center(art);
    lv_animimg_set_src(art, (const void **) anim_imgs, anim_imgs_count);
    lv_animimg_set_duration(art, CONFIG_CUSTOM_ANIMATION_SPEED);
    lv_animimg_set_repeat_count(art, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(art);
    
    lv_obj_align(art, LV_ALIGN_TOP_LEFT, 0, 0);
    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_peripheral_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }