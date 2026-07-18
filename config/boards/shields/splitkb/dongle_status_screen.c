/*
 * Tela de status do dongle (GC9A01 240x240 color) + ANIMACAO DE BOOT.
 *
 * Boot:
 *   1) "CAST IN THE NAME OF GOD   YE NOT GUILTY" desliza da direita pra esquerda;
 *   2) "YE NOT GUILTY" pisca 4 vezes no centro;
 *   3) o overlay preto some (fade) e revela a tela de status (layer + conexao).
 *
 * Licoes aprendidas (nao repetir):
 *  - a tela de status fica MINIMA (padrao do prospector). NAO chamar funcoes
 *    lv_obj_* nos objetos internos dos widgets do ZMK (ex.: desligar scroll) ->
 *    trava o boot.
 *  - cores EXPLICITAS (fundo preto, texto branco): no color, sem tema, o texto
 *    padrao sai preto sobre preto e some.
 *  - a sequencia pos-scroll usa UM lv_timer com contador de fases (delays de
 *    animacao nao dispararam de forma confiavel aqui).
 *  - rolagem desligada no overlay (o texto largo tornava-o rolavel = listras).
 */

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>

static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_output_status output_status_widget;

/* Tempos medidos do GIF do anime (big-o-cast-in-the-name-of-god.gif):
 *   deslize da frase ~3.0s ; "YE NOT GUILTY" pulsa com periodo ~600ms. */
#define STEP_MS 300           /* estado do pisca = 300ms -> periodo 600ms (= anime) */
#define SCROLL_STEPS 10       /* 10 * 300ms = 3000ms de deslize (= anime) */
#define BLINK_STEPS 8         /* 8 passos = 4 piscadas */
#define FADE_MS 500

static lv_obj_t *g_overlay;
static lv_obj_t *g_scroll;
static lv_obj_t *g_blink;
static int g_phase;

static void anim_set_opa(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, LV_PART_MAIN);
}
static void anim_set_x(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t *)var, v);
}

/* um timer so cuida de: esperar o deslize -> piscar -> revelar */
static void seq_timer_cb(lv_timer_t *t) {
    if (g_phase < SCROLL_STEPS) {          /* deslize acontecendo (lv_anim) */
        g_phase++;
        return;
    }
    int b = g_phase - SCROLL_STEPS;
    if (b < BLINK_STEPS) {                  /* pisca "YE NOT GUILTY" */
        if (b == 0) {
            lv_obj_add_flag(g_scroll, LV_OBJ_FLAG_HIDDEN);   /* esconde o deslize */
        }
        lv_obj_set_style_opa(g_blink, (b % 2 == 0) ? LV_OPA_COVER : LV_OPA_TRANSP,
                             LV_PART_MAIN);
        g_phase++;
        return;
    }
    /* revela: overlay some (fade) */
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

static void boot_animation(lv_obj_t *screen) {
    /* overlay preto cobrindo os 240x240, por cima da tela de status */
    g_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(g_overlay);
    lv_obj_set_size(g_overlay, 240, 240);
    lv_obj_set_style_bg_color(g_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_overlay, LV_OPA_COVER, LV_PART_MAIN);
    /* sem rolagem: o texto largo tornava o overlay rolavel (barras = listras) */
    lv_obj_set_scrollbar_mode(g_overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* 1) TEXTO DESLIZANDO (montserrat 28). Faixa de x FIXA (sem update_layout):
     *    o texto tem ~640px; entra em x=240 e sai todo em x=-720. */
    g_scroll = lv_label_create(g_overlay);
    lv_label_set_text(g_scroll, "CAST IN THE NAME OF GOD   YE NOT GUILTY");
    lv_obj_set_style_text_color(g_scroll, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_scroll, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_y(g_scroll, 105);        /* ~centro vertical pra fonte 28 */

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_scroll);
    lv_anim_set_values(&a, 240, -720);
    lv_anim_set_time(&a, SCROLL_STEPS * STEP_MS);
    lv_anim_set_exec_cb(&a, anim_set_x);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);

    /* 2) "YE NOT GUILTY" (mesma fonte), comeca invisivel */
    g_blink = lv_label_create(g_overlay);
    lv_label_set_text(g_blink, "YE NOT GUILTY");
    lv_obj_set_style_text_color(g_blink, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_blink, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(g_blink);
    lv_obj_set_style_opa(g_blink, LV_OPA_TRANSP, LV_PART_MAIN);

    /* 3) o timer conduz a sequencia */
    g_phase = 0;
    lv_timer_create(seq_timer_cb, STEP_MS, NULL);
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    /* conexao (USB/BLE) no topo. Cor branca explicita (definir COR no obj do
     * widget e seguro -- funcionou no build do Ø; so o no_scroll travava). */
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_set_style_text_color(zmk_widget_output_status_obj(&output_status_widget),
                                lv_color_white(), LV_PART_MAIN);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_MID, 0, 40);

    /* camada atual no centro. Fonte PADRAO (16): a 48 o texto transbordava a
     * caixa do widget e gerava scrollbar (a listra que sobrava). */
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_color(zmk_widget_layer_status_obj(&layer_status_widget),
                                lv_color_white(), LV_PART_MAIN);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_CENTER, 0, 0);

    boot_animation(screen);

    return screen;
}
