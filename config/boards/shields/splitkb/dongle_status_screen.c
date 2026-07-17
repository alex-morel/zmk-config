/*
 * TELA DE DIAGNOSTICO do dongle (GC9A01 240x240 color).
 *
 * Nao usa widgets nem tema: pinta o fundo de VERMELHO e escreve um "A" branco
 * gigante no centro, com cores EXPLICITAS. Serve pra provar o pipeline
 * display+LVGL+flush isolado do resto:
 *   - vermelho com A  -> tudo funciona; o problema era a status screen/tema
 *   - vermelho sem A   -> pipeline ok, faltou a fonte
 *   - continua preto   -> o flush nao chega no painel (problema mais fundo)
 */

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* fundo vermelho solido (inequivocamente != preto, mesmo com inversao) */
    lv_obj_set_style_bg_color(screen, lv_color_make(0xFF, 0x00, 0x00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "A");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return screen;
}
