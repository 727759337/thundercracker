#!/bin/bash
#../../../emulator/tc-siftulator.app/Contents/MacOS/tc-siftulator -n 3 -T &
#../../../emulator/tc-siftulator.app/Contents/MacOS/tc-siftulator -n 3 -f ../../../../firmware/cube/cube.ihx &
../../../emulator/tc-siftulator.app/Contents/MacOS/tc-siftulator -n 5 -T -LR &
sleep 0.1s
../../../firmware/master/master-sim sandwichcraft.elf
#../../../firmware/master/master-sim sandwichcraft.elf --flash_stats
#| grep -v "stamp=0x"  ... grep SYS_43
