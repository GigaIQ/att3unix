#include "daemon.h"

int BecomeDaemonProcess(const char *lockFileName, const char *logPrefix, const int logLevel, int *lockFileDesc, int  *thisPID)
{
	int	curPID, stdioFD, lockResult, killResult, lockFD, i, numFiles;
	char pidBuf[17],*lfs,pidStr[7];
	FILE *lfp;
	unsigned long lockPID;
	struct flock exclusiveLock;

	chdir("/");

	lockFD=open(lockFileName,O_RDWR|O_CREAT|O_EXCL,0644);
	
	if(lockFD==-1)
	{
		lfp=fopen(lockFileName,"r");

		if(lfp==0) 
		{
			perror("Can't get lockfile");
			return -1;
		}

		lfs=fgets(pidBuf,16,lfp);

		if(lfs!=0)
		{
			if(pidBuf[strlen(pidBuf)-1]=='\n') 
				pidBuf[strlen(pidBuf)-1]=0;
			
			lockPID=strtoul(pidBuf,(char**)0,10);
			
			killResult=kill(lockPID,0);
			
			if(killResult==0)
			{
				printf("\n\nERROR\n\nA lock file %s has been detected. It appears it is owned\nby the (active) process with PID %ld.\n\n",lockFileName,lockPID);
			}
			else
			{
				if(errno==ESRCH) 
				{
					printf("\n\nERROR\n\nA lock file %s has been detected. It appears it is owned\nby the process with PID %ld, which is now defunct. Delete the lock file\nand try again.\n\n",lockFileName,lockPID);
				}
				else
				{
					perror("Could not acquire exclusive lock on lock file");
				}
			}
		}
		else
			perror("Could not read lock file");

		fclose(lfp);
		
		return -1;
	}

	exclusiveLock.l_type=F_WRLCK; 
	exclusiveLock.l_whence=SEEK_SET; 
	exclusiveLock.l_len=exclusiveLock.l_start=0; 
	exclusiveLock.l_pid=0; 
	lockResult=fcntl(lockFD,F_SETLK,&exclusiveLock);
	
	if(lockResult<0) 
	{
		close(lockFD);
		perror("Can't get lockfile");
		return -1;
	}

	curPID=fork();

	switch(curPID)
	{
		case 0: 
		  break;

		case -1: 
		  fprintf(stderr,"Error: initial fork failed: %s\n",
					 strerror(errno));
		  return -1;
		  break;

		default: 
		  exit(0);
		  break;
	}

	if(setsid()<0)
		return -1;
	
	signal(SIGHUP,SIG_IGN);

	curPID=fork();

	switch(curPID) 
	{
		case 0:
		  break;

		case -1:
		  return -1;
		  break;

		default:
		  exit(0);
		  break;
	}

	
	if(ftruncate(lockFD,0)<0)
		return -1;

	sprintf(pidStr,"%d\n",(int)getpid());
	
	write(lockFD,pidStr,strlen(pidStr));

	*lockFileDesc=lockFD; 
	

	numFiles=sysconf(_SC_OPEN_MAX); 
	
	if(numFiles<0) 
		numFiles=256; 
		
	for(i = numFiles - 1; i >= 0; --i) 
	{
		if(i!=lockFD) 
			close(i);
	}
	
	umask(0); 

	stdioFD=open("/dev/null",O_RDWR); 
	dup(stdioFD); 
	dup(stdioFD); 

	openlog(logPrefix,LOG_PID|LOG_CONS|LOG_NDELAY|LOG_NOWAIT,LOG_LOCAL0);

	(void)setlogmask(LOG_UPTO(logLevel)); 
	setpgrp();

	return 0;
}

int ConfigureSignalHandlers(void)
{
	struct sigaction sighupSA,sigusr1SA,sigtermSA;
	
	signal(SIGUSR2,SIG_IGN);	
	signal(SIGPIPE,SIG_IGN);
	signal(SIGALRM,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTTOU,SIG_IGN);
	signal(SIGURG,SIG_IGN);
	signal(SIGXCPU,SIG_IGN);
	signal(SIGXFSZ,SIG_IGN);
	signal(SIGVTALRM,SIG_IGN);
	signal(SIGPROF,SIG_IGN);
	signal(SIGIO,SIG_IGN);
	signal(SIGCHLD,SIG_IGN);

	signal(SIGQUIT,FatalSigHandler);
	signal(SIGILL,FatalSigHandler);
	signal(SIGTRAP,FatalSigHandler);
	signal(SIGABRT,FatalSigHandler);
	signal(SIGIOT,FatalSigHandler);
	signal(SIGBUS,FatalSigHandler);
	signal(SIGFPE,FatalSigHandler);
	signal(SIGSEGV,FatalSigHandler);
	signal(SIGSTKFLT,FatalSigHandler);
	signal(SIGCONT,FatalSigHandler);
	signal(SIGPWR,FatalSigHandler);
	signal(SIGSYS,FatalSigHandler);
	
	sigtermSA.sa_handler=TermHandler;
	sigemptyset(&sigtermSA.sa_mask);
	sigtermSA.sa_flags=0;
	sigaction(SIGTERM,&sigtermSA,NULL);
		
	sigusr1SA.sa_handler=Usr1Handler;
	sigemptyset(&sigusr1SA.sa_mask);
	sigusr1SA.sa_flags=0;
	sigaction(SIGUSR1,&sigusr1SA,NULL);
	
	sighupSA.sa_handler=HupHandler;
	sigemptyset(&sighupSA.sa_mask);
	sighupSA.sa_flags=0;
	sigaction(SIGHUP,&sighupSA,NULL);
	
	return 0;
}

int BindPassiveSocket(const int portNum, int *boundSocket)
{
	struct sockaddr_in sin;
	int newsock, optval;
	size_t optlen;
	memset(&sin.sin_zero, 0, 8);
	sin.sin_port = htons(portNum);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	if((newsock= socket(PF_INET, SOCK_STREAM, 0))<0)
		return -1;
	optval = 1;
	optlen = sizeof(int);
	setsockopt(newsock, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
	if(bind(newsock, (struct sockaddr*) &sin, sizeof(struct sockaddr_in))<0)
	return -1;
	if(listen(newsock,SOMAXCONN)<0)
	return -1;
	*boundSocket = newsock;
	return 0;
}

int AcceptConnections(const int master)
{
	int proceed = 1, slave, retval = 0;
	struct sockaddr_in client;
	socklen_t clilen;
	while((proceed==1)&&(gGracefulShutdown==0)) {
		clilen = sizeof(client);
		slave = accept(master,(struct sockaddr *)&client,&clilen);
		if(slave<0) { /* ошибка accept() */
			if(errno == EINTR)
				continue;
			syslog(LOG_LOCAL0|LOG_INFO,"accept() failed: %m\n");
			proceed = 0;
		retval = -1;
		}
		else
		{
			retval = HandleConnection(slave);
			if(retval)
				proceed = 0;
		}
		close(slave);
	}
	return retval;
}

int HandleConnection(const int slave)
{
	char readbuf[1025];
	size_t bytesRead;
	const size_t buflen=1024;
	int retval;
	retval = ReadLine(slave, readbuf, buflen, &bytesRead);
	if(retval==0)
		WriteToSocket(slave, readbuf, bytesRead);
	return retval;
}

int WriteToSocket(const int sock, const char *const buffer, const size_t buflen)
{
	size_t bytesWritten=0;
	ssize_t	writeResult;
	int	retval=0,done=0;

	do
	{
		writeResult = send(sock,buffer+bytesWritten,buflen-bytesWritten,0);
		if(writeResult == -1)
		{
			if(errno == EINTR)
				writeResult=0;
			else
			{
				retval=1;
				done=1;
			}
		}
		else
		{
			bytesWritten+=writeResult;
			if(writeResult==0)
				done=1;
		}
	}while(done==0);

	return retval;
}

int ReadLine(const int sock,char *const buffer,const size_t buflen, size_t *const bytesRead)
{
	int	done=0,retval=0;
	char c,lastC=0;
	size_t bytesSoFar=0;
	ssize_t	readResult;
	
	do
	{
		readResult=recv(sock,&c,1,0);
		
		switch(readResult)
		{
			case -1:
			  if(errno!=EINTR)
			  {
				  retval=-1;
				  done=1;
			  }
			  break;
			case 0:
			  retval=0;
			  done=1;
			  break;
			case 1:
			  buffer[bytesSoFar]=c;
			  bytesSoFar+=readResult;
			  if(bytesSoFar>=buflen)
			  {
				  done=1;
				  retval=-1;
			  }
			  if((c=='\n')&&(lastC=='\r'))
				  done=1;
			  lastC=c;
			  break;
		}
	}while(!done);
	buffer[bytesSoFar]=0;
	*bytesRead=bytesSoFar;
	
	return retval;
}

void TidyUp(void)
{
	if(gLockFileDesc!=-1) {
		close(gLockFileDesc);
		unlink(gLockFilePath);
		gLockFileDesc=-1;
	}
	if(gMasterSocket!=-1) {
		close(gMasterSocket);
		gMasterSocket=-1;
	}
}

void FatalSigHandler(int sig) 
{
#ifdef _GNU_SOURCE
	syslog(LOG_LOCAL0|LOG_INFO,"caught signal: %s – exiting",strsignal(sig));
#else
	syslog(LOG_LOCAL0|LOG_INFO,"caught signal: %d – exiting",sig);
#endif
	closelog();
	TidyUp();
	_exit(0);
}

void TermHandler(int sig)
{
	TidyUp();
	_exit(0);
}

void HupHandler(int sig)
{
	syslog(LOG_LOCAL0|LOG_INFO,"caught SIGHUP");
	gGracefulShutdown=1;
	gCaughtHupSignal=1;
	return;
}

void Usr1Handler(int sig)
{
	syslog(LOG_LOCAL0|LOG_INFO,"caught SIGUSR1 - soft shutdown");
	gGracefulShutdown=1;

	return;
}
