cmake_minimum_required(VERSION 3.20)
project(nucleo_f401_firmware C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Feeds VS Code's IntelliSense a real, exact record of the flags used --
# replaces having to hand-maintain .vscode/c_cpp_properties.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

add_compile_options(
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -g3 # DWARF debug info -- source lines, local variables, macro expansions.
        # No -O flag is set anywhere here, so this is already an -O0 build,
        # the most debugger-friendly optimization level (nothing reordered
        # or eliminated) -- -g3 alone is what was actually missing.
)

add_link_options(
    ${MCU_FLAGS}
    -nostartfiles
    --specs=nosys.specs
    -T${CMAKE_SOURCE_DIR}/linker_script.ld
)

add_executable(blink
    src/test.c
    src/startup.c
    src/serial_interfaces.c
    src/gpio.c
    src/timers.c
    src/onewire.c
    src/app_tasks.c
    FreeRTOS-Kernel/tasks.c
    FreeRTOS-Kernel/list.c
    FreeRTOS-Kernel/queue.c
    FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c
    FreeRTOS-Kernel/portable/MemMang/heap_4.c
)

# include/ holds every project header (registers.h, gpio.h, app_tasks.h,
# etc.) plus FreeRTOSConfig.h -- FreeRTOS's own sources reach it via this
# same include path, exactly like they did when it sat at the project root.
target_include_directories(blink PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/FreeRTOS-Kernel/include
    ${CMAKE_SOURCE_DIR}/FreeRTOS-Kernel/portable/GCC/ARM_CM4F
)

set_target_properties(blink PROPERTIES SUFFIX ".elf")

# .elf -> .bin, same job your objcopy line was doing by hand.
add_custom_command(TARGET blink POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:blink> ${CMAKE_BINARY_DIR}/blink.bin
    COMMENT "Generating blink.bin"
)
