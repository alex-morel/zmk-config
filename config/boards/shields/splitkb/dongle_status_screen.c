/*
 * Tela de status do dongle (GC9A01 240x240 color) + ANIMACAO DE BOOT.
 *
 * Boot:
 *   1) o texto "CAST IN THE NAME OF GOD   YE NOT GUILTY" desliza da direita
 *      pra esquerda, ocupando a tela (fonte grande);
 *   2) "YE NOT GUILTY" pisca 4 vezes no centro;
 *   3) o overlay preto some (fade) e revela a tela de status (layer + conexao).
 *
 * A tela de status usa CORES EXPLICITAS (fundo preto, texto branco): no color,
 * sem tema, o texto padrao sai preto sobre preto e some (bug que investigamos).
 * O overlay nao e deletado no fim: fica em opacidade 0 (a tela nao tem toque).
 */

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>

static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_output_status output_status_widget;

/* --- tempos da animacao (ms) --- */
#define SCROLL_MS 4000        /* deslize do texto */
#define BLINK_STATES 8        /* 8 estados = 4 piscadas (on/off x4) */
#define BLINK_MS 1600         /* 200ms por estado */
#define FADE_MS 500           /* fade-out do overlay */

/* exec_cb da anim e (void*, int32_t); os setters de estilo querem selector. */
static void anim_set_opa(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, LV_PART_MAIN);
}
static void anim_set_x(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t *)var, v);
}
/* pisca: estado par = visivel, impar = invisivel */
static void anim_blink(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (v % 2 == 0) ? LV_OPA_COVER : LV_OPA_TRANSP,
                         LV_PART_MAIN);
}

static void boot_animation(lv_obj_t *screen) {
    /* overlay preto cobrindo os 240x240, por cima da tela de status */
    lv_obj_t *overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 240, 240);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    /* o texto que desliza e MUITO mais largo que a tela; sem isto o LVGL
     * torna o overlay rolavel e desenha barras de rolagem (as listras). */
    lv_obj_set_scrollbar_mode(overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_anim_t a;

    /* 1) TEXTO DESLIZANDO da direita pra esquerda */
    lv_obj_t *scroll = lv_label_create(overlay);
    lv_label_set_text(scroll, "CAST IN THE NAME OF GOD   YE NOT GUILTY");
    lv_obj_set_style_text_color(scroll, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(scroll, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_update_layout(scroll);                    /* pra get_width/height valerem */
    int32_t tw = lv_obj_get_width(scroll);
    lv_obj_set_y(scroll, (240 - lv_obj_get_height(scroll)) / 2);   /* centralizado vertical */

    lv_anim_init(&a);
    lv_anim_set_var(&a, scroll);
    lv_anim_set_values(&a, 240, -tw);                /* entra pela direita, sai pela esquerda */
    lv_anim_set_time(&a, SCROLL_MS);
    lv_anim_set_exec_cb(&a, anim_set_x);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);    /* velocidade constante (marquee) */
    lv_anim_start(&a);

    /* 2) "YE NOT GUILTY" pisca 4x no centro (comeca escondido) */
    lv_obj_t *blink = lv_label_create(overlay);
    lv_label_set_text(blink, "YE NOT GUILTY");
    lv_obj_set_style_text_color(blink, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(blink, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(blink);
    lv_obj_set_style_opa(blink, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_anim_init(&a);
    lv_anim_set_var(&a, blink);
    lv_anim_set_values(&a, 0, BLINK_STATES);
    lv_anim_set_time(&a, BLINK_MS);
    lv_anim_set_delay(&a, SCROLL_MS);                /* comeca quando o scroll termina */
    lv_anim_set_exec_cb(&a, anim_blink);
    lv_anim_start(&a);

    /* 3) overlay some e revela a tela de status */
    lv_anim_init(&a);
    lv_anim_set_var(&a, overlay);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, FADE_MS);
    lv_anim_set_delay(&a, SCROLL_MS + BLINK_MS);     /* depois do scroll + piscadas */
    lv_anim_set_exec_cb(&a, anim_set_opa);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* fundo preto + texto branco (herdado pelos filhos) */
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(screen, lv_color_white(), LV_PART_MAIN);
    /* sem rolagem/barras na tela toda */
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* conexao (USB/BLE) no topo */
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_set_style_text_color(zmk_widget_output_status_obj(&output_status_widget),
                                lv_color_white(), LV_PART_MAIN);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_MID, 0, 40);

    /* camada atual no centro, fonte grande */
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_color(zmk_widget_layer_status_obj(&layer_status_widget),
                                lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(zmk_widget_layer_status_obj(&layer_status_widget),
                               &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_CENTER, 0, 0);

    /* overlay animado por cima — toca no boot e some */
    boot_animation(screen);

    return screen;
}
