cmake_minimum_required(VERSION 3.6)
project(plant-sensor)

SET(MCU "atmega168")
SET(F_CPU "8000000")
SET(CMAKE_SYSTEM_NAME Generic)

# For some reason, these paths have to be absolute, otherwise
# CLion won't be able to find headers etc.
SET(CMAKE_C_COMPILER /opt/local/bin/avr-gcc)
SET(CMAKE_CXX_COMPILER /opt/local/bin/avr-g++)

set(CMAKE_CXX_STANDARD 11)

SET(CMAKE_C_FLAGS "-mmcu=${MCU} -DF_CPU=${F_CPU} -Os")
SET(CMAKE_C_LINK_FLAGS "-mmcu=${MCU}")

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(SOURCE_FILES
        Makefile
        main.c
        main.h
        nrf24.c
        nrf24.h
        nRF24L01.h
        radioPinFunctions.c)

add_executable(plant-sensor ${SOURCE_FILES})

