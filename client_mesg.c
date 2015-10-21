//this process add the mode that send the message,we add the stat machine that receive message
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
#include <error.h> //
#include <time.h>
#include <pthread.h>

#define MAXBUF 100

enum{
	START=0,
	LOGGING=1,
	PROCESSING=2,
	HEARTBEAT=3
};


int login_c(int fd);
int process_command(char buffer[],int sockfd);

int main(int argc,char *argv[])  
{
   
	int sockfd=0;  
	int len;  
	struct sockaddr_in address;     
	int result;  
	int i,byte;  
	char char_send[100] = { 0 };  
	int keepflag = 0;
	int choice;
	int ret;
	int flag = 1;
	int state;
	int state_logging_c,state_processing_c,state_processing_fail;
	char buffer[MAXBUF] = {0};
	char heartbeat[] = {0xF2,0x07,0x05,0x01,0xFF};
	fd_set rfds;
	struct timeval tv;
	int retval, maxfd = -1;

	if(argc != 3) {  
		printf("Usage: fileclient <address> <port>/n");
		return 0;  
	}
	
	sockfd = 0;
	state = START;
	while(1)
	{
		switch(state){
			case START:
				if(sockfd !=0)
					close(sockfd);
					
				if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1) {  
					perror("sock");  
					state = START;
					break;
				}
printf("*******************sockfd:%d\n",sockfd) ; 
				bzero(&address,sizeof(address));  
				address.sin_family = AF_INET;  
				address.sin_port = htons(atoi(argv[2]));  
				inet_pton(AF_INET,argv[1],&address.sin_addr);  
				len = sizeof(address);  
   
				if((result = connect(sockfd, (struct sockaddr *)&address, len))==-1) {  
					sleep(2);
					perror("connect");  
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

			sendmesg(sockfd); //send the message to 10010///////////////////

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

			tv.tv_sec = 5;//if server don't send any commnad to us within 5s, then we send a hearbeat to the server
			tv.tv_usec = 0;

			retval = select(maxfd+1, &rfds, NULL, NULL, &tv);
			if(retval == -1){
				printf("select is error\n");
				if(state_processing_c >10)
					state = START;
				else
					state = PROCESSING;
				break;
			}
			else if(retval ==0){
				state = HEARTBEAT;
				break;
			}
			//now retval >0 ,we have command to dealwith
			if (FD_ISSET(sockfd, &rfds))
			{
				bzero(buffer, MAXBUF+1);
		//		len = recv(sockfd, buffer, MAXBUF, 0);
				len = recv(sockfd, buffer, sizeof(buffer),0);
printf("len = %d\n",len);

//detail the command the command come together
				if(len > 0){
					keepflag = 0;
					if(len % buffer[2] == 0 && buffer[0] == (char)0xF1) //1.more than 1 command together
					{	
						int i,j;
						j = len / buffer[2];
						for(i = 0;i<j;i++)
						{
							buffer[1] = buffer[1+buffer[2] * i];
							buffer[3] = buffer[3+buffer[2] * i];
							process_command(buffer,sockfd);
						}	
					}
					else if(len % buffer[2] != 0 && buffer[0] == (char)0xF1) //2.more than 1 command together buf the last is not the command
					{
						int i,j;
						j = len / buffer[2];
						for(i = 0;i<j;i++)
						{
							buffer[1] = buffer[1+buffer[2] * i];
							buffer[3] = buffer[3+buffer[2] * i];
							process_command(buffer,sockfd);
						}
					}
					else if(len % 5 == 0 && buffer[0] != (char)0xF1) //3.
					{
						int i,j,k;
						j = len / 5;
						for(i=0;i<5;i++)
						{
							if(buffer[i] == (char)0xF1)
							{
								for(k=0;k<j-1;k++)
								{
								buffer[1] = buffer[i+1+5*k];
								buffer[3] = buffer[i+3+5*k];
								process_command(buffer,sockfd);
								}
							}
						}
					}
					else if(len % 5 != 0 && buffer[0] != (char)0xF1) //4.
					{
						int i,j,k;
						j = len / 5;
						i = len % 5;
						for(k=0;k<j;k++)
						{
							buffer[1] = buffer[i+1+5*k];
							buffer[3] = buffer[i+3+5*k];
							process_command(buffer,sockfd);
						}
					}
				}

/*
				if (len > 0)
				{
					keepflag = 0;
				//处理服务端发过来的命令上下左右	
				  process_command(buffer,sockfd);
				}
*/
				else
				{
					sleep(3);
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
		    
			if (FD_ISSET(0, &rfds))//0 represents for stdin
			{
				bzero(buffer, MAXBUF+1);
				fgets(buffer, MAXBUF, stdin);

				if (!strncasecmp(buffer, "exit", 4))
				{
					send(sockfd, buffer, strlen(buffer)-1, 0);
					printf("Own request to terminate the chat!\n");
					
				}

				len = send(sockfd, buffer, strlen(buffer)-1, 0);
				if (len < 0)
				{
					printf("Message failed to send ! \n");
					
				}
				else
				{	
					if(len != 0)
						printf("Send Mesg from stdin: %s ",buffer);
					
				}
			}	
		
			state = state;//keep state on processing state	
			break;
	
		case HEARTBEAT:
			len = send(sockfd, heartbeat, strlen(heartbeat), 0);
			if (len < 0)
			{
				printf("failed to send heartbeat !\n");//in this case, we also add keepflag by 1,to switch the state to START
			}
			else			
			{
				printf("%d\n",keepflag);
				printf("error = %d\n",errno);//use the debug
				printf("heartbeat send!\n");
				keepflag += 1;
				if(keepflag > 10){
					state = START;
					printf("we switch to START state because we sent 20 times heartbeat that but the server not sent us\n");
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
		}//switch state machine
	}//while(1)
		
	exit(0);  
} 


//推视频流
void *operation(void *arg);
int start_stream(int fd)
{
   	pthread_t tid;
		int err;
		err = pthread_create(&tid,NULL,operation,(void *)&fd);//at 10.12
    if(err != 0)
       printf("error creat");	
}

void * operation(void * arg)
{
	int sockfd = *((int*)arg);//at 10.12
	char command[] = {0xF2,0x08,0x05,0xF1,0xF0};//faild
	char command1[] = {0xF2,0x08,0x05,0x01,0x00};//success
	int status;
printf("*****************sockfd:%d\n",sockfd);
	system(" /etc/init.d/rsyslog stop ");
	status = system(" /home/media/work/start_stream.sh & ");
	printf("status:%d\n",status);

	if(status == -1){
		send(sockfd,command,sizeof(command),0);
		printf("system error:%s\n",strerror(errno));
	}
	else{
		
		send(sockfd,command1,5,0);
		system(" /home/media/work/gpio1.sh ");
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
	err = system(" /home/media/work/stop_stream.sh ");
	if((err == -1)){
		send(sockfd,command,sizeof(command),0);
		printf("system stop stream error\n");
	}
/*	else{
		send(sockfd,command1,sizeof(command1),0);
		system(" /home/media/work/gpio2.sh ");
		printf("stop stream successful\n");
	}
*/
	else{
		if(WIFEXITED(err)){
			if(0 == WEXITSTATUS(err)){
				printf("RUN the stop stream successful!\n");
				send(sockfd,command1,sizeof(command1),0);
				system(" /home/media/work/gpio2.sh ");
			}
			else{
				printf("RUN the stop stream faild! code is:\n",WEXITSTATUS(err));
				send(sockfd,command,sizeof(command),0);
			}
		}
		else{
			printf("exit status = [%d]\n", WEXITSTATUS(err));
			send(sockfd,command,sizeof(command),0);
		}
	}
	return 0;
}


int up(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x01,0x05,0xF1,0xE9};//faild
	char command1[] = {0xF2,0x01,0x05,0x01,0xF9};//success
        int err;
        err = system(" /home/media/work/write1 up ");
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
        err = system(" /home/media/work/write1 down ");
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
        err = system(" /home/media/work/write1 left ");
        if(err == -1){
		send(sockfd,command,sizeof(command),0);
                printf("system error\n");
        }
        else{
		send(sockfd,command1,sizeof(command1),0);
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
        err = system(" /home/media/work/write1 right ");
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
        err = system(" /home/media/work/write1 zoomshort ");
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
        err = system(" /home/media/work/write1 zoomlong ");
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
        err = system(" /home/media/work/write1 focusclose ");
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
	err = system(" /home/media/work/write1 focusfar ");
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
	err = system(" /home/media/work/write1 stop ");
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
	time_t timep;
    	time (&timep);
    	printf("%s", ctime(&timep));

	printf("HA HA HA HA the server is alive\n");
////////////	system(" /home/media/work/gpio.sh ");
}


//OMAP登录

//retuns:
//1: success
//-1 :failure
int login_c(int fd)
{
	int sockfd = fd;
	char command[40];
//	char user[16] = {0};
//	char passwd[16] = {0};
	char user[16] = {"board2"};
	char passwd[16] = {"1"};

	char checksum = 0x00;
	int i; 

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
	return -1;
}




/*
Name:process_command()
Description:
process various commands sent from the server
*/
int process_command(char buffer[],int sockfd)
{

int i; //show the receive command that from server
for(i=0;i<buffer[2];i++)
{
	printf("%4x",buffer[i]);
}
printf("\n");
                    printf("command is 0x0%x\n",buffer[1]); //show the command character
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
					}         //变倍长 变倍短
					else if(buffer[1] == 0x02){
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
					}//处理聚焦近，远
					else if(buffer[1] == 0x03){
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
					}//stop the camera's up down left right and so on
					else if(buffer[1] == 0x05){
						stop(sockfd);
					}//add the keepalive
					else if(buffer[1] == 0x07){
						keepalive();//just print a message
					}
					else if (buffer[1] == 0x08){
						start_stream(sockfd);
					}
					else if (buffer[1] == 0x09){
						stop_stream(sockfd);
					}
					
					return 0;
}

/*
   use the stat machine to wait the message,in 30',if we receive the message,we read and process the message 
   to send to the server
   if we not receive the machine,we send the CXLL to 10010 to check the stream
*/
//send the message
void *send_gsm(void *arg);
int sendmesg(int fd)
{
printf("***********************fd:%d\n",fd);
        pthread_t tid;
        int err;
        err = pthread_create(&tid,NULL,send_gsm,(void *)fd);//at 10.12
		if(err != 0)
		printf("error creat");   
}

#define BUFFER_SIZE 2048
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void * send_gsm(void * arg)
{
	enum{
		CONFIRM=0,
	    STYLE=1,
		SENDNUM=2,
		DETAIL=3,
		CHECK=4,
		PROCESS=5,
		DELETE=6,
		OPEN=7
	};
		int stat;
        int sockfd = (int)arg;//at 10.12
        int portfd;
        int nwrite, nread;
printf("****************sockfd:%d\n",sockfd);
        char buff[BUFFER_SIZE];
        char buffer1[20] = {'A','T','\r'};  //confirm the command
        char buffer2[20] = {'A','T','+','C','M','G','F','=','1','\r'}; //send the message's style,1->text,0->PDU
        char buffer3[20] = {'A','T','+','C','M','G','S','=','"','1','0','0','1','0','"','\r'};//send the message
        char buffer4[20] = {'C','X','L','L','\r',0x1A,'\r'};//send the message that check the stream
        char buffer5[20] = {'A','T','+','C','M','G','L','=','"','A','L','L','"','\r'}; //read the whole message
        char buffer6[20] = {'A','T','+','C','M','G','D','=','1',',','4','\r'}; //delete all the message
		int state_logging_c,state_processing_c,state_processing_fail;
		portfd = 0;
		stat = OPEN;
	while(1)
	{
		switch(stat){
			case OPEN:    //open the seriport 
				if(portfd != 0)
					close(portfd);
				if((portfd = open("/dev/ttyUSB1",O_RDWR|O_NOCTTY|O_NDELAY)) < 0) //open the serial port
				{
					perror("open_port");
					stat = OPEN;
					break;
				}
				printf("open port ok!\n");
					stat = CONFIRM;
					break;

			case CONFIRM:  //AT confirm the seriport 
				nwrite = write(portfd,buffer1,strlen(buffer1));
					printf("write the confirm of message len is:%d\n",nwrite);
					printf("buffer1 is:%s\n",buffer1);
				sleep(2);
				nread = read(portfd, buff, BUFFER_SIZE);
					printf("receive the buff Len is:%d\n", nread);
				if(buff[2] == 'O' && buff[3] == 'K'){
					printf("confirm ok!!\n");
					stat = STYLE;
					break;
				}
				else{
					printf("confirm is wrong!!\n");
					stat = OPEN;
					break;
				}

			case STYLE:   //the message's mode 1->text,0->PDU
				nwrite = write(portfd,buffer2,strlen(buffer2)); //send the mode of message
					printf("write the mode of message len is:%d\n",nwrite);
					printf("buffer2 is:%s\n",buffer2);
				sleep(2);
				nread = read(portfd, buff, BUFFER_SIZE);
					printf("receive the buff Len is:%d\n", nread);
				if(buff[2] == 'O' && buff[3] == 'K'){
					printf("mode ok!!\n");
					stat = CHECK;
					break;
				}
				else{
					printf("mode is wrong!!\n");
					stat = OPEN;
					break;
				}

			case CHECK:  //AT+CMGL="ALL",check the whether the SIM have message
				nwrite = write(portfd,buffer5,strlen(buffer5));  //check the SIM have or no message
					printf("write the index that you want look the message is:%d\n",nwrite);
					printf("buffer5 is:%s\n",buffer5);
				sleep(2);
				nread = read(portfd, buff, BUFFER_SIZE);
				if(buff[2] == 'O' && buff[3] == 'K'){   //the SIM have no message that we send 10010 to receive message
					stat = SENDNUM;
					break;
				}
				else{
					stat = PROCESS;
					break;
				}

			case SENDNUM:  //AT+CMGS="10010",send to 10010
				nwrite = write(portfd,buffer3,strlen(buffer3)); //send the number                                                                                                                       
					printf("write the number len is:%d\n",nwrite);
					printf("buffer3 is:%s\n",buffer3);
				sleep(2);
					stat = DETAIL;
					break;

			case DETAIL: //CXLL 0x1A we can check the stream
				nwrite = write(portfd,buffer4,strlen(buffer4)); //send the command that check the stream
					printf("write the command len is:%d\n",nwrite);
					printf("buffer4 is:%s\n",buffer4);
				sleep(60);
					stat = CHECK;
					break;
			
			case PROCESS: //we process the string
					printf("receive the buff Len is:%d\n", nread);
					printf("buff is:%s\n",buff); //printf the message that receive GSM
				char *string = NULL;
				char buffer[10];
				char comm[15];
				char checksum = 0x00;
				char *ptr;
				char *strbegin = "52694F596D4191CF0020",*strend = "004D0042000D000A0020672C6570";
				int beginindex,endindex,beginstrlength = strlen(strbegin);
				ptr = strstr(buff, strbegin);
					beginindex = ptr-buff-1;
					ptr = strstr(buff, strend);
					endindex = ptr-buff;
				int n = endindex - beginindex - beginstrlength;
				if(n>0){
					string=(char*)malloc((n)*sizeof(char));
					strncpy(string, buff+beginindex+beginstrlength+1, n-1); 
					string[n-1]='\0';
					printf("string:%s\n", string); //打印剩余流量的字符串

                int len = strlen(string); ///// 
					printf("%d\n",len); //打印剩余流量字符串长度
                int i,j=0;
                for(i=0;i<len;i++){ //处理剩余流量字符串string,string是收到的剩余流量的字符串,buffer是处理后的字符串
					if((i+1)%4 == 0){ 
						if(string[i] != 'E' ){
							buffer[j]=string[i];
							j++;
						}   
						else{
							buffer[j] = '.';
							j++;
						}  
					   buffer[len / 4] = '\0';	
					}   
				}   
                printf("buffer is:%s\n",buffer);//打印处理后的字符串，显示剩余流量
                free(string);
				sprintf(comm,"%c%c%c%-12s",0xF2,0x10,0x0F,buffer);
				for( i=0;i<14;i++ )
					checksum += comm[i];
				comm[14] = checksum;
				comm[15] = '\0';
				int length = send(sockfd, comm, strlen(comm), 0);//SENT THE BUFFER TO THE SERVER
					printf("sent the last flow Length:%d\n",length);
            }
				sleep(60);
//				stat = DELETE;
				stat = CHECK;
				break;
/*
		case DELETE: //AT+CMGD=1,4 ,delete all message
			nwrite = write(portfd,buffer6,strlen(buffer6));  //delete all the message
				printf("write the delete the all message's length is:%d\n",nwrite);
				printf("buffer6 is:%s\n",buffer6);
            sleep(2);
            nread = read(portfd, buff, BUFFER_SIZE);
				printf("receive the buff Len is:%d\n", nread);
            if(buff[2] == 'O' && buff[3] == 'K'){
                printf("delete the ALL the message is ok!!\n");
				sleep(1800); //wait half an hour to check
				stat = CHECK;
				break;
            }
            else{
                printf("delete message is wrong!!\n");
                stat = DELETE;
				break;
            }
*/
		default:
			stat = OPEN;
			break;
		}//switch the machine
	}//while(1)
	exit(0);
}


