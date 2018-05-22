#
# This file is part of GreatFET
#

cmake_minimum_required(VERSION 2.8.9)

if(NOT DEFINED ENV{GREATFET_PATH})
  message(FATAL_ERROR "GREATFET_PATH must be defined to point to your GreatFET tree.")
endif()

set(PATH_GREATFET_FIRMWARE $ENV{GREATFET_PATH}/firmware/)
set(CMAKE_TOOLCHAIN_FILE ${PATH_GREATFET_FIRMWARE}/cmake/toolchain-arm-cortex-m.cmake)
project(greatfet-rcm-modchip)

include(${PATH_GREATFET_FIRMWARE}/cmake/greatfet-common.cmake)
set(SRC_M4
	main.c
	usb_host_stack.c
	"${PATH_GREATFET_FIRMWARE}/common/usb.c"
	"${PATH_GREATFET_FIRMWARE}/common/usb_request.c"
	"${PATH_GREATFET_FIRMWARE}/common/usb_standard_request.c"
	"${PATH_GREATFET_FIRMWARE}/common/gpdma.c"
	"${PATH_GREATFET_FIRMWARE}/common/gpio_dma.c"
	"${PATH_GREATFET_FIRMWARE}/common/gpio_scu.c"
	"${PATH_GREATFET_FIRMWARE}/common/usb_host.c"
	"${PATH_GREATFET_FIRMWARE}/common/usb_queue_host.c"
	"${PATH_GREATFET_FIRMWARE}/common/glitchkit.c"
	"${PATH_GREATFET_FIRMWARE}/common/usb_queue.c"
	"${PATH_GREATFET_FIRMWARE}/common/fault_handler.c"
	"${PATH_GREATFET_FIRMWARE}/common/rom_iap.c"
	"${PATH_GREATFET_FIRMWARE}/common/spi_bus.c"
	"${PATH_GREATFET_FIRMWARE}/common/debug.c"
)

DeclareTargets()
