cmake_minimum_required(VERSION 3.14)
project(heathrow-flights CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/rpi-rgb-led-matrix/CMakeLists.txt")
  message(FATAL_ERROR
    "rpi-rgb-led-matrix not found. Run: ./scripts/setup.sh")
endif()

add_subdirectory(third_party/rpi-rgb-led-matrix)

add_executable(heathrow-flights
  src/main.cpp
  src/flight.cpp
  src/flight_api.cpp
  src/display.cpp
)

target_include_directories(heathrow-flights PRIVATE
  src
  third_party/json/include
)

target_link_libraries(heathrow-flights PRIVATE
  rgbmatrix
  CURL::libcurl
)

install(TARGETS heathrow-flights DESTINATION bin)
