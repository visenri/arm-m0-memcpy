/*
 * Copyright (c) 2024 Visenri.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Benchmark function for memcpy.
 */
#include "memcpy_benchmark.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/sync.h" // save & restore interrupts
#include "hardware/structs/xip_ctrl.h" // XIP macros
#include "hardware/timer.h"

#define MEMCPY_BENCHMARK_MEM_ROM 0
#define MEMCPY_BENCHMARK_MEM_RAM 1
#define MEMCPY_BENCHMARK_MEM_FLASH_WITH_CACHE 2
#define MEMCPY_BENCHMARK_MEM_FLASH_WITHOUT_CACHE 3

#define MEMCPY_BENCHMARK_MEM MEMCPY_BENCHMARK_MEM_ROM

#define TEST_SRC_OFFSET 3
#define TEST_DST_OFFSET 3
#define TEST_MISALIGNED 1


#define ROW_SIZE 512
#define ROW_SIZE_EXTRA (ROW_SIZE + 16)
#define ROW_COUNT 100

#if MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_ROM || MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_RAM
    // Keep ROM access valid for RP2040, ROW_SIZE_EXTRA + (ROW_COUNT - 1) * DST_OFFSET_PER_ROW_COPY < 16kb.
    // Keep RAM usage low.
    #define SRC_OFFSET_PER_ROW_COPY 4
#else
    #if PICO_FLASH_SIZE_BYTES > 2 * ROW_SIZE_EXTRA * ROW_COUNT
        #define SRC_OFFSET_PER_ROW_COPY ROW_SIZE_EXTRA
    #else
        #define SRC_OFFSET_PER_ROW_COPY 4
    #endif
#endif
#define DST_OFFSET_PER_ROW_COPY 4 // 4 or ROW_SIZE_EXTRA , 4 Keeps RAM usage low.


#if MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_FLASH_WITH_CACHE || MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_FLASH_WITHOUT_CACHE
    // XIP FLASH is very slow, 1 iteration is good enough
    #define TEST_UNROLLED_ITERATIONS 1
    #define TEST_SRC_IS_XIP_FLASH 1
#else
    #define TEST_UNROLLED_ITERATIONS 10
#endif

// Memory buffers
#if MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_FLASH_WITH_CACHE || MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_FLASH_WITHOUT_CACHE
    const uint8_t flashMemoryBuffer[ROW_SIZE_EXTRA + (ROW_COUNT - 1) * SRC_OFFSET_PER_ROW_COPY] = {0};
#endif
#if MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_RAM
    static uint8_t ramMemoryBuffer[ROW_SIZE_EXTRA + (ROW_COUNT - 1) * SRC_OFFSET_PER_ROW_COPY];
#endif

static uint8_t ramDstBuffer[ROW_SIZE_EXTRA + (ROW_COUNT - 1) * DST_OFFSET_PER_ROW_COPY];


void __not_in_flash_func(memcpy_benchmark)(void)
{
    uint32_t t1, t2;
    const char * MEMORY_NAME = "";

    #if MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_ROM
        const uint8_t * src = (const uint8_t * )0x8; // ROM
        MEMORY_NAME = "ROM";
    #elif MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_RAM
        const uint8_t * src = ramMemoryBuffer;
        MEMORY_NAME = "RAM";
    #elif MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_FLASH_WITH_CACHE
        const uint8_t * src = flashMemoryBuffer;
        MEMORY_NAME = "FLASH";
    #elif MEMCPY_BENCHMARK_MEM == MEMCPY_BENCHMARK_MEM_FLASH_WITHOUT_CACHE
        //const uint8_t * src = (uint8_t *)((uint32_t)flashMemoryBuffer + (XIP_NOALLOC_BASE - XIP_MAIN_BASE));
        //const uint8_t * src = (uint8_t *)((uint32_t)flashMemoryBuffer + (XIP_NOCACHE_BASE - XIP_MAIN_BASE));
        const uint8_t * src = (uint8_t *)((uint32_t)flashMemoryBuffer + (XIP_NOCACHE_NOALLOC_BASE - XIP_MAIN_BASE));
        MEMORY_NAME = "FLASH - NO CACHE";
    #else
        #error "MEMCPY TEST CASE NOT IMPLEMENTED"
    #endif
    
    uint8_t * dst = ramDstBuffer;
    float usToCycles = (float)(clock_get_hz(clk_sys) * (1 / (TEST_UNROLLED_ITERATIONS * ROW_COUNT * 1000000.0)));
    
    printf(" %s\n", MEMORY_NAME);

    // Repeat for all sizes, size starts at -1 to print headers with the same logic / order
    // For this reason, size is of type "int32_t" and not "size_t"
    for (int32_t size = -1; size <= ROW_SIZE; size++)
    {
        // Jumps to make the tests faster, for big sizes, test only 8 sizes out of every 100
        if (size == 84)
            size = 99;
        if (size == 108)
            size = 199;
        if (size == 208)
            size = 299;
        if (size == 308)
            size = 399;
        if (size == 408)
            size = 499;
        if (size > ROW_SIZE)
            size = ROW_SIZE;

        if (size >= 0) // Active test row
            printf("%" PRIi32, size);
        else    // Header
            printf("Size"); 

        for (int8_t aligned = 1; aligned >= 0; aligned--) // First test aligned data, then misaligned
        {
            //if (!aligned)
                //asm("bkpt");
            for (size_t srcOffset = 0; srcOffset <= TEST_SRC_OFFSET; srcOffset++)
            {
#if TEST_MISALIGNED
                for (size_t dstOffset = 0; dstOffset <= TEST_DST_OFFSET; dstOffset++)
#else
                size_t dstOffset = srcOffset;
#endif
                {
                    if ((aligned == 1) == (srcOffset == dstOffset)) // If offset combination matches the alignment being tested
                    {
                        if (size >= 0) // Active test row
                        {
                            const uint8_t * s = src + srcOffset;
                            uint8_t * d = dst + dstOffset;
                            uint32_t h = ROW_COUNT;

                            // Make sure no interrupts are enabled
                            uint32_t savedInterruptState = save_and_disable_interrupts();

                            #if TEST_SRC_IS_XIP_FLASH
                                xip_ctrl_hw->flush = 1;
                                xip_ctrl_hw->flush;
                                while (!(xip_ctrl_hw->stat & XIP_STAT_FLUSH_READY_BITS))
                                tight_loop_contents();
                            #endif

                            {   // Hack to force a call to memcpy with size 0 to cache veneer (if any).
                                // To avoid compiler optimization, pointers must not be null & size must not be known (volatile).
                                volatile uint32_t sz = 0;
                                memcpy(d, &h, sz);
                            }

                            t1 = time_us_32();
                            for (; h > 0; h--)
                            {
                                memcpy(d, s, (size_t)size);
                                #if TEST_UNROLLED_ITERATIONS > 1
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                    memcpy(d, s, (size_t)size);
                                #endif
                                d += DST_OFFSET_PER_ROW_COPY;
                                s += SRC_OFFSET_PER_ROW_COPY;
                                //printf("H: %" PRIu32, h);
                                //if (h == 83)
                                //    asm("bkpt");
                            }
                            t2 = time_us_32();
                            restore_interrupts(savedInterruptState);

                            //if (size == 103)
                            //    asm("bkpt");

                            //asm volatile("nop");
                            printf("\t%.1f",  (float)(t2 - t1) * usToCycles);
                        }
                        else // Size < 0, header
                        {
                            printf("\t %zu-%zu", srcOffset, dstOffset); // Space is important for excel, if not used, it changes format to date
                        }
                    }
                }
            }
        }
        //busy_wait_ms(4000);
        printf("\n");
    }
}
