cmake_minimum_required(VERSION 3.7)
project(paq8px)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_EXE_LINKER_FLAGS  ${CMAKE_EXE_LINKER_FLAGS} "-lz")

add_definitions(-DUNIX -DNOASM -Ofast -march=native -g -Weverything)

file(GLOB SOURCE_FILES main.cpp)
add_executable(paq8px ${SOURCE_FILES} Buf.h Array.h utils.h String.h user_interface.h Encoder.h Predictor.h APM1.h Stretch.h models.h Ilog.h jpegmodel.h ContextMap.h Mixer.h StateMap.h StateTable.h BH.h filters.h)
