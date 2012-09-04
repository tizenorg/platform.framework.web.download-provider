#ifndef DOWNLOAD_PROVIDER_UTILS_H
#define DOWNLOAD_PROVIDER_UTILS_H

#include "download-provider-config.h"

int get_download_request_id(void);
void clear_clientinfoslot(download_clientinfo_slot *clientinfoslot);
void clear_clientinfo(download_clientinfo *clientinfo);
void clear_socket(download_clientinfo *clientinfo);
int get_downloading_count(download_clientinfo_slot *clientinfo_list);
int get_same_request_slot_index(download_clientinfo_slot *clientinfo_list,
				int requestid);
int get_empty_slot_index(download_clientinfo_slot *clientinfo_list);
int get_pended_slot_index(download_clientinfo_slot *clientinfo_list);
int get_network_status();

#endif
