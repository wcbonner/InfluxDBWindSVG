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
    VERSION 2.20231205.2
    DESCRIPTION "Retrieves Environment Data from Influx Database and creates SVG files"
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

install(TARGETS influxdbwindsvg
    DESTINATION bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(FILES "influxdbwindsvg.service"
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
    COMPONENT "influxdbwindsvg"
)

# Going through https://cmake.org/cmake/help/latest/guide/tutorial/index.html#guide:CMake%20Tutorial

#enable_testing()
#add_test(NAME Runs COMMAND influxdbwindsvg --help)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "wcbonner@users.noreply.github.com")
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE ${CMAKE_PROJECT_VERSION_PATCH})
set(CPACK_DEBIAN_PACKAGE_DEPENDS curl,influxdb)
set(CPACK_DEBIAN_PACKAGE_SECTION custom)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst" "${CMAKE_CURRENT_SOURCE_DIR}/prerm" "${CMAKE_CURRENT_SOURCE_DIR}/postrm")
include(CPack)
