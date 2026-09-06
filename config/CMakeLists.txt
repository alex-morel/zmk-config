# zmk-config tambem funciona como um MODULO Zephyr proprio (ver
# zephyr/module.yml, dts_root=.) -- so pro filtro de ruido customizado da
# trackball (input_processor_deadzone.c). Nao mexe em nada do resto do
# ZMK_CONFIG (keymap/overlay/conf continuam funcionando pelo mecanismo
# normal, separado disso).
add_subdirectory(src/pointing)
