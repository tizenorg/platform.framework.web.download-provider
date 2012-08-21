#include <unistd.h>
#include <fcntl.h>

int lock_download_provider_pid(char *path)
{
	int lockfd = 0;
	if ((lockfd = open(path, O_WRONLY | O_CREAT, (0666 & (~000)))) < 0) {
		return -1;
	} else if (lockf(lockfd, F_TLOCK, 0) < 0) {
		close(lockfd);
		return -1;
	}
	return 0;
}
