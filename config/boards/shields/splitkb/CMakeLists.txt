# Compila a tela de status custom (retrato) SO quando ela esta selecionada.
# (Desativada durante o diagnostico -> usa a tela built-in do ZMK.)
if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  zephyr_library_sources(custom_status_screen.c)
endif()
