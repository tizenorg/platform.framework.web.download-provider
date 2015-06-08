PROJECT(downloadagent2 C)

SET(VERSION "0.1.0")

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
IF("${ARCH}" MATCHES "^arm.*")
	ADD_DEFINITIONS("-D_TARGET")
	SET(CMAKE_C_FLAGS_RELEASE "-mabi=aapcs-linux -msoft-float -O2")
ENDIF("${ARCH}" MATCHES "^arm.*")

#DA Engine Include Directory
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include)

SET(SRCS_PATH ".")
SET(SRCS_DA
        ${SRCS_PATH}/download-agent-interface.c
        ${SRCS_PATH}/download-agent-client-mgr.c
        ${SRCS_PATH}/download-agent-dl-mgr.c
        ${SRCS_PATH}/download-agent-dl-info.c
        ${SRCS_PATH}/download-agent-http-mgr.c
        ${SRCS_PATH}/download-agent-http-msg-handler.c
        ${SRCS_PATH}/download-agent-encoding.c
        ${SRCS_PATH}/download-agent-file.c
        ${SRCS_PATH}/download-agent-plugin-conf.c
        ${SRCS_PATH}/download-agent-mime-util.c
)

SET(HEADERS
	include/download-agent-defs.h
	include/download-agent-interface.h
)

INCLUDE(FindPkgConfig)

MESSAGE("SUPPORT_SYS_RESOURCE: ${SUPPORT_SYS_RESOURCE}")
IF (SUPPORT_SYS_RESOURCE)
pkg_check_modules(subpkgs REQUIRED
	xdgmime
	vconf
	capi-network-connection
	dlog
	system-resource
	storage
)
ELSE (SUPPORT_SYS_RESOURCE)
pkg_check_modules(subpkgs REQUIRED
	xdgmime
	vconf
	capi-network-connection
	dlog
	storage
)
ENDIF (SUPPORT_SYS_RESOURCE)

IF ("${HTTP_LIB}" MATCHES "libcurl")
	MESSAGE("HTTP_LIB: ${HTTP_LIB}")
	pkg_check_modules(httppkgs REQUIRED
		libcurl
	)
	LIST(APPEND SRCS_DA
        ${SRCS_PATH}/download-agent-plugin-libcurl.c
   	)
ENDIF ("${HTTP_LIB}" MATCHES "libcurl")

IF (SUPPORT_OMA_DRM)
	MESSAGE("SUPPORT_OMA_DRM: ${SUPPORT_OMA_DRM}")
	ADD_DEFINITIONS("-D_ENABLE_OMA_DRM")
	LIST(APPEND SRCS_DA
	        ${SRCS_PATH}/download-agent-plugin-drm.c
	)
	pkg_check_modules(drmpkgs REQUIRED
		drm-client
		drm-trusted
	)
ENDIF (SUPPORT_OMA_DRM)

IF (SUPPORT_SYS_RESOURCE)
	ADD_DEFINITIONS("-D_ENABLE_SYS_RESOURCE")
ENDIF (SUPPORT_SYS_RESOURCE)

IF (SUPPORT_DOWNLOAD_BOOSTER)
	MESSAGE("SUPPORT_DOWNLOAD_BOOSTER:${SUPPORT_DOWNLOAD_BOOSTER}")
	ADD_DEFINITIONS("-D_RAF_SUPPORT")
	ADD_DEFINITIONS("-D_DOWNLOAD_BOOSTER_SUPPORT")
ENDIF (SUPPORT_DOWNLOAD_BOOSTER)

FOREACH(flag ${subpkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

FOREACH(flag ${httppkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

IF (SUPPORT_OMA_DRM)
	FOREACH(flag ${drmpkgs_CFLAGS})
		SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
	ENDFOREACH(flag)
	#This is request of DRM Team.
	ADD_DEFINITIONS("-D_FILE_OFFSET_BITS=64")
ENDIF (SUPPORT_OMA_DRM)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -Wall")

ADD_DEFINITIONS("-D_ENABLE_DLOG")
#This should be removed when release a target
ADD_DEFINITIONS("-D_SAMSUNG_MIME_POLICY")

ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRCS_DA})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${subpkgs_LDFLAGS} ${httppkgs_LDFLAGS} ${drmpkgs_LDFLAGS})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SOVERSION ${VERSION})

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_INSTALL_DIR} COMPONENT RuntimeLibraries)
