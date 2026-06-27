cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(spider)

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE
	src/main.c
	src/kame_servo_cal.c
	src/kame_pose.c
	src/kame_motion.c
	src/spider_servo.c
	src/spider_control.c
	src/ble_remote.c
)
