PROJECT(downloadagent2 C)

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Debug")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(VERSION "0.0.1")
FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")

#DA Engine Include Directory
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include)

INCLUDE(FindPkgConfig)
pkg_check_modules(subpkgs REQUIRED
	libsoup-2.4
	xdgmime
	vconf
	capi-network-connection
	glib-2.0
	dlog
	libtzplatform-config
)

FOREACH(flag ${subpkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -Wall")

IF("${ARCH}" MATCHES "^arm.*")
	ADD_DEFINITIONS("-D_TARGET")
	SET(CMAKE_C_FLAGS_RELEASE "-mabi=aapcs-linux -msoft-float -O2")
ENDIF("${ARCH}" MATCHES "^arm.*")

ADD_DEFINITIONS("-D_EFL_PLATFORM")
#allow to install widget, deb pkg and apk for testing
ADD_DEFINITIONS("-DDA_DEBUG_USING_DLOG")
#This should be removed when release a target
ADD_DEFINITIONS("-D_SAMSUNG_MIME_POLICY")

#############################################################################
#+++++++++++++++++++++++++DA ENGINE+++++++++++++++++++++++++++++++++++++++++++
#############################################################################

SET(SRCS_PATH ".")
SET(SRCS_DA_ENGINE
        ${SRCS_PATH}/download-agent-debug.c
        ${SRCS_PATH}/download-agent-interface.c
        ${SRCS_PATH}/download-agent-client-mgr.c
        ${SRCS_PATH}/download-agent-dl-mgr.c
        ${SRCS_PATH}/download-agent-dl-info-util.c
        ${SRCS_PATH}/download-agent-http-queue.c
        ${SRCS_PATH}/download-agent-http-misc.c
        ${SRCS_PATH}/download-agent-http-mgr.c
        ${SRCS_PATH}/download-agent-http-msg-handler.c
        ${SRCS_PATH}/download-agent-encoding.c
        ${SRCS_PATH}/download-agent-utils.c
        ${SRCS_PATH}/download-agent-utils-dl-id-history.c
        ${SRCS_PATH}/download-agent-basic.c
        ${SRCS_PATH}/download-agent-file.c
        ${SRCS_PATH}/download-agent-plugin-libsoup.c
        ${SRCS_PATH}/download-agent-plugin-conf.c
        ${SRCS_PATH}/download-agent-mime-util.c
)

SET(HEADERS
	include/download-agent-defs.h
	include/download-agent-interface.h
)

ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRCS_DA_ENGINE})
#TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${subpkgs_LDFLAGS} "-ldl")
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${subpkgs_LDFLAGS})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SOVERSION 0.0.1)

#############################################################################
#+++++++++++++++++++++++++INSTALLATION++++++++++++++++++++++++++++++++++++++++
#############################################################################

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_INSTALL_DIR} COMPONENT RuntimeLibraries)

