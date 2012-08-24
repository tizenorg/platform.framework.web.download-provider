ADD_SUBDIRECTORY(src/agent)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

## PROJECT NAME
PROJECT(download-provider C)
SET(VERSION "0.0.1")

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Debug")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

SET(DATABASE_DIR "/opt/dbspace")
SET(DATABASE_NAME ".download-provider.db")
SET(RES_DIR "/opt/data/download-provider")
SET(IMAGE_DIR "${RES_DIR}")

INCLUDE(FindPkgConfig)

pkg_check_modules(pkgs REQUIRED glib-2.0
		gobject-2.0
		db-util
		sqlite3
		capi-appfw-application
		capi-network-connection
		bundle
		dlog)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

## INCLUDES
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/agent/include)

set(LINK_LIBRARIES ${GLIB-2_LIBRARIES}
		${GOBJECT-2_LIBRARIES}
		pthread
		capi-appfw-application
		downloadagent
	)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -fpie -Wall")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed -Wl -fpie,--hash-style=both")

ADD_DEFINITIONS(
	-DDATABASE_DIR=\"${DATABASE_DIR}\"
	-DDATABASE_NAME=\"${DATABASE_NAME}\"
	-DIMAGE_DIR=\"${IMAGE_DIR}\"
	)

ADD_EXECUTABLE(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/src/download-provider-pid.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/download-provider-ipc.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/download-provider-notification.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/download-provider-db.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/download-provider-utils.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/download-provider-receiver.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/download-provider-main.c )
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} ${LINK_LIBRARIES})
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

CONFIGURE_FILE(download-provider.pc.in download-provider.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/download-provider.pc DESTINATION lib/pkgconfig)
INSTALL(FILES include/download-provider.h DESTINATION include/${PROJECT_NAME})

INSTALL(PROGRAMS download-provider-service DESTINATION /etc/rc.d/init.d)

# install images
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/res/images/ DESTINATION /opt/data/download-provider/
		FILES_MATCHING
		PATTERN "*.png"
		)
