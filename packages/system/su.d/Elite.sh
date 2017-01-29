#!/system/bin/sh

chmod 666 /sys/block/mmcblk0/queue/scheduler
chmod 666 /sys/block/mmcblk0/queue/read_ahead_kb
chmod 666 /sys/class/leds/button-backlight/bln
chmod 666 /sys/class/leds/button-backlight/bln_speed
chmod 666 /sys/class/leds/button-backlight/bln_number
chmod 666 /sys/fpf/fpf
chmod 666 /sys/fpf/vib_strength
chmod 666 /sys/fpf/fpf_dt_wait_period
chmod 666 /sys/module/mdss_fb/parameters/backlight_dimmer
chmod 666 /sys/backlight_dimmer/backlight_min
chmod 666 /sys/class/leds/button-backlight/bln_rgb_batt_colored
chmod 666 /sys/class/leds/button-backlight/bln_rgb_blink_light_level
chmod 666 /sys/class/leds/button-backlight/bln_rgb_pulse

#Set BLN params
#On-1
#off-2
echo "2" > /sys/class/leds/button-backlight/bln

#blink speed 0-9 0=slowest 9=fastest
echo "5" > /sys/class/leds/button-backlight/bln_speed

#number of times to blink 0-50 0=unlimited
echo "20" > /sys/class/leds/button-backlight/bln_number

#Set finger print double tap to wake params
# 1 -> work simple HOME input button (configurable)
# 2 (default) -> work as doubletap sleep without 3rd party apps
# 0 - off, stock behavior
echo "1" > /sys/fpf/fpf

#vib strength
echo "20" > /sys/fpf/vib_strength

#min 0 max 9 , default value without tweak app setting is 2 -> 90msec, 0 = 72msec... 9 = 146msec wait before press is interpreted as single press
echo "2" > /sys/fpf/fpf_dt_wait_period

#Set Backlight Dimmer params
#Enable y/n
echo "N" > /sys/module/mdss_fb/parameters/backlight_dimmer

#Backlight min value
#default 10
echo "10" > /sys/backlight_dimmer/backlight_min

#Set LED params
#enable charge LED coloring
#0-off 1-on
echo "1" > /sys/class/leds/button-backlight/bln_rgb_batt_colored

#led brightness 0-20. Lower is brighter
echo "0" > /sys/class/leds/button-backlight/bln_rgb_blink_light_level

#Pulse Green LED 0-off 1-on
echo "1" > /sys/class/leds/button-backlight/bln_rgb_pulse

#Set IO read ahead
echo "cfq" > /sys/block/mmcblk0/queue/scheduler
echo "1536" > /sys/block/mmcblk0/queue/read_ahead_kb

#Set Fastcharge
# 0 - off 
# 1 - on
echo "0" > /sys/kernel/fast_charge/force_fast_charge

#Set Adreno Boost
# 0 - off
# 1 - smooth/battery 
# 2 - stronger
# 3 - aggressive
echo "1" > /sys/class/kgsl/kgsl-3d0/devfreq/adrenoboost

# Set Wakelocks
 # set to 1 for stock
 # set to 0 to disable
 # higher = less wakelocks
 # echo 0 > /sys/module/bcmdhd/parameters/wlrx_divide
 echo 8 > /sys/module/bcmdhd/parameters/wlrx_divide
 # echo 0 > /sys/module/bcmdhd/parameters/wlctrl_divide
 echo 8 > /sys/module/bcmdhd/parameters/wlctrl_divide

#EAS Schedtune 
echo 2 > /dev/stune/schedtune.boost #global CFS boost
echo 2 > /dev/stune/background/schedtune.boost #background cpuset CFS boost
echo 2 > /dev/stune/foreground/schedtune.boost #foreground cpuset CFS boost
echo 4 > /dev/stune/top-app/schedtune.boost #top-app cpuset CFS boost
echo "sched" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 
echo "sched" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor

chmod 644 /sys/block/mmcblk0/queue/scheduler
chmod 644 /sys/block/mmcblk0/queue/read_ahead_kb
chmod 644 /sys/class/leds/button-backlight/bln
chmod 644 /sys/class/leds/button-backlight/bln_speed
chmod 644 /sys/class/leds/button-backlight/bln_number
chmod 644 /sys/fpf/fpf
chmod 644 /sys/fpf/vib_strength
chmod 644 /sys/fpf/fpf_dt_wait_period
chmod 644 /sys/module/mdss_fb/parameters/backlight_dimmer
chmod 644 /sys/backlight_dimmer/backlight_min
chmod 644 /sys/class/leds/button-backlight/bln_rgb_batt_colored
chmod 644 /sys/class/leds/button-backlight/bln_rgb_blink_light_level
chmod 644 /sys/class/leds/button-backlight/bln_rgb_pulse

exit 0
