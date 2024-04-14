cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

function(pico_add_dis_c_output TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
            # COMMAND ${CMAKE_OBJDUMP} -h ${TARGET}${CMAKE_EXECUTABLE_SUFFIX} >${TARGET}.dis_c
            COMMAND echo ${TARGET}${CMAKE_EXECUTABLE_SUFFIX} >${TARGET}.dis_c
            COMMAND ${CMAKE_OBJDUMP} -S --disassemble ${TARGET}${CMAKE_EXECUTABLE_SUFFIX} >>${TARGET}.dis_c
            )
endfunction()

project(arm-m0-memcpy C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT_NAME} 
    memcpy_benchmark.c
    main.cpp
)


target_compile_options(${PROJECT_NAME}  PRIVATE -ftree-vrp -Wall -Wextra -Warray-bounds=2 -Wstringop-overflow=2  -Wenum-compare -Wconversion -Wsign-conversion -Wformat=2)
target_compile_options(${PROJECT_NAME}  PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -Wno-ignored-qualifiers >)
target_compile_options(${PROJECT_NAME}  PRIVATE $<$<COMPILE_LANGUAGE:C>: -Wenum-conversion  -Wno-unused-function -Wno-old-style-declaration >) # -Wc++-compat -pedantic -Wmissing-prototypes


target_compile_definitions(${PROJECT_NAME} PRIVATE
  PICO_DEFAULT_UART_TX_PIN=16
  PICO_DEFAULT_UART_RX_PIN=17
  PICO_MEM_IN_RAM=1
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
pico_add_dis_c_output(${PROJECT_NAME})

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
    memops_opt
    pico_stdlib
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/lib/memops_opt/test/test-memcpy.c")
    target_link_libraries(${PROJECT_NAME} 
        memops_opt_test
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        MEMOPS_OPT_TEST=1
    )
endif ()


if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/lib/memops_opt/test/memops_opt_test_imp.cmake")
    target_link_libraries(${PROJECT_NAME} 
        memops_opt_test_imp
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        MEMOPS_OPT_TEST_IMP=1
    )
endif ()

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

add_subdirectory(lib)
