/******************
	0-stand the successful
	1-stand the server is no onling
	2-stand the args is wrong
	3-stand the fork is error
include two processes (socket.c , test.c)
./test -c 5 -p 123.57.250.247        stand 5 clients connect the 123.57.250.247,the port is 9734 by default
*******************/
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "socket.c"
#define MAXBUF 100
#define PROGRAM_VERSION "0.1"
static const struct option long_options[]= 
{
////	{"time",required_argument,NULL,'t'}, //the time
	{"help",no_argument,NULL,'?'},    //help the information
	{"client",required_argument,NULL,'c'}, //the number of client
	{"version",no_argument,NULL,'V'},   //show the version
	{"proxy",no_argument,NULL,'p'}, //show the host and port
	{NULL,0,NULL,0}
};

static void usage(void)
{
	fprintf(stderr,
	" (like)  ./test -c 5 -p 192.168.1.122\n"
	"  -c|--client                  run the client's number,defult is 10.\n"
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	);
};
int clients = 10; //by defult ,we use the number of clients is 10
int proxyport = 9734;
char proxyhost[MAXHOSTNAMELEN];
static int bench(void);

int main(int argc, char *argv[])
{
	int opt=0;
 	int options_index=0;
	if(argc==1)
 	{
		usage();
		return 2;
 	} 

 	while((opt=getopt_long(argc,argv,"c:t:p:V?",long_options,&options_index))!=EOF )
 	{
  	switch(opt)
  		{
  		case 'c': clients=atoi(optarg);break;  		//the number of client
   		case '?': usage();return 2;break;				
   		case 'V': printf(PROGRAM_VERSION"\n");exit(0); //shown the version 
   		case 'p': 								//show the proxy
  		strcpy(proxyhost,optarg);           //the host ,the proxyport is default0
	  }
	}

    	if(clients==0) clients=10;
//// 	if(benchtime==0) benchtime=60;
 
   	printf("all %d clients\n",clients);
//// 	printf("running %d sec\n", benchtime);
 	if(proxyhost!=NULL)
 		printf("via proxy server %s:%d\n",proxyhost,proxyport);
 	return bench();
}


static int bench(void)
{
	int i;
	pid_t pid=0;
  	for(i=0;i<clients;i++)
  	{
	 	pid=fork();
 		if( pid< (pid_t) 0)
  		{
          		fprintf(stderr,"problems forking worker no. %d\n",i);
	  		perror("fork failed.");
	  		return 3; //the fork is error
  		}
  		if(pid ==  (pid_t) 0)
  		{
 // 			sleep(1);
  			printf("pid is :%d\n",getpid());
  			child_test();
  			return 0;
  		}
  	}
}
 
child_test()
{
	enum{
	START=0,
	LOGGING=1,
	PROCESSING=2,
	HEARTBEAT=3
	};
	int keepflag = 0;
	int sockfd = 0;
	int state , ret;
	int state_logging_c, state_processing_c, state_processing_fail;
	char buffer[MAXBUF] = {0};   //we receive
	char heartbeat[] = {0xF2,0x07,0x05,0x01,0xFF};   //heartbeat's command
	fd_set rfds;
	int len;
	int result;
	struct timeval tv;
	int retval, maxfd = -1;
	struct sockaddr_in address; 
	int login_c(int fd); //log the server
	int process_command(char buffer[],int sockfd);//detail the command
	sockfd = 0;
	state = START;
	while(1){
		switch(state)
			{
			case START:
				sockfd = Socket(proxyhost,proxyport);
				if(sockfd<0) 
				{ 
	   				fprintf(stderr,"\n Connect to server failed. \n");
	   				state = START;
	   				break;
         			}
				state = LOGGING;
				state_logging_c=0;
				break;
			case LOGGING:
				ret = login_c(sockfd);//1 stands for success
				state_logging_c+=1;
				if(ret < 0)//failure
				{
					if(state_logging_c > 10)
						state = START;
					else
						state = LOGGING;
					break;
				}
				printf("LOGIN SUCESS!\n");	
				state = PROCESSING;	
				state_processing_c = 0;
                     state_processing_fail = 0;
				break;

			case PROCESSING:
				state_processing_c++;
				FD_ZERO(&rfds);
				FD_SET(0, &rfds);
				maxfd = 0;
				FD_SET(sockfd, &rfds);
				if (sockfd > maxfd)
					maxfd = sockfd;

				tv.tv_sec = 5;//if server don't send any commnad to us within 5', then we send a hearbeat to the server
				tv.tv_usec = 0;

				retval = select(maxfd+1, &rfds, NULL, NULL, &tv);
				if(retval == -1)
				{
					printf("select is error\n");
					if(state_processing_c >10)
					{
						state = START;
						break;
					}
					else
					{
						state = PROCESSING;
						break;
					}
				}
				else if(retval ==0)
				{ //time is out
					state = HEARTBEAT;
					break;
				}
			//now retval >0 ,we have command to dealwith
				if (FD_ISSET(sockfd, &rfds))
				{
					int len;
					bzero(buffer, MAXBUF+1);
					len = recv(sockfd, buffer, sizeof(buffer),0);
//printf("len = %d\n",len);//show the length that we receive
					if (len > 0)
					{
						keepflag = 0;//处理服务端发过来的命令上下左右
				  		process_command(buffer,sockfd);
					}
					else
					{
						sleep(2);
                    			state_processing_fail++;
                    			if(state_processing_fail > 10) 
                    			{
                        			state=START;
                        			printf("we switch to START state because we cannot receive message from server\n");
                        			break;
                        		}
						printf("Failed to receive the message! \n");
					}
				}
				state = state;//keep state on processing state	
				break;

			case HEARTBEAT:
				len = send(sockfd, heartbeat, /*strlen(heartbeat)*/5, 0);
				if (len < 0)
				{
					printf("failed to send heartbeat !\n");//in this case, we also add keepflag by 1,to switch the state to START
				}
				else			
				{
					printf("keepflag = %d\n",keepflag);
//					printf("heartbeat send!\n");
					keepflag += 1;
					if(keepflag > 10)
					{
						state = START;
						printf("we switch to START state because we sent 10 times heartbeat that but the server not sent us!!!\n");
						keepflag = 0;
                    			break;
					}
				}
			
				state = PROCESSING;
				state_processing_c =0;
                	state_processing_fail =0;
				break;
	
			default:
				state = START;
				break;			
			}//switch
		}//while
}//child_test

int login_c(int fd)
{
	int sockfd = fd;
	char command[40];
	char user[16] = {"board1"};
	char passwd[16] = {"1"};
	char checksum = 0x00;
	int i, ret; 
	sprintf(command, "%c%c%c%-16s%-16s",0xF2,0x06,0x24,user,passwd);
	for( i=0;i<35;i++ )
		checksum += command[i];
	command[35] = checksum;
	command[36] = '\0';
printf("#########################\n");
	ret = send(sockfd, command, strlen(command), 0);
printf("the command length is %d\n",ret);
	recv(sockfd, command, strlen(command), 0);
	if( command[3] == 0x01 )
		return 1;
	else if( command[3] == (char)0xFF )
		printf("Command ERROR!\n");
	else
		printf("The user or passwd is Wrong!\n");
	return -1;
}

int process_command(char buffer[],int sockfd)
{
/*
int i; //show the receive command that from server
for(i=0;i<buffer[2];i++)
{
	printf("%4x",buffer[i]);
}
printf("\n");
*/
	printf("command is 0x%x\n",buffer[1]); //show the command character
	if(buffer[1] == 0x07)
	{
		keepalive();//just print a message
	}
	return 0;
}

int keepalive()
{
	time_t timep;
    	time (&timep);
    	printf("%s", ctime(&timep));
	printf("HA HA HA HA the server is alive\n\n");
}




//this is the socket.c
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int Socket(const char *host, int clientPort)
{
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;
    struct hostent *hp;
    
    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;
    inet_pton(AF_INET,host,&ad.sin_addr);
/*
    inaddr = inet_addr(host);
    if (inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    else
    {
        hp = gethostbyname(host);
        if (hp == NULL)
            return -1;
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }
    */
    ad.sin_port = htons(clientPort);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return sock;
    if (connect(sock, (struct sockaddr *)&ad, sizeof(ad)) < 0)
        return -1;
    return sock;
    printf("sock:%d\n",sock);
}

//this is kill all the process
#!/bin/sh                                                                                                                                                                                                    
ps -aux|grep "./test"|awk '{print $2}'|xargs kill {} \

