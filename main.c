#include "daemon.h"

int main(int argc, const char **argv)
{
	int result;
	pid_t daemonID;
	if (argc > 1) {
		int fd, len;
		pid_t pid;
		char pid_buf[16];
		if ((fd = open(gLockFilePath, O_RDONLY)) < 0)
		{
			perror("Lock file not found. May be the server is not running?");
			exit(fd);
		}
		len = read(fd, pid_buf, 16);
		pid_buf[len] = 0;
		pid = atoi(pid_buf);
		if(!strcmp(argv[1], "stop")) {
			kill(pid, SIGUSR1);
			exit(EXIT_SUCCESS);
		}
		if(!strcmp(argv[1], "restart")) {
			kill(pid, SIGHUP);
			exit(EXIT_SUCCESS);
		}
		printf ("usage %s [stop|restart]\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	if((result = BecomeDaemonProcess(gLockFilePath, "daemon", LOG_DEBUG, &gLockFileDesc, &daemonID))<0) {
		perror("Failed to become daemon process");
		exit(result);
	}

	if((result = ConfigureSignalHandlers())<0) {
		syslog(LOG_LOCAL0|LOG_INFO, "ConfigureSignalHandlers failed, errno=%d", errno);
		unlink(gLockFilePath);
		exit(result);
	}

	if((result = BindPassiveSocket(gaahzdPort, &gMasterSocket))<0) {
		syslog(LOG_LOCAL0|LOG_INFO, "BindPassiveSocket failed, errno=%d", errno);
		unlink(gLockFilePath);
		exit(result);
	}

	do {
		if(AcceptConnections(gMasterSocket)<0) {
			syslog(LOG_LOCAL0|LOG_INFO,"AcceptConnections failed, errno=%d",errno);
			unlink(gLockFilePath);
			exit(result);
		}
		if((gGracefulShutdown==1)&&(gCaughtHupSignal==0))
			break;
		gGracefulShutdown=gCaughtHupSignal=0;
	} while(1);
	TidyUp();
	return 0;	
}
