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
#include <zephyr/logging/log.h>
#include <lvgl.h>
#ifdef CONFIG_SYS_HEAP_RUNTIME_STATS
#include <lvgl_mem.h>
#endif

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ---------- DIAGNOSTICO: relatorio periodico do heap do LVGL ----------
 * Investigando a corrupcao da tela que comeca "algum tempo depois de ligado":
 * se o pool do LVGL vaza/fragmenta, o numero de alocado sobe ate falhar --
 * este timer loga a curva a cada 10s pelo log NORMAL do ZMK (o LV_USE_LOG
 * travou o boot em qualquer modo; nao usar). Compila pra nada quando
 * SYS_HEAP_RUNTIME_STATS esta desligado (build de producao). */
#ifdef CONFIG_SYS_HEAP_RUNTIME_STATS
static void heap_report_cb(lv_timer_t *t) {
    struct sys_memory_stats st;
    lvgl_heap_stats(&st);
    LOG_INF("lvgl heap: alocado %zu livre %zu pico %zu", st.allocated_bytes, st.free_bytes,
            st.max_allocated_bytes);
}
#endif

/* O ZMK amarra DUAS coisas na mesma flag:
 *   target_sources_ifdef(CONFIG_ZMK_BATTERY_REPORTING ... events/battery_state_changed.c)
 *   target_sources_ifdef(CONFIG_ZMK_BATTERY_REPORTING ... battery.c)
 * Ou seja, a implementacao do EVENTO vem junto com o amostrador de 60s. No
 * dongle desligamos BATTERY_REPORTING (ele nao tem bateria e travava na
 * amostragem), entao o evento de bateria de PERIFERICO ficaria sem simbolo --
 * quebrando ate o central.c do proprio ZMK. Como o .c do ZMK nao e compilado
 * aqui, fornecemos so a implementacao do evento (sem duplicar simbolo). */
/* fonte custom (bigzero.c) com o glifo Ø grande -- o logo do projeto */
LV_FONT_DECLARE(bigzero);

ZMK_EVENT_IMPL(zmk_peripheral_battery_state_changed);

static struct zmk_widget_output_status output_status_widget;

/* ---------- CAMADA (rotulo proprio, nao o widget do ZMK) ----------
 * Motivo: o widget do ZMK usa LV_SIZE_CONTENT e RE-DIMENSIONA quando o texto
 * muda; no LVGL isso deixa residuo da area antiga -> era o glitch visual a
 * cada troca de camada. Aqui o rotulo tem TAMANHO FIXO e texto centralizado,
 * entao a area invalidada e sempre a mesma e nao sobra lixo. De quebra, com
 * caixa fixa da pra usar fonte grande sem transbordar. */
static lv_obj_t *g_layer_lbl;

struct layer_state {
    uint8_t index;
};
static struct layer_state layer_get_state(const zmk_event_t *eh) {
    return (struct layer_state){.index = zmk_keymap_highest_layer_active()};
}
static void layer_update_cb(struct layer_state s) {
    if (g_layer_lbl != NULL) {
        lv_label_set_text_fmt(g_layer_lbl, "%d", s.index);
    }
}
ZMK_DISPLAY_WIDGET_LISTENER(dongle_layer, struct layer_state, layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(dongle_layer, zmk_layer_state_changed);

/* ---------- bateria das METADES (perifericos) ----------
 * O central recebe zmk_peripheral_battery_state_changed com .source = indice
 * do slot do periferico.
 *
 * CUIDADO (motivo do g_seen): no ZMK o array de niveis nasce zerado, entao um
 * periferico que NUNCA conectou le 0 -- igualzinho a "bateria vazia". Por isso
 * so mostramos numero de quem ja mandou dado; o resto fica "--". Assim a
 * metade direita (ainda nao montada) aparece como ausente, nao como 0%.
 *
 * OBS: .source e o indice do SLOT, atribuido por ordem de conexao -- nao e
 * fixo "esquerda/direita". Por isso as duas nao levam rotulo L/R. */
#define PERIPH_N 2
#define BATT_W 44        /* largura do corpo do icone */
#define BATT_H 20
#define BATT_INNER 36    /* largura util do preenchimento */

static lv_obj_t *g_batt_fill[PERIPH_N];
static lv_obj_t *g_batt_lbl[PERIPH_N];
static uint8_t g_batt_level[PERIPH_N];
static bool g_batt_seen[PERIPH_N];

static void batt_refresh(int i) {
    if (g_batt_fill[i] == NULL || g_batt_lbl[i] == NULL) {
        return;
    }
    if (g_batt_seen[i]) {
        uint8_t pct = g_batt_level[i] > 100 ? 100 : g_batt_level[i];
        int w = (BATT_INNER * pct) / 100;
        lv_obj_set_width(g_batt_fill[i], w > 1 ? w : 1);
        lv_obj_set_style_opa(g_batt_fill[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_label_set_text_fmt(g_batt_lbl[i], "%d", pct);
    } else {
        /* nunca conectou: icone vazio + "--" (nao mostrar 0, seria enganoso) */
        lv_obj_set_style_opa(g_batt_fill[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_label_set_text(g_batt_lbl[i], "--");
    }
}

/* monta um icone de bateria (contorno + preenchimento + polo) e o numero */
static void batt_create(lv_obj_t *parent, int i, lv_coord_t x_ofs) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, BATT_W, BATT_H);
    lv_obj_set_style_border_color(box, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(box, 3, LV_PART_MAIN);
    lv_obj_align(box, LV_ALIGN_BOTTOM_MID, x_ofs, -48);

    g_batt_fill[i] = lv_obj_create(box);
    lv_obj_remove_style_all(g_batt_fill[i]);
    lv_obj_set_size(g_batt_fill[i], 1, BATT_H - 8);
    lv_obj_set_style_bg_color(g_batt_fill[i], lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_batt_fill[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(g_batt_fill[i], LV_ALIGN_LEFT_MID, 2, 0);

    /* polo positivo, colado na direita do corpo */
    lv_obj_t *nub = lv_obj_create(parent);
    lv_obj_remove_style_all(nub);
    lv_obj_set_size(nub, 4, 8);
    lv_obj_set_style_bg_color(nub, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(nub, LV_ALIGN_BOTTOM_MID, x_ofs + BATT_W / 2 + 2, -54);

    g_batt_lbl[i] = lv_label_create(parent);
    lv_obj_set_style_text_color(g_batt_lbl[i], lv_color_white(), LV_PART_MAIN);
    lv_obj_align(g_batt_lbl[i], LV_ALIGN_BOTTOM_MID, x_ofs, -22);

    batt_refresh(i);
}

/* Usa o listener de DISPLAY do ZMK: ele marshala pra work queue do display,
 * que e o jeito seguro de mexer no LVGL a partir de um evento. */
struct periph_batt_state {
    uint8_t source;
    uint8_t level;
};
static struct periph_batt_state periph_batt_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL) {
        return (struct periph_batt_state){.source = 0xFF, .level = 0}; /* chamada inicial */
    }
    return (struct periph_batt_state){.source = ev->source, .level = ev->state_of_charge};
}
static void periph_batt_update_cb(struct periph_batt_state s) {
    if (s.source >= PERIPH_N) {
        return; /* ignora a chamada inicial e qualquer slot fora do esperado */
    }
    g_batt_level[s.source] = s.level;
    g_batt_seen[s.source] = true;
    batt_refresh(s.source);
}
ZMK_DISPLAY_WIDGET_LISTENER(dongle_periph_batt, struct periph_batt_state, periph_batt_update_cb,
                            periph_batt_get_state)
ZMK_SUBSCRIPTION(dongle_periph_batt, zmk_peripheral_battery_state_changed);

/* ---------- ANIMACAO DE BOOT (dimensionada pra banda do SPI) ----------
 * LICAO DE ARQUITETURA (2026-07-18): um frame CHEIO de 240x240x16bpp = 115KB
 * = ~115ms no SPI a 8MHz. A versao antiga deslizava texto pela tela inteira
 * com tick de 40ms -- pedia ~25fps de um barramento que entrega ~8fps de
 * frame cheio, saturando a fila do display (glitches/instabilidade). Os
 * modulos de referencia so animam AREAS PEQUENAS (prospector: roller;
 * englmaxi: icones em OLED mono de 1KB/frame).
 *
 * Nova sequencia, so com updates de area pequena (a caixa dos labels):
 *   1) "CAST IN THE NAME OF GOD" aparece centrado (quebrado em linhas), ~2s;
 *   2) a frase corre da direita pra esquerda em velocidade CONSTANTE e para
 *      exatamente com o "YE NOT GUILTY" no centro da tela;
 *   3) o "YE NOT GUILTY" PULSA no lugar (periodo 600ms, 4x, fade suave
 *      100%<->47% -- ritmo medido do GIF do anime);
 *   4) o overlay e ESCONDIDO de uma vez (um unico redraw de tela cheia),
 *      revelando a tela de status. Sem fade de tela inteira.
 * O deslize so e viavel com a tela SOLDADA no SPIM3 a 32MHz (frame ~29ms);
 * na era dupont/8MHz isso saturava a fila do display e derrubava o dongle. */
#define TICK_MS 30            /* passo do timer (~33fps no deslize) */
#define SLIDE_MS 5000         /* duracao da corrida da frase */
#define PULSE_PERIOD 600      /* periodo de um pulso (ritmo do anime) */
#define PULSE_COUNT 4
#define PULSE_TOTAL (PULSE_PERIOD * PULSE_COUNT)
#define PULSE_MIN_OPA 120     /* ponto mais escuro do pulso (~47%, nao apaga) */

static lv_obj_t *g_overlay;
static lv_obj_t *g_cast;
static lv_obj_t *g_blink;
static int g_elapsed;         /* ms desde o inicio da animacao */
static int g_x_start, g_x_end;

/* fases: frase correndo (ease-out) -> pulso -> esconder overlay */
static void seq_timer_cb(lv_timer_t *t) {
    g_elapsed += TICK_MS;

    if (g_elapsed <= SLIDE_MS) {
        /* deslize LINEAR: velocidade constante do inicio ao fim */
        int x = g_x_start + (int)((int64_t)(g_x_end - g_x_start) * g_elapsed / SLIDE_MS);
        lv_obj_align(g_cast, LV_ALIGN_LEFT_MID, x, 0);
        return;
    }

    int pt = g_elapsed - SLIDE_MS;
    if (pt < PULSE_TOTAL) {
        if (pt <= TICK_MS) {    /* primeira entrada da fase (pt chega == TICK_MS) */
            /* troca de fase: a frase parou com o "YE NOT GUILTY" NA MESMA
             * posicao do g_blink centrado -> swap invisivel */
            lv_obj_add_flag(g_cast, LV_OBJ_FLAG_HIDDEN);
        }
        /* onda triangular: cheio -> escuro -> cheio a cada periodo */
        int tp = pt % PULSE_PERIOD;
        int half = PULSE_PERIOD / 2;
        int tri = (tp < half) ? tp : (PULSE_PERIOD - tp);
        int opa = 255 - (255 - PULSE_MIN_OPA) * tri / half;
        lv_obj_set_style_opa(g_blink, (lv_opa_t)opa, LV_PART_MAIN);
        return;
    }

    /* revela a tela de status: UM redraw de tela cheia, sem fade */
    lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_timer_delete(t);
}

static void boot_animation(lv_obj_t *screen) {
    /* overlay preto cobrindo os 240x240, por cima da tela de status */
    g_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(g_overlay);
    lv_obj_set_size(g_overlay, 240, 240);
    lv_obj_set_style_bg_color(g_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(g_overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* 1) a frase inteira numa linha so (sem largura fixa = sem quebra),
     * comecando fora da tela a direita */
    g_cast = lv_label_create(g_overlay);
    lv_label_set_text(g_cast, "CAST IN THE NAME OF GOD   YE NOT GUILTY");
    lv_obj_set_style_text_color(g_cast, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_cast, &lv_font_montserrat_28, LV_PART_MAIN);

    /* 2) "YE NOT GUILTY" centrado, invisivel ate a fase do pulso */
    g_blink = lv_label_create(g_overlay);
    lv_label_set_text(g_blink, "YE NOT GUILTY");
    lv_obj_set_style_text_color(g_blink, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_blink, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(g_blink);
    lv_obj_set_style_opa(g_blink, LV_OPA_TRANSP, LV_PART_MAIN);

    /* Ponto de parada calculado com as larguras REAIS dos labels: o centro
     * do trecho final ("YE NOT GUILTY") tem que cair no centro da tela
     * (x=120), que e exatamente onde o g_blink centrado esta -> a troca
     * frase->pulso nao mexe um pixel. */
    lv_obj_update_layout(g_overlay);
    int w_full = lv_obj_get_width(g_cast);
    int w_tail = lv_obj_get_width(g_blink);
    g_x_start = 240;                            /* fora da tela, a direita */
    g_x_end = 120 - w_full + w_tail / 2;
    lv_obj_align(g_cast, LV_ALIGN_LEFT_MID, g_x_start, 0);

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

    /* LOGO "Ø" no topo (fonte custom bigzero) */
    lv_obj_t *logo = lv_label_create(screen);
    lv_label_set_text(logo, "\xC3\x98");            /* Ø em UTF-8 */
    lv_obj_set_style_text_color(logo, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(logo, &bigzero, LV_PART_MAIN);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 18);

    /* conexao (USB/BLE) — desceu pra logo acima das baterias, pra dar o topo
     * ao logo. Cor branca explicita (definir COR no obj do widget e seguro;
     * era o no_scroll que travava o boot). */
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_set_style_text_color(zmk_widget_output_status_obj(&output_status_widget),
                                lv_color_white(), LV_PART_MAIN);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_BOTTOM_MID, 0, -88);

    /* camada atual no centro, caixa FIXA (sem redimensionar = sem glitch) */
    g_layer_lbl = lv_label_create(screen);
    lv_obj_set_size(g_layer_lbl, 120, 64);
    lv_obj_set_style_text_align(g_layer_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_layer_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_layer_lbl, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_label_set_text(g_layer_lbl, "0");
    lv_obj_align(g_layer_lbl, LV_ALIGN_CENTER, 0, 0);
    dongle_layer_init();

    /* bateria das duas metades, lado a lado na parte de baixo */
    batt_create(screen, 0, -38);
    batt_create(screen, 1, 38);
    dongle_periph_batt_init();

    boot_animation(screen);

#ifdef CONFIG_SYS_HEAP_RUNTIME_STATS
    lv_timer_create(heap_report_cb, 10000, NULL);   /* diagnostico: heap a cada 10s */
#endif

    return screen;
}
