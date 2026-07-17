/*
 * Tela de status do dongle (GC9A01 240x240 color) + ANIMACAO DE BOOT.
 *
 * Boot:
 *   1) o texto "CAST IN THE NAME OF GOD   YE NOT GUILTY" desliza da direita
 *      pra esquerda (fonte grande, ocupa a tela);
 *   2) "YE NOT GUILTY" pisca 4 vezes no centro (fonte menor, cabe na tela);
 *   3) o overlay preto some (fade) e revela a tela de status (layer + conexao).
 *
 * A sequencia pos-scroll (piscar + revelar) e feita com um lv_timer (maquina de
 * estados) em vez de animacoes com 'delay' -- os delays nao dispararam de forma
 * confiavel aqui. O scroll e o fade sao lv_anim (esses funcionam).
 *
 * Cores EXPLICITAS (fundo preto, texto branco): no color, sem tema, o texto
 * padrao sai preto sobre preto e some. Rolagem/scrollbar DESLIGADA em tudo: o
 * texto largo tornava os objetos rolaveis e o LVGL desenhava barras (as listras).
 */

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>

static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_output_status output_status_widget;

#define SCROLL_MS 4000        /* deslize do texto */
#define BLINK_STEPS 8         /* 8 passos = 4 piscadas (on/off x4) */
#define BLINK_PERIOD 200      /* ms por passo do pisca */
#define FADE_MS 500           /* fade-out do overlay */

/* refs pro timer/anim chegarem nos objetos */
static lv_obj_t *g_overlay;
static lv_obj_t *g_scroll;
static lv_obj_t *g_blink;
static int g_phase;

/* desliga rolagem+barras (senao conteudo largo vira scrollbar = listras) */
static void no_scroll(lv_obj_t *o) {
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static void anim_set_opa(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, LV_PART_MAIN);
}
static void anim_set_x(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t *)var, v);
}

/* passo do pisca + revelacao */
static void seq_timer_cb(lv_timer_t *t) {
    if (g_phase < BLINK_STEPS) {
        lv_obj_set_style_opa(g_blink, (g_phase % 2 == 0) ? LV_OPA_COVER : LV_OPA_TRANSP,
                             LV_PART_MAIN);
        g_phase++;
        return;
    }
    /* acabou de piscar: some com o overlay (fade) e revela os widgets */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_overlay);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, FADE_MS);
    lv_anim_set_exec_cb(&a, anim_set_opa);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
    lv_timer_delete(t);
}

/* scroll terminou: esconde o texto e comeca a sequencia piscar->revelar */
static void on_scroll_done(lv_anim_t *a) {
    lv_obj_add_flag(g_scroll, LV_OBJ_FLAG_HIDDEN);
    g_phase = 0;
    lv_obj_set_style_opa(g_blink, LV_OPA_COVER, LV_PART_MAIN);   /* aparece pra piscar */
    lv_timer_create(seq_timer_cb, BLINK_PERIOD, NULL);
}

static void boot_animation(lv_obj_t *screen) {
    /* overlay preto cobrindo os 240x240, por cima da tela de status */
    g_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(g_overlay);
    lv_obj_set_size(g_overlay, 240, 240);
    lv_obj_set_style_bg_color(g_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_overlay, LV_OPA_COVER, LV_PART_MAIN);
    no_scroll(g_overlay);

    /* 1) TEXTO DESLIZANDO da direita pra esquerda (fonte grande) */
    g_scroll = lv_label_create(g_overlay);
    lv_label_set_text(g_scroll, "CAST IN THE NAME OF GOD   YE NOT GUILTY");
    lv_obj_set_style_text_color(g_scroll, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_scroll, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_update_layout(g_scroll);
    int32_t tw = lv_obj_get_width(g_scroll);
    lv_obj_set_y(g_scroll, (240 - lv_obj_get_height(g_scroll)) / 2);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_scroll);
    lv_anim_set_values(&a, 240, -tw);
    lv_anim_set_time(&a, SCROLL_MS);
    lv_anim_set_exec_cb(&a, anim_set_x);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a, on_scroll_done);   /* -> comeca piscar+revelar */
    lv_anim_start(&a);

    /* 2) "YE NOT GUILTY" (fonte menor, cabe na tela), comeca invisivel */
    g_blink = lv_label_create(g_overlay);
    lv_label_set_text(g_blink, "YE NOT GUILTY");
    lv_obj_set_style_text_color(g_blink, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_blink, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(g_blink);
    lv_obj_set_style_opa(g_blink, LV_OPA_TRANSP, LV_PART_MAIN);
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(screen, lv_color_white(), LV_PART_MAIN);
    no_scroll(screen);

    /* conexao (USB/BLE) no topo */
    zmk_widget_output_status_init(&output_status_widget, screen);
    no_scroll(zmk_widget_output_status_obj(&output_status_widget));
    lv_obj_set_style_text_color(zmk_widget_output_status_obj(&output_status_widget),
                                lv_color_white(), LV_PART_MAIN);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_MID, 0, 40);

    /* camada atual no centro, fonte grande */
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    no_scroll(zmk_widget_layer_status_obj(&layer_status_widget));
    lv_obj_set_style_text_color(zmk_widget_layer_status_obj(&layer_status_widget),
                                lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(zmk_widget_layer_status_obj(&layer_status_widget),
                               &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_CENTER, 0, 0);

    boot_animation(screen);

    return screen;
}
