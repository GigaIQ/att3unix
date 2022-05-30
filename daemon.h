#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

static volatile sig_atomic_t gGracefulShutdown=0;
static volatile sig_atomic_t gCaughtHupSignal=0;
static int gLockFileDesc=-1;
static int gMasterSocket=-1;
static const int gaahzdPort=50555;
static const char *const gLockFilePath = "/var/run/daemon.pid";


void TidyUp(void);
void FatalSigHandler(int sig);
void TermHandler(int sig);
void HupHandler(int sig);
void Usr1Handler(int sig);
int BindPassiveSocket(const int portNum, int *const boundSocket);
int BecomeDaemonProcess(const char *lockFileName, const char *logPrefix, const int logLevel, int *lockFileDesc, int *thisPID);
int ConfigureSignalHandlers(void);
int AcceptConnections(const int master);
int HandleConnection(const int slave);
int WriteToSocket(const int sock,const char *const buffer, const size_t buflen);
int ReadLine(const int sock,char *const buffer,const size_t buflen, size_t *const bytesRead);

#endif
