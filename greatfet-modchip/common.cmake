#
# Excercise common code.
#


if(NOT DEFINED ENV{GREATFET_PATH})
  message(FATAL_ERROR "GREATFET_PATH must be defined to point to your GreatFET tree.")
endif()


set(PATH_GREATFET_FIRMWARE $ENV{GREATFET_PATH}/firmware/)
set(CMAKE_TOOLCHAIN_FILE ${PATH_GREATFET_FIRMWARE}/cmake/toolchain-arm-cortex-m.cmake)


