file /code/schmancy_blink.elf

target extended-remote localhost:3333

# Add in the SVD-assisted register dumper
source svd-dump.py

# For the STM32F767

# Add our breakpoint limits
set remote hardware-breakpoint-limit 8
set remote hardware-watchpoint-limit 4

# And load up the relevant SVD
svd_load STMicro STM32F7x7.svd
