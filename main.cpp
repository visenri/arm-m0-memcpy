#include "lib/memops_opt/memops_opt.h"
#include "memcpy_benchmark.h"

#include <stdio.h>
#include <inttypes.h>
#include <stddef.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"

#if MEMOPS_OPT_TEST
    #include "lib/memops_opt/test/test-memcpy.h"
#endif
#if MEMOPS_OPT_TEST_IMP
    #include "lib/memops_opt/test/memops_opt_test_imp.h"
#else
    #define MEMCPY_ARMV6M_TEST_IMP_COUNT 0
#endif

#ifndef PICO_DEFAULT_LED_PIN
    #warning This program requires a board with an LED
#else
    #define LED_PIN PICO_DEFAULT_LED_PIN
#endif

#if MEMOPS_OPT_TEST_IMP
/* This is the basic CRC-32 calculation with some optimization but no
table lookup. The the byte reversal is avoided by shifting the crc reg
right instead of left and by using a reversed 32-bit word to represent
the polynomial.*/
static uint32_t crc32b(uint8_t *buffer, size_t length) {
   int i, j;
   uint32_t byte, crc, mask;

   i = 0;
   crc = 0xFFFFFFFF;
   while (length--) {
      byte = buffer[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 7; j >= 0; j--) {    // Do eight times.
         mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
      i = i + 1;
   }
   return ~crc;
}

static size_t memcpy_get_implementation_size(void * fn)
{
    size_t size = 0;
    uint8_t value1Count = 0;
    uint8_t value2Count = 0;
    uint8_t * memBytes = (uint8_t*)((uint32_t)fn & 0xFFFFFFFE);
    const uint8_t SIGNATURE_1_SIZE = 8;
    const uint8_t SIGNATURE_2_SIZE = 8;
    
    while (1)
    {
        size++;
        uint8_t value = *memBytes++;
        if (value1Count < SIGNATURE_1_SIZE)
        {
            if (value == 0xFF)
                value1Count++;
            else
                value1Count = 0;
        }
        else
        {
            if (value == 0x00)
            {
                value2Count++;
                if (value2Count >= SIGNATURE_2_SIZE)
                    return size - SIGNATURE_1_SIZE - SIGNATURE_2_SIZE;
            }
            else
            {
                if ((value != 0xFF) || (value2Count > 0))
                {
                    value1Count = 0;
                    value2Count = 0;
                }
            }
        }
    }
}
#endif

#if 0
constexpr uint8_t testConstArray1[] __attribute__((aligned(4))) = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31} ;
volatile uint8_t testArray1[] __attribute__((aligned(4))) = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31} ;
volatile uint8_t testArray2[] __attribute__((aligned(4))) = 
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
     0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
#endif


int main(void)
{
    uint32_t startTime, endTime;
    sleep_ms(150); // >80ms seems to be enough to allow debugger to start the core and restart it afterwards, without executing the application.
    set_sys_clock_khz(clock_get_hz(clk_sys) / 1000, true); // Set actual frequency without changing it, this makes sure we use the default frequency for clk_peri (stdio).

    stdio_init_all();
    //stdio_uart_init();
  
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("\n ARM-M0-Memcpy - Program started!\n");
    printf(" CPU frequency: %" PRIu32 "Hz\n",  clock_get_hz(clk_sys));

    sleep_ms(3000);

    startTime = time_us_32();

    //memcpy_armv6m_test_miwo_1_mssp_2_opxip_1_opsz_1_msup_0((void *)(testArray2 + 0), (void *)(testConstArray1 + 1), 14);
    //const uint8_t * src = (uint8_t *)((uint32_t)testConstArray1 + (XIP_NOCACHE_NOALLOC_BASE - XIP_MAIN_BASE));
    //memcpy_armv6m_test_miwo_1_mssp_2_opxip_1_opsz_1_msup_0((void *)(testArray2 + 0), (void *)(src + 1), 14);
    
    #if MEMOPS_OPT_TEST_IMP
        printf("\nImplementation\tSize\tHash\n");

        for (int i = 0; i < MEMCPY_ARMV6M_TEST_IMP_COUNT; i++)
        {
            uint8_t * fnBytes = (uint8_t*)((uint32_t)MEMCPY_ARMV6M_TEST_IMP_FUNCTIONS[i] & 0xFFFFFFFE);
            size_t size = memcpy_get_implementation_size((void *)fnBytes);
            uint32_t hash = crc32b(fnBytes, size);
            printf("%s\t%zu\t0x%08" PRIX32 "\n", MEMCPY_ARMV6M_TEST_IMP_NAMES[i], size, hash);
        }
    #endif
    

    //asm("bkpt");
    #if MEMOPS_OPT_TEST
        printf("\nTesting implementation: memcpy_armv6m\n");
        test_memcpy(nullptr, &memcpy_armv6m);

        #if MEMOPS_OPT_TEST_IMP
            for (int i = 0; i < MEMCPY_ARMV6M_TEST_IMP_COUNT; i++)
            {
                printf("\nTesting implementation: %s\n", MEMCPY_ARMV6M_TEST_IMP_NAMES[i]);
                test_memcpy(nullptr, MEMCPY_ARMV6M_TEST_IMP_FUNCTIONS[i]);
            }

        #endif
        endTime = time_us_32();
        printf("\nTesting finished in %.2f minutes\n", (endTime - startTime) / (60.0 * 1e6));
        startTime = endTime;
    #endif

    //memcpy_armv6m((void *)(testArray2 + 0), (void *)(testArray1 + 0), 25);
    //memcpy_armv6m((void *)(testArray2 + 0), (void *)(testArray1 + 1), 14);    
    //memcpy_armv6m((void *)(testArray2 + 1), (void *)(testConstArray1 + 0), 14);

    //memcpy_armv6m((void *)(testArray2 + 0), (void *)(testArray1 + 1), 25);
    //memcpy_armv6m((void *)(testArray2 + 1), (void *)(testArray1 + 0), 7); //19

    printf("\nBenchmarking implementations:\n");
    for (int i = -2; i < MEMCPY_ARMV6M_TEST_IMP_COUNT; i++)
    {
        if (i >= 0)
        {
            #if MEMOPS_OPT_TEST_IMP
                printf("\n%s\n", MEMCPY_ARMV6M_TEST_IMP_NAMES[i]);
                memcpy_wrapper_replace(MEMCPY_ARMV6M_TEST_IMP_FUNCTIONS[i]);
            #endif
        }
        else
        {
            if (i == -1)
            {
                memcpy_wrapper_replace(NULL);
                printf("\nmemcpy_armv6m\n");
            }
            else
                printf("\nDefault\n");
        }
        memcpy_benchmark();
    }
    
    endTime = time_us_32();

    printf("\nBenchmarch finished in %.2f minutes\n", (endTime - startTime) / (60.0 * 1e6));

    while(1)
    {
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        sleep_ms(50);
    };
}
