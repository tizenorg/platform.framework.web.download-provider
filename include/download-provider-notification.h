#ifndef DOWNLOAD_PROVIDER_NOTIFICATION_H
#define DOWNLOAD_PROVIDER_NOTIFICATION_H

#include "download-provider-config.h"

int set_downloadinginfo_appfw_notification(download_clientinfo *clientinfo);
int set_downloadedinfo_appfw_notification(download_clientinfo *clientinfo);
int destroy_appfw_notification(download_clientinfo *clientinfo);

#endif
