CMAKE_MINIMUM_REQUIRED(VERSION 2.6)


ADD_SUBDIRECTORY(src/agent)
ADD_SUBDIRECTORY(src/)

PROJECT(download-provider C)

CONFIGURE_FILE(download-provider.pc.in download-provider.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/download-provider.pc DESTINATION lib/pkgconfig)

INSTALL(PROGRAMS download-provider-service DESTINATION /etc/rc.d/init.d)

CONFIGURE_FILE(download-provider-service.service.in download-provider-service.service @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/download-provider-service.service DESTINATION /usr/share/dbus-1/services)

# install images
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/res/images/ DESTINATION share/download-provider/
		FILES_MATCHING
		PATTERN "*.png"
		)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.APLv2 share/license/${PROJECT_NAME})
INSTALL(FILES share/license/${PROJECT_NAME} DESTINATION share/license)
