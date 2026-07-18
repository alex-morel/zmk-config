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

#include <stdbool.h>
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

/* fonte custom (bigzero.c) com o glifo Ø (zero cortado) grande */
LV_FONT_DECLARE(bigzero);

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

static void draw_text(lv_coord_t y, lv_coord_t h, const char *txt, const lv_font_t *font) {
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = lv_color_black();
    dsc.font = font;
    dsc.align = LV_TEXT_ALIGN_CENTER;
    dsc.text = txt;

    lv_layer_t layer;
    lv_canvas_init_layer(g_draw, &layer);
    lv_area_t coords = {0, y, DH, y + h};
    lv_draw_label(&layer, &dsc, &coords);
    lv_canvas_finish_layer(g_draw, &layer);
}

/* barra diagonal (pra "cortar" o zero, estilo Big-O) */
static void draw_line(lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_black();
    dsc.width = 3;
    dsc.p1.x = x1;
    dsc.p1.y = y1;
    dsc.p2.x = x2;
    dsc.p2.y = y2;

    lv_layer_t layer;
    lv_canvas_init_layer(g_draw, &layer);
    lv_draw_line(&layer, &dsc);
    lv_canvas_finish_layer(g_draw, &layer);
}

/* ---------- animacao de boot: "ACTION!" em pe ---------- */
/* A tela e retrato (32 de largura x 128 de altura), entao as 7 letras ficam
 * EMPILHADAS, uma por linha: A / C / T / I / O / N / !
 * Elas aparecem uma a uma (de cima pra baixo), seguram um instante e a tela
 * normal entra. */
#define BOOT_STEP_MS 200
#define ACTION_N 7
#define BOOT_HOLD_STEPS 4          /* ~800ms segurando o "ACTION!" completo */
#define ACTION_Y0 1                /* y da 1a letra */
#define ACTION_DY 18               /* espacamento (7 * 18 = 126, cabe em 128) */

static const char *const ACTION_LETTERS[ACTION_N] = {"A", "C", "T", "I", "O", "N", "!"};
static int g_boot_step;
static bool g_boot_done;

static void draw_boot(void) {
    int shown = (g_boot_step < ACTION_N) ? g_boot_step : ACTION_N;
    for (int i = 0; i < shown; i++) {
        draw_text(ACTION_Y0 + i * ACTION_DY, ACTION_DY, ACTION_LETTERS[i],
                  &lv_font_montserrat_16);
    }
}

static void draw_normal(void) {
    char buf[16];

    /* --- header "the big 0" (empilhado, tamanhos diferentes) --- */
    draw_text(0, 12, "the", &lv_font_montserrat_8);   /* menor fonte */
    draw_text(10, 22, "big", &lv_font_montserrat_16); /* 16 cabe as 3 letras em 32px */
    draw_text(28, 44, "\xC3\x98", &bigzero);          /* Ø = zero cortado (fonte custom) */

    /* --- widgets (embaixo do header) --- */
#if SHOW_LAYER
    snprintf(buf, sizeof(buf), "L%d", g_layer);
    draw_text(82, 20, buf, &lv_font_montserrat_16);
#endif
    /* porcentagem em fonte menor (8) pra nao competir com o header */
    snprintf(buf, sizeof(buf), "%d%%", g_battery);
    draw_text(110, 14, buf, &lv_font_montserrat_8);
}

static void redraw(void) {
    if (g_draw == NULL) {
        return;
    }
    lv_canvas_fill_bg(g_draw, lv_color_white(), LV_OPA_COVER);

    /* durante o boot mostra "ACTION!"; depois, a tela normal. (Eventos de
     * bateria/camada chamam redraw() e caem aqui tambem -- por isso o teste.) */
    if (!g_boot_done) {
        draw_boot();
    } else {
        draw_normal();
    }

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

/* avanca a animacao de boot; no fim entrega a tela normal */
static void boot_timer_cb(lv_timer_t *t) {
    g_boot_step++;
    if (g_boot_step >= ACTION_N + BOOT_HOLD_STEPS) {
        g_boot_done = true;
        lv_timer_delete(t);
    }
    redraw();
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    /* remove a margem/padding padrao do LVGL pra usar a tela inteira */
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);

    g_out = lv_canvas_create(screen);
    lv_canvas_set_buffer(g_out, out_buf, DW, DH, CF);
    lv_obj_set_style_pad_all(g_out, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_out, 0, LV_PART_MAIN);
    lv_obj_align(g_out, LV_ALIGN_TOP_LEFT, 0, 0);

    g_draw = lv_canvas_create(screen);
    lv_canvas_set_buffer(g_draw, draw_buf, DH, DW, CF);
    lv_obj_add_flag(g_draw, LV_OBJ_FLAG_HIDDEN);

    /* comeca na animacao de boot ("ACTION!"), o timer conduz ate a tela normal */
    g_boot_step = 0;
    g_boot_done = false;
    redraw();
    lv_timer_create(boot_timer_cb, BOOT_STEP_MS, NULL);

#if SHOW_LAYER
    splitkb_layer_init();
#endif
    splitkb_batt_init();

    return screen;
}
