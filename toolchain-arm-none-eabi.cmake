# Cross-compilation toolchain file for bare-metal STM32F401 (Cortex-M4F).
# Usage: cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-none-eabi.cmake

set(CMAKE_SYSTEM_NAME Generic)      # no OS -- bare metal
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)

# A bare-metal target can't run a test program during CMake's compiler
# sanity check -- there's no OS underneath to execute it. This restricts
# that check to linking a static library instead of building+running an
# executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
