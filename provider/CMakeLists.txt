
## PROJECT NAME
PROJECT(${PKG_NAME} C)

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Debug")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

INCLUDE(FindPkgConfig)

pkg_check_modules(dp2_pkgs REQUIRED glib-2.0
		gobject-2.0
		db-util
		sqlite3
		capi-appfw-app-manager
		capi-network-connection
		wifi-direct
		notification
		appsvc
		dbus-1
		bundle
		libtzplatform-config
		dlog)

FOREACH(flag ${dp2_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

## INCLUDES
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/agent/include)

set(DP2_LINK_LIBRARIES ${GLIB-2_LIBRARIES}
		${GOBJECT-2_LIBRARIES}
		pthread
		capi-appfw-app-manager
		downloadagent2
	)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wall")

IF(DEFINED IMAGE_DIR)
	ADD_DEFINITIONS(-DIMAGE_DIR=\"${IMAGE_DIR}\")
ENDIF(DEFINED IMAGE_DIR)

ADD_EXECUTABLE(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-pid.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-socket.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-slots.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-network.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-db.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-request.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-da-interface.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-thread-request.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-thread-queue.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-notification.c
	${CMAKE_CURRENT_SOURCE_DIR}/download-provider-main.c )
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${dp2_pkgs_LDFLAGS} ${DP2_LINK_LIBRARIES})
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})

INSTALL(FILES include/download-provider-defs.h DESTINATION ${INCLUDE_INSTALL_DIR}/${PKG_NAME})
