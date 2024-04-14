@echo off
if %1 == 1 (

echo DEBUG_OPT: ENABLE DEBUGGING
echo RUN_TO_ENTRY_ID=main > options.txt
echo CONTINUE_ID=monitor rp2040.core1 arp_reset assert 0 ; rp2040.core0 arp_reset assert 0 ; reset halt >> options.txt
rem echo CONTINUE_ID= >> options.txt

) else (

echo DEBUG_OPT: ENABLE RUN
echo RUN_TO_ENTRY_ID= > options.txt
echo CONTINUE_ID=continue >> options.txt

)