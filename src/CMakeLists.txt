
## PROJECT NAME
PROJECT(download-provider C)
SET(VERSION "0.0.1")

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Debug")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

SET(DATABASE_FILE "/opt/usr/dbspace/.download-provider.db")
SET(IMAGE_DIR "/usr/share/download-provider")

INCLUDE(FindPkgConfig)

pkg_check_modules(dp2_pkgs REQUIRED glib-2.0
		gobject-2.0
		db-util
		sqlite3
		capi-appfw-app-manager
		capi-network-connection
		notification
		appsvc
		dbus-1
		bundle
		dlog)

FOREACH(flag ${dp2_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

## INCLUDES
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/agent/include)

set(DP2_LINK_LIBRARIES ${GLIB-2_LIBRARIES}
		${GOBJECT-2_LIBRARIES}
		pthread
		capi-appfw-app-manager
		downloadagent2
	)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC -D_REENTRANT")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -fpie -Wall")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed -Wl,-pie,--hash-style=both")

ADD_DEFINITIONS(
	-DDATABASE_FILE=\"${DATABASE_FILE}\"
	-DIMAGE_DIR=\"${IMAGE_DIR}\"
	)

ADD_DEFINITIONS(-DDP_SUPPORT_DBUS_ACTIVATION)

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
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(FILES include/download-provider.h DESTINATION include/download-provider/)
