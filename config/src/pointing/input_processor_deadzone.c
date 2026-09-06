/*
 * Filtro de ruido/zona morta CUSTOMIZADO deste projeto -- o ZMK core nao
 * tem um input-processor pronto pra isso (o mv-deadzone so existe no
 * driver de joystick ANALOGICO, nao se aplica a eventos de movimento
 * RELATIVO como o da trackball/PMW3610).
 *
 * IDEIA: descarta (nao propaga) qualquer evento INPUT_EV_REL cujo |valor|
 * seja menor que o limiar (param1) -- filtra pequenos saltos/tremores
 * causados por folga mecanica residual da trackball, sem cortar
 * movimento intencional maior que o limiar.
 *
 * Baseado na estrutura do zip_xy_scaler (ZMK core,
 * app/src/pointing/input_processor_scaler.c).
 */

#define DT_DRV_COMPAT zmk_input_processor_deadzone

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct deadzone_config {
    uint8_t type;
    size_t codes_len;
    uint16_t codes[];
};

static int deadzone_handle_event(const struct device *dev, struct input_event *event,
                                  uint32_t param1, uint32_t param2,
                                  struct zmk_input_processor_state *state) {
    const struct deadzone_config *cfg = dev->config;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    bool matches = false;
    for (int i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] == event->code) {
            matches = true;
            break;
        }
    }
    if (!matches) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* param1 = limiar (LIMIAR na instancia dtsi, ex.: <&zip_xy_deadzone 2>) */
    int32_t threshold = (int32_t)param1;
    int32_t value = event->value;
    int32_t abs_value = value < 0 ? -value : value;

    if (abs_value < threshold) {
        LOG_DBG("deadzone: descartado delta %d (limiar %d) code %d", value, threshold,
                event->code);
        /* PARA aqui -- nem propaga um evento de valor 0 pra frente
         * (economiza banda do split e nao acorda o mapper de scroll a toa) */
        return ZMK_INPUT_PROC_STOP;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api deadzone_driver_api = {
    .handle_event = deadzone_handle_event,
};

#define DEADZONE_INST(n)                                                                         \
    static const struct deadzone_config deadzone_config_##n = {                                  \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                          \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                 \
        .codes = DT_INST_PROP(n, codes),                                                         \
    };                                                                                           \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &deadzone_config_##n, POST_KERNEL,                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &deadzone_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DEADZONE_INST)
