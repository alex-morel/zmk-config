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
 *   deslize da frase ~3.0s ; "YE NOT GUILTY" PULSA (escurece e clareia
 *   suavemente, nao apaga) com periodo ~600ms, ~4 vezes. */
#define TICK_MS 40            /* timer fino, pro pulso ficar suave */
#define SCROLL_MS 3000        /* deslize (= anime) */
#define PULSE_PERIOD 1000     /* periodo de um pulso (mais lento) */
#define PULSE_COUNT 4         /* 4 pulsos */
#define PULSE_TOTAL (PULSE_PERIOD * PULSE_COUNT)
#define PULSE_MIN_OPA 120     /* opacidade no ponto mais escuro (~47%, nao apaga) */
#define FADE_MS 500

static lv_obj_t *g_overlay;
static lv_obj_t *g_scroll;
static lv_obj_t *g_blink;
static int g_elapsed;         /* ms desde o inicio da animacao */

static void anim_set_opa(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, LV_PART_MAIN);
}
static void anim_set_x(void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t *)var, v);
}

/* um timer so cuida de: esperar o deslize -> pulsar -> revelar */
static void seq_timer_cb(lv_timer_t *t) {
    g_elapsed += TICK_MS;

    if (g_elapsed < SCROLL_MS) {            /* deslize acontecendo (lv_anim) */
        return;
    }

    int pt = g_elapsed - SCROLL_MS;         /* tempo dentro da fase de pulso */
    if (pt < PULSE_TOTAL) {
        if (pt < TICK_MS) {                 /* primeira entrada: esconde o deslize */
            lv_obj_add_flag(g_scroll, LV_OBJ_FLAG_HIDDEN);
        }
        /* onda triangular: cheio (255) -> escuro (MIN) -> cheio, a cada periodo */
        int tp = pt % PULSE_PERIOD;         /* 0..PULSE_PERIOD */
        int half = PULSE_PERIOD / 2;
        int tri = (tp < half) ? tp : (PULSE_PERIOD - tp);   /* 0..half..0 */
        int opa = 255 - (255 - PULSE_MIN_OPA) * tri / half;
        lv_obj_set_style_opa(g_blink, (lv_opa_t)opa, LV_PART_MAIN);
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

    /* 1) TEXTO DESLIZANDO (montserrat 28). So "CAST IN THE NAME OF GOD" (o
     *    "YE NOT GUILTY" nao desliza; aparece pulsando na fase seguinte).
     *    Texto ~340px; entra em x=240 e sai todo em x=-380. */
    g_scroll = lv_label_create(g_overlay);
    lv_label_set_text(g_scroll, "CAST IN THE NAME OF GOD");
    lv_obj_set_style_text_color(g_scroll, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_scroll, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_y(g_scroll, 105);        /* ~centro vertical pra fonte 28 */

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_scroll);
    lv_anim_set_values(&a, 240, -380);
    lv_anim_set_time(&a, SCROLL_MS);
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
    g_elapsed = 0;
    lv_timer_create(seq_timer_cb, TICK_MS, NULL);
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
