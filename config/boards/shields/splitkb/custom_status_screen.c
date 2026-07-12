/*
 * Tela de status em RETRATO para OLED SSD1306 (128x32) montada girada 90 graus.
 *
 * O ZMK/LVGL NAO consegue rotacionar o display mono por software
 * (bug conhecido: zmkfirmware/zmk#1749). A solucao (usada pelo nice!view):
 * desenhar num CANVAS em formato L8 e rotacionar o BUFFER do canvas com
 * lv_draw_sw_rotate (L8 = menor formato suportado pelo sw_rotate; 1-bit falha).
 *
 * ESTA VERSAO E UM TESTE ESTATICO: so um texto rotacionado, pra validar a
 * tecnica no hardware. Se funcionar, adiciono os widgets vivos (layer/bateria).
 */

#include <lvgl.h>
#include <zmk/display/status_screen.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DW 128 /* largura do display */
#define DH 32  /* altura do display  */
#define CF LV_COLOR_FORMAT_L8
#define BPP LV_COLOR_FORMAT_GET_BPP(CF)

/* canvas de DESENHO: retrato (DH de largura x DW de altura = 32 x 128) */
static uint8_t draw_buf[LV_CANVAS_BUF_SIZE(DH, DW, BPP, LV_DRAW_BUF_STRIDE_ALIGN)];
/* canvas de SAIDA: orientacao do display (128 x 32), recebe o buffer rotacionado */
static uint8_t out_buf[LV_CANVAS_BUF_SIZE(DW, DH, BPP, LV_DRAW_BUF_STRIDE_ALIGN)];

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* canvas de saida (o que aparece na tela) */
    lv_obj_t *out = lv_canvas_create(screen);
    lv_canvas_set_buffer(out, out_buf, DW, DH, CF);
    lv_obj_align(out, LV_ALIGN_TOP_LEFT, 0, 0);

    /* canvas de desenho, em retrato (32 x 128) — temporario */
    lv_obj_t *draw = lv_canvas_create(screen);
    lv_canvas_set_buffer(draw, draw_buf, DH, DW, CF);
    lv_canvas_fill_bg(draw, lv_color_white(), LV_OPA_COVER);

    /* desenha um texto de teste no canvas retrato */
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_black();
    label_dsc.font = &lv_font_montserrat_16;
    label_dsc.align = LV_TEXT_ALIGN_CENTER;
    label_dsc.text = "ZMK\nOK";

    lv_layer_t layer;
    lv_canvas_init_layer(draw, &layer);
    lv_area_t coords = {0, 8, DH, DW};
    lv_draw_label(&layer, &label_dsc, &coords);
    lv_canvas_finish_layer(draw, &layer);

    /* rotaciona draw_buf (32x128) -> out_buf (128x32), 90 graus */
    uint32_t src_stride = lv_draw_buf_width_to_stride(DH, CF);
    uint32_t dst_stride = lv_draw_buf_width_to_stride(DW, CF);
    lv_draw_sw_rotate(draw_buf, out_buf, DH, DW, src_stride, dst_stride,
                      LV_DISPLAY_ROTATION_90, CF);

    /* remove o canvas de desenho; so o de saida (rotacionado) fica visivel */
    lv_obj_delete(draw);
    lv_obj_invalidate(out);

    return screen;
}
