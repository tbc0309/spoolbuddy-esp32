execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/apply_bsp.py"
    "${SB_ROOT}/managed_components/waveshare__esp32_p4_wifi6_touch_lcd_4_3"
    RESULT_VARIABLE SB_POWER_BSP_RESULT)
if(NOT SB_POWER_BSP_RESULT EQUAL 0)
    message(FATAL_ERROR "Rev3 display power lifecycle dependency fix failed")
endif()
