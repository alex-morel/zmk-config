/*
 * Tela de status do dongle (GC9A01 240x240 color) + ANIMACAO DE BOOT.
 *
 * A tela de status (layer + conexao) e montada com CORES EXPLICITAS (fundo
 * preto, texto branco) — no color, sem tema, o texto padrao sai preto sobre
 * preto e some (bug que investigamos). Por cima dela nasce um OVERLAY preto
 * com o logo Ø que anima no boot e some (fade-out), revelando a tela.
 *
 * Como a zmk_display_status_screen() roda UMA vez no boot e o LVGL tica pela
 * work queue do display, a animacao toca sozinha toda vez que o dongle liga.
 * O overlay nao e deletado: fica em opacidade 0 (invisivel; a tela nao tem
 * toque, entao nao atrapalha).
 */

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>

LV_FONT_DECLARE(bigzero);   // fonte custom com o glifo Ø (0xD8), tamanho 56

static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_output_status output_status_widget;

/* wrappers: o exec_cb da anim e (void*, int32_t); os setters de estilo pedem
 * um 3o argumento (selector). */
static void anim_set_opa(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, LV_PART_MAIN);
}
static void anim_set_translate_y(void *var, int32_t v) {
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, LV_PART_MAIN);
}

static void boot_animation(lv_obj_t *screen) {
    /* overlay preto cobrindo os 240x240, por cima da tela de status */
    lv_obj_t *overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 240, 240);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);

    /* logo Ø no centro, comeca invisivel e 40px abaixo */
    lv_obj_t *logo = lv_label_create(overlay);
    lv_label_set_text(logo, "\xC3\x98");   /* Ø em UTF-8 */
    lv_obj_set_style_text_color(logo, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(logo, &bigzero, LV_PART_MAIN);
    lv_obj_center(logo);
    lv_obj_set_style_opa(logo, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_anim_t a;

    /* 1) logo sobe deslizando (translate_y 40 -> 0) */
    lv_anim_init(&a);
    lv_anim_set_var(&a, logo);
    lv_anim_set_values(&a, 40, 0);
    lv_anim_set_time(&a, 700);
    lv_anim_set_delay(&a, 150);
    lv_anim_set_exec_cb(&a, anim_set_translate_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* 2) logo aparece (opacidade 0 -> 255), junto com o slide */
    lv_anim_init(&a);
    lv_anim_set_var(&a, logo);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 700);
    lv_anim_set_delay(&a, 150);
    lv_anim_set_exec_cb(&a, anim_set_opa);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* 3) depois de segurar, o overlay inteiro some (255 -> 0) e revela a tela */
    lv_anim_init(&a);
    lv_anim_set_var(&a, overlay);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 600);
    lv_anim_set_delay(&a, 1500);
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
