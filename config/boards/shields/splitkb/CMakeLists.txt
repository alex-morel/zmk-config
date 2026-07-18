# Tela de status custom, so quando selecionada. O dongle (color 240x240) usa
# uma tela propria; as metades (OLED mono retrato) usam a delas.
if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  if(CONFIG_SHIELD_SPLITKB_DONGLE)
    zephyr_library_sources(dongle_status_screen.c)
    zephyr_library_sources(bigzero.c)   # fonte com o Ø, usada como logo
  else()
    zephyr_library_sources(custom_status_screen.c)
    zephyr_library_sources(bigzero.c)   # fonte custom com o Ø (zero cortado) grande
  endif()
endif()
