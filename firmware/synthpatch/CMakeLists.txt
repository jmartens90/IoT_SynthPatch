# Desktop build of the synth engine (no ESP32 needed).
# Open this file in Qt Creator (File > Open File or Project), or:
#   cmake -B build && cmake --build build && ./build/tests && ./build/demo
cmake_minimum_required(VERSION 3.16)
project(synthpatch_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The engine is a library: the same files are compiled into the ESP32 firmware later.
add_library(engine
  lib/engine/Modulator.cpp
  lib/engine/Lfo.cpp
  lib/engine/Envelope.cpp
  lib/engine/Engine.cpp
  lib/engine/PitchTracker.cpp
  lib/engine/Sequencer.cpp
)
target_include_directories(engine PUBLIC lib/engine)
if(MSVC)
  target_compile_options(engine PRIVATE /W4)
else()
  target_compile_options(engine PRIVATE -Wall -Wextra)
endif()

add_executable(demo desktop/demo.cpp)
target_link_libraries(demo engine)

add_executable(tests desktop/tests.cpp)
target_link_libraries(tests engine)

enable_testing()
add_test(NAME engine_tests COMMAND tests)
