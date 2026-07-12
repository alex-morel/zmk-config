/*
 * Tela de status em RETRATO para OLED SSD1306 (128x32) montada girada 90 graus.
 *
 * ZMK/LVGL nao rotaciona display mono por software (issue #1749). Solucao
 * (nice!view): desenha num CANVAS L8 e rotaciona o BUFFER com lv_draw_sw_rotate.
 * Precisa de: LV_USE_CANVAS, work queue dedicada, mem pool grande (Kconfig.defconfig).
 *
 * Central: mostra camada (topo) + bateria (base). Periferica: so bateria
 * (layer/keymap sao exclusivos da central).
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/battery.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Camada/keymap so existem na central (ou em teclado nao-split). */
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#define SHOW_LAYER 1
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#else
#define SHOW_LAYER 0
#endif

#define DW 128 /* largura do display */
#define DH 32  /* altura do display  */
#define CF LV_COLOR_FORMAT_L8
#define BPP LV_COLOR_FORMAT_GET_BPP(CF)

static uint8_t draw_buf[LV_CANVAS_BUF_SIZE(DH, DW, BPP, LV_DRAW_BUF_STRIDE_ALIGN)];
static uint8_t out_buf[LV_CANVAS_BUF_SIZE(DW, DH, BPP, LV_DRAW_BUF_STRIDE_ALIGN)];

static lv_obj_t *g_draw; /* canvas retrato (oculto), onde desenhamos */
static lv_obj_t *g_out;  /* canvas mostrado, recebe o buffer rotacionado */

static uint8_t g_battery = 0;
#if SHOW_LAYER
static uint8_t g_layer = 0;
#endif

static void draw_text(lv_coord_t y, lv_coord_t h, const char *txt) {
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = lv_color_black();
    dsc.font = &lv_font_montserrat_16;
    dsc.align = LV_TEXT_ALIGN_CENTER;
    dsc.text = txt;

    lv_layer_t layer;
    lv_canvas_init_layer(g_draw, &layer);
    lv_area_t coords = {0, y, DH, y + h};
    lv_draw_label(&layer, &dsc, &coords);
    lv_canvas_finish_layer(g_draw, &layer);
}

static void redraw(void) {
    if (g_draw == NULL) {
        return;
    }
    lv_canvas_fill_bg(g_draw, lv_color_white(), LV_OPA_COVER);

    char buf[16];
#if SHOW_LAYER
    snprintf(buf, sizeof(buf), "L%d", g_layer);
    draw_text(6, 24, buf);
#endif
    snprintf(buf, sizeof(buf), "%d%%", g_battery);
    draw_text(DW - 30, 24, buf);

    /* rotaciona draw_buf (32x128) -> out_buf (128x32), 90 graus */
    uint32_t ss = lv_draw_buf_width_to_stride(DH, CF);
    uint32_t ds = lv_draw_buf_width_to_stride(DW, CF);
    lv_draw_sw_rotate(draw_buf, out_buf, DH, DW, ss, ds, LV_DISPLAY_ROTATION_90, CF);

    if (g_out != NULL) {
        lv_obj_invalidate(g_out);
    }
}

#if SHOW_LAYER
/* ---------- listener de CAMADA (so central) ---------- */
struct layer_state {
    uint8_t index;
};
static struct layer_state layer_get_state(const zmk_event_t *eh) {
    return (struct layer_state){.index = zmk_keymap_highest_layer_active()};
}
static void layer_update_cb(struct layer_state s) {
    g_layer = s.index;
    redraw();
}
ZMK_DISPLAY_WIDGET_LISTENER(splitkb_layer, struct layer_state, layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(splitkb_layer, zmk_layer_state_changed);
#endif /* SHOW_LAYER */

/* ---------- listener de BATERIA (central + periferica) ---------- */
struct batt_state {
    uint8_t level;
};
static struct batt_state batt_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct batt_state){.level =
                                   (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge()};
}
static void batt_update_cb(struct batt_state s) {
    g_battery = s.level;
    redraw();
}
ZMK_DISPLAY_WIDGET_LISTENER(splitkb_batt, struct batt_state, batt_update_cb, batt_get_state)
ZMK_SUBSCRIPTION(splitkb_batt, zmk_battery_state_changed);

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    g_out = lv_canvas_create(screen);
    lv_canvas_set_buffer(g_out, out_buf, DW, DH, CF);
    lv_obj_align(g_out, LV_ALIGN_TOP_LEFT, 0, 0);

    g_draw = lv_canvas_create(screen);
    lv_canvas_set_buffer(g_draw, draw_buf, DH, DW, CF);
    lv_obj_add_flag(g_draw, LV_OBJ_FLAG_HIDDEN);

    redraw();

#if SHOW_LAYER
    splitkb_layer_init();
#endif
    splitkb_batt_init();

    return screen;
}
