cmake_minimum_required(VERSION 3.16)
project(cpu_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binaries into build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(cpu-sim
  src/main.cpp
  src/trace_loader.cpp
  src/pipeline.cpp       
  src/hazard.cpp  
  src/predictor.cpp 
  src/predictor_factory.cpp   
)

# Tell the target where to find headers
target_include_directories(cpu-sim PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Warnings
if (MSVC)
  target_compile_options(cpu-sim PRIVATE /W4 /permissive-)
else()
  target_compile_options(cpu-sim PRIVATE -Wall -Wextra -Wpedantic)
endif()
