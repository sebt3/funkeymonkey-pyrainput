CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(funkeymonkey-pyrainput)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include_directories(include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(pyrainput SHARED pyrainput.cpp)
install(TARGETS pyrainput DESTINATION lib/funkeymonkey)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD "systemd")
if (SYSTEMD_FOUND AND "${SYSTEMD_SERVICES_INSTALL_DIR}" STREQUAL "")
	execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
		--variable=systemdsystemunitdir systemd
		OUTPUT_VARIABLE SYSTEMD_SERVICES_INSTALL_DIR)
	string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SERVICES_INSTALL_DIR
		"${SYSTEMD_SERVICES_INSTALL_DIR}")
endif()

if (SYSTEMD_FOUND)
	configure_file(
		"${PROJECT_SOURCE_DIR}/pyrainput.service.in"
		"${PROJECT_BINARY_DIR}/pyrainput.service"
		@ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pyrainput.service"
		DESTINATION "${SYSTEMD_SERVICES_INSTALL_DIR}")
endif (SYSTEMD_FOUND)
install(FILES "${PROJECT_SOURCE_DIR}/pyrainputctl" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ WORLD_READ DESTINATION sbin)
