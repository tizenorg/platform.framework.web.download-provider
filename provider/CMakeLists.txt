
## PROJECT NAME
PROJECT(${PKG_NAME} C)

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Debug")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

INCLUDE(FindPkgConfig)

pkg_check_modules(dp2_pkgs REQUIRED glib-2.0
		gobject-2.0
		sqlite3
		capi-appfw-app-manager
		capi-network-connection
		capi-content-mime-type
		appsvc
		bundle
		libsmack
		dlog
		libsystemd-daemon
		vconf)

FOREACH(flag ${dp2_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

IF(SUPPORT_WIFI_DIRECT)
MESSAGE("WIFI direct:On")
pkg_check_modules(dp2_wifi_direct_pkgs REQUIRED
		capi-network-wifi-direct)
FOREACH(flag ${dp2_wifi_direct_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)
ELSE (SUPPORT_WIFI_DIRECT)
MESSAGE("WIFI direct:Off")
ENDIF(SUPPORT_WIFI_DIRECT)

IF(SUPPORT_SECURITY_PRIVILEGE)
pkg_check_modules(dp2_security_privilege_pkgs REQUIRED security-server)
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${dp2_security_privilege_pkgs_CFLAGS}")
ENDIF(SUPPORT_SECURITY_PRIVILEGE)

IF(SUPPORT_NOTIFICATION)
MESSAGE("Notification:On")
pkg_check_modules(dp2_noti_pkgs REQUIRED
		notification)
FOREACH(flag ${dp2_noti_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)
ADD_DEFINITIONS(-DSUPPORT_NOTIFICATION)
ELSE (SUPPORT_NOTIFICATION)
MESSAGE("Notification:Off")
ENDIF(SUPPORT_NOTIFICATION)

IF(SUPPORT_COMPANION_MODE)
MESSAGE("Companion:On")
pkg_check_modules(dp2_companion_pkgs REQUIRED
			sap-client-stub-api)
FOREACH(flag ${dp2_companion_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)
ADD_DEFINITIONS(-DSUPPORT_COMPANION_MODE)
ELSE (SUPPORT_COMPANION_MODE)
MESSAGE("Companion:Off")
ENDIF(SUPPORT_COMPANION_MODE)

## INCLUDES
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/agent/include)

set(DP2_LINK_LIBRARIES ${GLIB-2_LIBRARIES}
		${GOBJECT-2_LIBRARIES}
		pthread
		capi-appfw-app-manager
	)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIE")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wall -fPIE")

IF(DEFINED IMAGE_DIR)
	ADD_DEFINITIONS(-DIMAGE_DIR=\"${IMAGE_DIR}\")
ENDIF(DEFINED IMAGE_DIR)

IF(DEFINED PROVIDER_DIR)
	ADD_DEFINITIONS(-DPROVIDER_DIR=\"${PROVIDER_DIR}\")
ENDIF(DEFINED PROVIDER_DIR)

IF(DEFINED DATABASE_CLIENT_DIR)
	ADD_DEFINITIONS(-DDATABASE_CLIENT_DIR=\"${DATABASE_CLIENT_DIR}\")
ENDIF(DEFINED DATABASE_CLIENT_DIR)

IF(DEFINED DATABASE_DIR)
	ADD_DEFINITIONS(-DDATABASE_DIR=\"${DATABASE_DIR}\")
ENDIF(DEFINED DATABASE_DIR)

IF(DEFINED LOCALE_DIR)
	ADD_DEFINITIONS(-DPKG_NAME=\"${PKG_NAME}\")
	ADD_DEFINITIONS(-DLOCALE_DIR=\"${LOCALE_DIR}\")
ENDIF(DEFINED LOCALE_DIR)

ADD_DEFINITIONS(-DDOWNLOAD_PROVIDER_LOG_TAG=\"DOWNLOAD_PROVIDER\")

ADD_EXECUTABLE(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-pthread.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-ipc.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-utils.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-notify.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-smack.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-network.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-db.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-plugin-download-agent.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-queue.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-queue-manager.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-client.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-client-manager.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-notification.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-notification-manager.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-main.c )
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${dp2_pkgs_LDFLAGS} ${DP2_LINK_LIBRARIES} ${dp2_noti_pkgs_LDFLAGS} ${dp2_companion_pkgs_LDFLAGS} ${dp2_wifi_direct_pkgs_LDFLAGS} ${dp2_security_privilege_pkgs_LDFLAGS} -ldl -pie)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})

INSTALL(FILES include/download-provider.h DESTINATION ${INCLUDE_INSTALL_DIR}/${PKG_NAME})
