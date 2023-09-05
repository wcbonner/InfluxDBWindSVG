# CMakeList.txt : CMake project for influxd-wind-svg, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.12.0)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("influxdbwindsvg" 
    VERSION 1.20230905.1 
    DESCRIPTION "Retrieves Apparent Wind Data from Influx Database and creates SVG files" 
    HOMEPAGE_URL https://github.com/wcbonner/InfluxDBWindSVG
)

configure_file(influxdbwindsvg-version.h.in influxdbwindsvg-version.h)

# Add source to this project's executable.
add_executable (influxdbwindsvg "influxd-wind-svg.cpp" "influxd-wind-svg.h" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET influxdbwindsvg PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(influxdbwindsvg PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           ${EXTRA_INCLUDES}
                           )

# TODO: Add tests and install targets if needed.

find_package(InfluxDB REQUIRED)
target_link_libraries(influxdbwindsvg PRIVATE InfluxData::InfluxDB)