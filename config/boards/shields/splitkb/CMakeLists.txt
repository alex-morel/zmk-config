# Compila a tela de status custom (retrato) quando o display esta ativo.
if(CONFIG_ZMK_DISPLAY)
  zephyr_library_sources(custom_status_screen.c)
endif()
