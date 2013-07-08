CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

MESSAGE("PROJECT : ${PKG_NAME}")

# DEFINES

SET(VERSION ${PKG_VERSION}-${PKG_RELEASE})
MESSAGE("VERSION : ${VERSION}")

IF(DEFINED SUPPORT_DBUS_SYSTEM)
	MESSAGE("SUPPORT DBUS SYSTEM : ${SUPPORT_DBUS_SYSTEM}")
	IF(SUPPORT_DBUS_SYSTEM)
		ADD_DEFINITIONS(-DDP_SUPPORT_DBUS_ACTIVATION)
	ENDIF(SUPPORT_DBUS_SYSTEM)
ENDIF(DEFINED SUPPORT_DBUS_SYSTEM)

IF(DEFINED SUPPORT_WIFI_DIRECT)
	MESSAGE("SUPPORT_WIFI_DIRECT: ${SUPPORT_WIFI_DIRECT}")
	IF(SUPPORT_WIFI_DIRECT)
		ADD_DEFINITIONS(-DSUPPORT_WIFI_DIRECT)
	ENDIF(SUPPORT_WIFI_DIRECT)
ENDIF(DEFINED SUPPORT_WIFI_DIRECT)

IF(DEFINED SUPPORT_LOG_MESSAGE)
	MESSAGE("SUPPORT_LOG_MESSAGE: ${SUPPORT_LOG_MESSAGE}")
	IF(SUPPORT_LOG_MESSAGE)
		ADD_DEFINITIONS(-DSUPPORT_LOG_MESSAGE)
	ENDIF(SUPPORT_LOG_MESSAGE)
ENDIF(DEFINED SUPPORT_LOG_MESSAGE)

IF(DEFINED SUPPORT_CHECK_IPC)
	MESSAGE("SUPPORT_CHECK_IPC: ${SUPPORT_CHECK_IPC}")
	IF(SUPPORT_CHECK_IPC)
		ADD_DEFINITIONS(-DSUPPORT_CHECK_IPC)
	ENDIF(SUPPORT_CHECK_IPC)
ENDIF(DEFINED SUPPORT_CHECK_IPC)

# BUILD

ADD_SUBDIRECTORY(agent)
ADD_SUBDIRECTORY(provider-interface)
ADD_SUBDIRECTORY(provider)

# INSTALL

PROJECT(${PKG_NAME} C)
SET(PACKAGE_DESCRIPTION "Defines for ${PROJECT_NAME}")

CONFIGURE_FILE(download-provider.pc.in download-provider.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/download-provider.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

INSTALL(PROGRAMS download-provider-service DESTINATION /etc/rc.d/init.d)

# install images
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/res/images/ DESTINATION ${IMAGE_DIR}
		FILES_MATCHING
		PATTERN "*.png"
		)

IF(DEFINED DBUS_SERVICE_DIR)
	CONFIGURE_FILE(org.download-provider.service.in org.download-provider.service @ONLY)
	INSTALL(FILES org.download-provider.service DESTINATION ${DBUS_SERVICE_DIR})
ENDIF(DEFINED DBUS_SERVICE_DIR)

IF(DEFINED LICENSE_DIR)
	CONFIGURE_FILE(LICENSE.APLv2 share/license/${PROJECT_NAME})
	INSTALL(FILES share/license/${PROJECT_NAME} DESTINATION ${LICENSE_DIR})
ENDIF(DEFINED LICENSE_DIR)