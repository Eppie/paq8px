cmake_minimum_required(VERSION 3.7)
project(paq8px)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_EXE_LINKER_FLAGS  ${CMAKE_EXE_LINKER_FLAGS} "-lz")

add_definitions(-DUNIX -DNOASM -Ofast -march=native -g -Weverything)

file(GLOB SOURCE_FILES main.cpp)
add_executable(paq8px ${SOURCE_FILES} Buf.h Array.h utils.h String.h user_interface.h Encoder.h Predictor.h APM1.h Stretch.h models/models.h Ilog.h models/jpegmodel.h ContextMap.h Mixer.h StateMap.h StateTable.h BH.h filters.h models/im8bitModel.h models/im1bitModel.h models/im24bitModel.h models/recordModel.h models/wavModel.h models/sparseModel.h models/distanceModel.h models/wordModel.h models/indirectModel.h models/dmcModel.h models/nestModel.h models/exeModel.h models/contextModel2.h)
