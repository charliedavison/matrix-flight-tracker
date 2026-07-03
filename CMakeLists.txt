cmake_minimum_required(VERSION 3.14)
project(heathrow-flights CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

set(RGBMATRIX_DIR "${CMAKE_SOURCE_DIR}/third_party/rpi-rgb-led-matrix")
set(RGBMATRIX_LIB "${RGBMATRIX_DIR}/lib/librgbmatrix.a")

if(NOT EXISTS "${RGBMATRIX_DIR}/lib/Makefile")
  message(FATAL_ERROR
    "rpi-rgb-led-matrix not found. Run: ./scripts/setup.sh")
endif()

# Build via the upstream Makefile — their CMakeLists.txt uses CMAKE_SOURCE_DIR
# and breaks when included as a subdirectory.
add_custom_command(
  OUTPUT ${RGBMATRIX_LIB}
  COMMAND ${CMAKE_MAKE_PROGRAM} -C ${RGBMATRIX_DIR}/lib
  COMMENT "Building rpi-rgb-led-matrix"
  VERBATIM
)

add_custom_target(rgbmatrix_build DEPENDS ${RGBMATRIX_LIB})

add_library(rgbmatrix STATIC IMPORTED GLOBAL)
set_target_properties(rgbmatrix PROPERTIES IMPORTED_LOCATION ${RGBMATRIX_LIB})

add_executable(heathrow-flights
  src/main.cpp
  src/flight.cpp
  src/flight_api.cpp
  src/display.cpp
)

add_dependencies(heathrow-flights rgbmatrix_build)

target_include_directories(heathrow-flights PRIVATE
  src
  third_party/json/include
  ${RGBMATRIX_DIR}/include
)

target_link_libraries(heathrow-flights PRIVATE
  rgbmatrix
  CURL::libcurl
  pthread
  rt
  m
)

install(TARGETS heathrow-flights DESTINATION bin)
