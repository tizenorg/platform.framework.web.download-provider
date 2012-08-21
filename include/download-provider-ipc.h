#ifndef DOWNLOAD_PROVIDER_IPC_H
#define DOWNLOAD_PROVIDER_IPC_H

#include "download-provider-config.h"

int ipc_receive_header(int fd);
int ipc_send_stateinfo(download_clientinfo *clientinfo);
int ipc_send_downloadinfo(download_clientinfo *clientinfo);
int ipc_send_downloadinginfo(download_clientinfo *clientinfo);
int ipc_send_request_stateinfo(download_clientinfo *clientinfo);
int ipc_receive_request_msg(download_clientinfo *clientinfo);

#endif
