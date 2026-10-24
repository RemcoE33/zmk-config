if(CONFIG_ZMK_DISPLAY AND CONFIG_NICE_VIEW_WIDGET_STATUS)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources(custom_status_screen.c)
  zephyr_library_sources(widgets/bolt.c)
  zephyr_library_sources(widgets/util.c)

  # Show the artwork slideshow on BOTH halves (art + battery/connection canvas).
  # widgets/status.c (the central status widget) is intentionally not compiled.
  zephyr_library_sources(widgets/art.c)
  zephyr_library_sources(widgets/peripheral_status.c)
endif()