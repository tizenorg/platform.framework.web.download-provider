#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include <pthread.h>

#include "download-provider-config.h"
#include "download-provider-log.h"

GMainLoop *gMainLoop = 0;	// need for libsoup, decided the life-time by mainloop.

int lock_download_provider_pid(char *path);
void *run_manage_download_server(void *args);

void TerminateDaemon(int signo)
{
	TRACE_DEBUG_INFO_MSG("Received SIGTERM");
	if (g_main_loop_is_running(gMainLoop))
		g_main_loop_quit(gMainLoop);
}

static gboolean CreateThreadFunc(void *data)
{
	pthread_t thread_pid;
	pthread_attr_t thread_attr;
	if (pthread_attr_init(&thread_attr) != 0) {
		TRACE_DEBUG_MSG("failed to init pthread attr");
		TerminateDaemon(SIGTERM);
		return FALSE;
	}
	if (pthread_attr_setdetachstate(&thread_attr,
									PTHREAD_CREATE_DETACHED) != 0) {
		TRACE_DEBUG_MSG("failed to set detach option");
		TerminateDaemon(SIGTERM);
		return 0;
	}
	// create thread for receiving the client request.
	if (pthread_create
		(&thread_pid, &thread_attr, run_manage_download_server,
		data) != 0) {
		TRACE_DEBUG_MSG
			("failed to create pthread for run_manage_download_server");
		TerminateDaemon(SIGTERM);
	}
	return FALSE;
}

int main()
{
	if (chdir("/") < 0) {
		TRACE_DEBUG_MSG("failed to call setsid or chdir");
		exit(EXIT_FAILURE);
	}
#if 0
	// close all console I/O
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
#endif

	if (signal(SIGTERM, TerminateDaemon) == SIG_ERR) {
		TRACE_DEBUG_MSG("failed to register signal callback");
		exit(EXIT_FAILURE);
	}
	// write IPC_FD_PATH. and lock
	if (lock_download_provider_pid(DOWNLOAD_PROVIDER_LOCK_PID) < 0) {
		TRACE_DEBUG_MSG
			("It need to check download-provider is already alive");
		TRACE_DEBUG_MSG("Or fail to create pid file in (%s)",
				DOWNLOAD_PROVIDER_LOCK_PID);
		exit(EXIT_FAILURE);
	}
	// if exit socket file, delete it
	if (access(DOWNLOAD_PROVIDER_IPC, F_OK) == 0) {
		unlink(DOWNLOAD_PROVIDER_IPC);
	}
	// libsoup need mainloop.

	gMainLoop = g_main_loop_new(NULL, 0);

	g_type_init();

	g_idle_add(CreateThreadFunc, gMainLoop);

	g_main_loop_run(gMainLoop);

	TRACE_DEBUG_INFO_MSG("Download-Provider will be terminated.");

	// if exit socket file, delete it
	if (access(DOWNLOAD_PROVIDER_IPC, F_OK) == 0) {
		unlink(DOWNLOAD_PROVIDER_IPC);
	}
	// delete pid file
	if (access(DOWNLOAD_PROVIDER_LOCK_PID, F_OK) == 0) {
		unlink(DOWNLOAD_PROVIDER_LOCK_PID);
	}
	exit(EXIT_SUCCESS);
}
