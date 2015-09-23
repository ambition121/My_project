#include <sys/types.h>     
#include <sys/socket.h>  
#include <stdio.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <unistd.h>  
#include <stdlib.h>  
#include <string.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/time.h>
#include <resolv.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

#define MAXBUF 1024


int login_c(int fd);

int main(int argc,char *argv[])  
{  
	int sockfd;  
	int len;  
	struct sockaddr_in address;     
	int result;  
	int i,byte;  
	char char_send[100] = { 0 };  
	
	char buffer[MAXBUF];
	char heartbeat[] = {0xF2,0x07,0x05,0x01,0xFF};
	fd_set rfds;
	struct timeval tv;
	int retval, maxfd = -1;


	if(argc != 3) {  
		printf("Usage: fileclient <address> <port>/n");
		return 0;  
	}  
	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1) {  
		perror("sock");  
		exit(1);  
	}  
	bzero(&address,sizeof(address));  
	address.sin_family = AF_INET;  
	address.sin_port = htons(atoi(argv[2]));  
	inet_pton(AF_INET,argv[1],&address.sin_addr);  
	len = sizeof(address);  
   
	if((result = connect(sockfd, (struct sockaddr *)&address, len))==-1) {  
		perror("connect");  
		exit(EXIT_FAILURE);  
	}
	
	/*****************************OMAP登录验证**************************************/
	int choice;
	int ret;
	int flag = 1;
	char buffer2[10] = {0};



	while(flag) {
		printf("****************Welcome***************\n");
		printf("*\t\t1.LOGIN\t\t     *\n");
		printf("*\t\t2.EXIT\t\t     *\n");
		printf("**************************************\n");
		printf("Please choose: \n");

		ret = login_c(sockfd);
		if(ret == 1)
			flag = 0;
		break;
		
       /*
		scanf("%d",&choice);		
		switch(choice)
		{
			case 1:
				ret = login_c(sockfd);
				if(ret == 1)
					flag = 0;
				break;
			case 2:
				ret = exit_c(sockfd);
				if( ret == 1 ) {
					close(sockfd);
					exit(0);
				}
				break;
		}
*/
	}	



	/*******************************************************************************/
	//add the heartbeat and deal the command!!!!!!
	printf("LOGIN SUCESS!\n");
	
	int keepflag = 0;
	while(keepflag < 20)//send 20 heartbeat if client doesn't receive server's command  
	{  
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		maxfd = 0;

		FD_SET(sockfd, &rfds);
		if (sockfd > maxfd)
			maxfd = sockfd;

		tv.tv_sec = 30;//if server don't send any commnad to us within 30s, then we send a hearbeat to the server
		tv.tv_usec = 0;

		retval = select(maxfd+1, &rfds, NULL, NULL, &tv);
		if (retval == -1)
		{
			printf("select is error!\n");
			break;
		}
		else if (retval == 0)//timeout,then we send a heartbeat
		{
			len = send(sockfd, heartbeat, strlen(heartbeat), 0);
			if (len < 0)
			{
				printf("failed to send heartbeat !\n");
				break;
			}
			else
			{
				keepflag += 1;
				printf("%d\n",keepflag);
				printf("heartbeat send!\n");
			}
			continue;
		}			
		else
		{
			if (FD_ISSET(sockfd, &rfds))
			{
				bzero(buffer, MAXBUF+1);
				len = recv(sockfd, buffer, MAXBUF, 0);

int i;
for(i=0;i<buffer[2];i++)
printf("%4x",buffer[i]);
printf("\n");


				if (len > 0)
				{
					keepflag = 0;

//					if(checksum_check(buffer,strlen(buffer)) != 0){
//						printf("checksum error\n");
//						continue;
//					}


				//处理服务端发过来的命令上下左右	
					if(buffer[1] == 0x01){
						switch(buffer[3]){
							case 0x01:
								up(sockfd);
								usleep(300000);
								stop(sockfd);
								break;
							case 0x02:
								down(sockfd);
								usleep(300000);
								stop(sockfd);
								break;
							case 0x03:
								left(sockfd);
								usleep(300000);
								stop(sockfd);	
								break;
							case 0x04:
								right(sockfd);
								usleep(300000);
								stop(sockfd);
								break;
						}
					}
				
				//变倍长 变倍短
					if(buffer[1] == 0x02){
						switch(buffer[3]){
							case 0x01:
								zoomlong(sockfd);//变倍长
								usleep(300000);
								stop(sockfd);
								break;
							case 0x02:
								zoomshort(sockfd);//变倍短
								usleep(300000);
								stop(sockfd);
								break;
						}
					}
				
				//处理聚焦近，远
					if(buffer[1] == 0x03){
						switch(buffer[3]){
							case 0x01:
								focusclose(sockfd);
								usleep(300000);
								stop(sockfd);
								break;
							case 0x02:
								focusfar(sockfd);
								usleep(300000);
								stop(sockfd);
								break;
						}	
					}

				//stop the camera's up down left right and so on
					if(buffer[1] == 0x05){
						stop(sockfd);
					}

				//add the keepalive
					if(buffer[1] == 0x07){
						keepalive();
					}

	
					switch(buffer[1])
					{
						case 0x08:
							start_stream(sockfd);
							break;
						case 0x09:
							stop_stream(sockfd);
							break;
					}


			}


			else
			{
				if (len < 0)
					printf("Failed to receive the message! \n");
				else
					printf("Chat to terminate!\n");
					break;
				}
			}
		    
			if (FD_ISSET(0, &rfds))
			{
				bzero(buffer, MAXBUF+1);
				fgets(buffer, MAXBUF, stdin);

				if (!strncasecmp(buffer, "exit", 4))
				{
					send(sockfd, buffer, strlen(buffer)-1, 0);
					printf("Own request to terminate the chat!\n");
					break;
				}

				len = send(sockfd, buffer, strlen(buffer)-1, 0);
				if (len < 0)
				{
					printf("Message failed to send ! \n");
					break;
				}
				else
				{	if(len != 0)
						printf("Send Mesg: %s ",buffer);
					keepflag = 0;
				}
			}	
		}
	}  
	close(sockfd);  
	exit(0);  
} 


//推视频流
void *operation(void *arg);
int start_stream(int fd)
{
   	pthread_t tid;
	int err;
	err = pthread_create(&tid,NULL,operation,(void *)fd);
    	if(err != 0)
        printf("error creat");	
}

//void * operation(void *arg)
void * operation(void * arg)
{
	int sockfd = (int)arg;
	char command[] = {0xF2,0x08,0x05,0xF1,0xF0};//faild
	char command1[] = {0xF2,0x08,0x05,0x01,0x00};//success
	int status;
//	status = system(" sudo gst-launch gstrtpbin name=rtpbin latency=50 v4l2src device=/dev/video3  sensor_name=\"adv7280m_b 2-0020\"  inputnum=7 brightnessex=30 framerate=30 name=v4l2src1 saturationex=20 ! video/x-raw-yuv,width=640,height=480,framerate=30/1 ! queue ! ducatih264enc ! queue ! rtph264pay pt=96 ! rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 ! udpsink name=udpsink1 host=112.74.89.131 port=8554 async=true  rtpbin.send_rtcp_src_0 ! queue2 ! udpsink name=udpsink2 host=112.74.89.131 port=8555 async=true udpsrc port=5005 ! rtpbin.recv_rtcp_sink_0 ");

	status = system(" ./start_stream& ");

	printf("status:%d\n",status);

	if(status == -1){
	send(sockfd,command,sizeof(command),0);
	printf("system error:%s\n",strerror(errno));
				}
	else{
		
int i;						//test sent the message to server
		i=send(sockfd,command1,5,0);
printf("%d\n",i);

		printf("run command successful\n");	
	}
/*
	else{
		if(WIFEXITED(status))
		{
			send(sockfd,command1,strlen(command1)+1,0);
			if(WEXITSTATUS(status) == 0)
				printf("run command successful\n");
			else
				printf("run command fail and exit code is %d\n",WEXITSTATUS(status));
		}
		else
			printf("exit status = %d\n",WEXITSTATUS(status));
	}
	return 0;
*/
}





//停止推视频流
int stop_stream(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x09,0x05,0xF1,0xF1};//faild
	char command1[] = {0xF2,0x09,0x05,0x01,0x01};//success
	int err;
	err = system(" ./stop_stream ");
	if((err == -1)){
		send(sockfd,command,sizeof(command),0);
		printf("system stop stream error\n");
	}
	else{
		send(sockfd,command1,sizeof(command1),0);
		printf("stop stream successful\n");
	}


	return 0;

}


int up(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x01,0x05,0xF1,0xE9};//faild
	char command1[] = {0xF2,0x01,0x05,0x01,0xF9};//success
        int err;
        err = system(" ./write1 up ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		send(sockfd,command1,sizeof(command1),0);
                printf("up ok\n");
        }
        return 0;
}


int down(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x01,0x05,0xF2,0xEA};//faild
	char command1[] = {0xF2,0x01,0x05,0x02,0xFA};//success
        int err;
        err = system(" ./write1 down ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		send(sockfd,command1,sizeof(command1),0);
                printf("down ok\n");
        }
        return 0;
}

int left(int fd)
{
int ret;
	int sockfd = fd;
        int err;
	char command[] = {0xF2,0x01,0x05,0xF3,0xEB};
	char command1[] = {0xF2,0x01,0x05,0x03,0xFB};
        err = system(" ./write1 left ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		ret = send(sockfd,command1,sizeof(command1),0);
for(ret=0;ret<command1[2];ret++)
	printf("%4x",command1[ret]);
printf("\n");
		printf("ret:%d\n",ret);
                printf("left ok\n");
        }
        return 0;
}


int right(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x01,0x05,0xF4,0xEC};
	char command1[] = {0xF2,0x01,0x05,0x04,0xFC};
        int err;
        err = system(" ./write1 right ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		send(sockfd,command1,sizeof(command1),0);
                printf("right ok\n");
        }
        return 0;
}


int zoomshort(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x02,0x05,0xF2,0xEB};//faild
	char command1[] = {0xF2,0x02,0x05,0x02,0xFB};//success
        int err;
        err = system(" ./write1 zoomshort ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		send(sockfd,command1,sizeof(command1),0);
                printf("zoomshort ok\n");
        }
        return 0;
}


int zoomlong(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x02,0x05,0xF1,0xEA};//faild
	char command1[] = {0xF2,0x02,0x05,0x01,0xFA};//success
        int err;
        err = system(" ./write1 zoomlong ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		send(sockfd,command1,sizeof(command1),0);
                printf("zoomlong ok\n");
        }
        return 0;
}


int focusclose(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x03,0x05,0xF1,0xEB};//faild
	char command1[] = {0xF2,0x03,0x05,0x01,0xFB};//success
        int err;
        err = system(" ./write1 focusclose ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		send(sockfd,command1,sizeof(command1),0);
                printf("focusclose ok\n");
        }
        return 0;
}


int focusfar(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x03,0x05,0xF2,0xEC};//faild
	char command1[] = {0xF2,0x03,0x05,0x02,0xFC};//success	
	int err;
	err = system(" ./write1 focusfar ");
	if(err == -1)
	{
		send(sockfd,command,sizeof(command),0);
		printf("system error\n");
	}
	else
	{
		send(sockfd,command1,sizeof(command1),0);
		printf("focusfar ok\n");
	}
	return 0;
}

int stop(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x05,0x05,0xF1,0xED};//faild
	char command1[] = {0xF2,0x05,0x05,0x01,0xFD};//success
	int err;
	err = system(" ./write1 stop ");
	if(err == -1){
		send(sockfd,command,sizeof(command),0);
		printf("system error\n");
	}
	else{
		send(sockfd,command1,sizeof(command1),0);
		printf("stop ok\n");
	}
	return 0;
}


//check the keepalive
int keepalive()
{
	printf("HA HA HAHthe server is alive\n");
}



//检查校验位

int checksum_check(char buffer[],int n)
{
    char checksum;
    int i;
    for( i=0;i<n-1;i++ ){
        checksum += buffer[i];
	}
    if( checksum == buffer[n-1] )
        return 0;
    else 
        return -1; 
}



//OMAP登录

int login_c(int fd)
{
	int sockfd = fd;
	char command[40];
//	char user[16] = {0};
//	char passwd[16] = {0};
	char user[16] = {"123"};
	char passwd[16] = {"456"};

	char checksum = 0x00;
	int i; 
	
	/*
	printf("Please Input User name:\n ");
//	scanf("%s",user);
	send(sockfd,user,sizeof(user),0);

	printf("Please Input Passwd: ");
//	scanf("%s",passwd);
	send(sockfd,passwd,sizeof(passwd),0);
	*/

//	sprintf(command, "%c%c%c%-16s%-16s",0xF1,0x01,0x24,user,passwd);//use the debug	
	sprintf(command, "%c%c%c%-16s%-16s",0xF2,0x06,0x24,user,passwd);

	for( i=0;i<35;i++ )
		checksum += command[i];
	command[35] = checksum;
//	command[35] = checksum + 0x01;
	command[36] = '\0';

	send(sockfd, command, strlen(command), 0);

	recv(sockfd, command, strlen(command), 0);
	if( command[3] == 0x01 )
		return 1;
	else if( command[3] == (char)0xFF )
		printf("Command ERROR!\n");
	else
		printf("The user or passwd is Wrong!\n");
	return 0;
}



int exit_c(int fd) 
{
	int sockfd = fd; 
	char command[5] = {0};

	sprintf(command, "%c%c%c%c%c",0xF1,0x03,0x05,0x01,0xFA);
//	sprintf(command, "%c%c%c%c%c",0xF1,0x03,0x05,0x01,0xF9);
	send(sockfd, command, 5, 0);
	
	recv(sockfd, command, strlen(command), 0);
	if( command[3] == 0x01 )
		return 1;
	else if( command[3] == (char)0xFF )
		printf("Command ERROR!\n");
	return 0;
}

