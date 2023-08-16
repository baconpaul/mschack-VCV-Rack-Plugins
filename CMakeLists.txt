cmake_minimum_required(VERSION 3.16)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
project(mscHack VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PLUGIN_NAME ${PROJECT_NAME})
set(ADDITIONAL_PLUGIN_DISTRIBUTABLES res LICENSE.txt README.md)
include(RackSDK.cmake)

message(STATUS "${PROJECT_NAME} for Rack Build Process" )
message(STATUS "Installing into '${CMAKE_INSTALL_PREFIX}'")

add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)

target_include_directories(${RACK_PLUGIN_LIB} PRIVATE src src/autogen_airwin/)
target_sources(${RACK_PLUGIN_LIB} PRIVATE
        src/3Ch_Osc.cpp
        src/ARP700.cpp
        src/ASAF8.cpp
        src/Alienz.cpp
        src/Compressor.cpp
        src/Dronez.cpp
        src/Lorenz.cpp
        src/MasterClockx4.cpp
        src/Maude221.cpp
        src/Mixer_16_4_4.cpp
        src/Mixer_24_4_4.cpp
        src/Mixer_4_0_4.cpp
        src/Mixer_9_3_4.cpp
        src/Morze.cpp
        src/OSC_WaveMorph_3.cpp
        src/PingPong.cpp
        src/SEQ_6x32x16.cpp
        src/SEQ_Envelope_8.cpp
        src/Seq_Triad2.cpp
        src/StepDelay.cpp
        src/SynthDrums.cpp
        src/Windz.cpp
        src/mscHack.cpp
        src/mscHack_Control_EnvelopeData.cpp
        src/mscHack_Control_EnvelopeEdit.cpp
        )

target_compile_options(${RACK_PLUGIN_LIB} PUBLIC -Wno-suggest-override -Wno-multichar -Wno-unused-value -Wno-unused-but-set-variable -Wno-unused-variable )

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${RACK_PLUGIN_LIB} PUBLIC -Wno-stringop-truncation)
endif()
