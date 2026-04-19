cmake_minimum_required(VERSION 3.20)
project(mkBatteryChecker VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(mkBatteryChecker WIN32
    src/main.c
    src/tray_app.c
    src/device_scanner.c
    src/battery_monitor.c
    src/hid_util.c
)

target_compile_definitions(mkBatteryChecker PRIVATE UNICODE _UNICODE)

target_link_libraries(mkBatteryChecker PRIVATE
    hid
    setupapi
    shell32
    user32
    advapi32
)

if(MSVC)
    target_compile_options(mkBatteryChecker PRIVATE
        /W4 /WX-
        $<$<CONFIG:Release>:/Os>
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/GS->
    )

    target_link_options(mkBatteryChecker PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        $<$<CONFIG:Release>:/MANIFEST:NO>   #
        $<$<CONFIG:Release>:/INCREMENTAL:NO>
    )
endif()
