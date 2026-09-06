/*
 * Filtro de "trava de eixo" CUSTOMIZADO deste projeto -- pedido explicito
 * ("forcar a mover mais em linha reta") depois do filtro de zona morta
 * (deadzone) nao ter resolvido bem o problema, que era desvio PERPENDICULAR
 * durante o movimento (nao ruido parado).
 *
 * IDEIA: guarda a magnitude RECENTE de cada eixo (X e Y) no proprio
 * dispositivo (dev->data, com decaimento a cada evento). Quando um evento
 * chega num eixo e a magnitude dele e pequena PERTO da magnitude recente
 * do OUTRO eixo (menos que param1%), esse evento e descartado -- ou seja,
 * se voce esta claramente arrastando mais num eixo, o desvio pequeno no
 * outro eixo e suprimido, deixando a linha mais reta. Se os dois eixos
 * tem magnitude parecida (movimento diagonal de verdade), nenhum e
 * suprimido.
 *
 * Baseado na estrutura do zip_xy_scaler (ZMK core,
 * app/src/pointing/input_processor_scaler.c), com estado proprio (nao usa
 * o "remainder" do zmk_input_processor_state, que e outra coisa).
 */

#define DT_DRV_COMPAT zmk_input_processor_axislock

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct axislock_config {
    uint8_t type;
    uint16_t x_code;
    uint16_t y_code;
};

struct axislock_data {
    int32_t x_mag; /* magnitude recente (com decaimento) do eixo X */
    int32_t y_mag; /* magnitude recente (com decaimento) do eixo Y */
};

/* decaimento a cada evento -- 3/4 do valor anterior, pra "esquecer" aos
 * poucos um eixo que parou de se mexer (sem precisar de timestamp) */
static int32_t decay(int32_t v) { return (v * 3) / 4; }

static int axislock_handle_event(const struct device *dev, struct input_event *event,
                                  uint32_t param1, uint32_t param2,
                                  struct zmk_input_processor_state *state) {
    const struct axislock_config *cfg = dev->config;
    struct axislock_data *data = dev->data;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* param1 = razao minima (%) em relacao ao outro eixo pra passar --
     * ex.: 35 = descarta se for menor que 35% da magnitude recente do
     * outro eixo. */
    int32_t ratio_pct = (int32_t)param1;

    int32_t value = event->value;
    int32_t abs_value = value < 0 ? -value : value;

    data->x_mag = decay(data->x_mag);
    data->y_mag = decay(data->y_mag);

    if (event->code == cfg->x_code) {
        int32_t other = data->y_mag;
        data->x_mag = abs_value > data->x_mag ? abs_value : data->x_mag;
        if (other > 0 && abs_value * 100 < other * ratio_pct) {
            LOG_DBG("axislock: suprime X=%d (Y recente=%d, razao %d%%)", value, other, ratio_pct);
            return ZMK_INPUT_PROC_STOP;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code == cfg->y_code) {
        int32_t other = data->x_mag;
        data->y_mag = abs_value > data->y_mag ? abs_value : data->y_mag;
        if (other > 0 && abs_value * 100 < other * ratio_pct) {
            LOG_DBG("axislock: suprime Y=%d (X recente=%d, razao %d%%)", value, other, ratio_pct);
            return ZMK_INPUT_PROC_STOP;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api axislock_driver_api = {
    .handle_event = axislock_handle_event,
};

#define AXISLOCK_INST(n)                                                                          \
    static const struct axislock_config axislock_config_##n = {                                   \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                           \
        .x_code = DT_INST_PROP(n, x_code),                                                         \
        .y_code = DT_INST_PROP(n, y_code),                                                         \
    };                                                                                             \
    static struct axislock_data axislock_data_##n = {0};                                           \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &axislock_data_##n, &axislock_config_##n, POST_KERNEL,     \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &axislock_driver_api);

DT_INST_FOREACH_STATUS_OKAY(AXISLOCK_INST)
