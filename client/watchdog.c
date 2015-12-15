//producer_consumer,message mode,gpio ,remote updata, Independent check_flow
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MIN_COMMAND_LEN 5
#define MAX_COMMAND_LEN 10
#define BUFFER_SIZE 3072 //message mode
#define MAXBUF 100
int check_flow = 0; //Define a global variable, used to check_flow 
int stop_stream_flag = 0;//if the stream is stop we make the flag to 0,,,at 1106
int watchdog = 0;//we use this global variable to make the watchdog function
int restart_flag = 0;//we use this flag,that we know the result we logging the server

/*we send the flow to the server*/
int atcommand_to_server_pending = 0;
char atcommand_to_server[MAXBUF];

/*wen send the all the message to server*/
int atcommand_to_server_allmessage = 0;
char atcommand_to_servermesg[BUFFER_SIZE];

#include <fcntl.h>  
#include <malloc.h>
#define DATA_LEN 2
#define GPIO_NUM_LEN 4
#define file_len 126
#define GPIO_CLASS_NAME "/sys/class/gpio/export" //1026
/*this is use to add the gpio*/
    char  bu[DATA_LEN];
    int fd_class,fd_direction,fd_value,fd_num;
    char *gpio_dir[2]={"out","in"};
    int gpio_num1 = 34; 
    int gpio_num2 = 37;
    int gpio_num3 = 128;
    int gpio_num4 = 129;
    char gpio_direction_name1[file_len];
    char gpio_direction_name2[file_len];
    char gpio_value_name1[file_len];
    char gpio_value_name2[file_len];
    char *gpio_name1 = "34"; //heartbeat
    char *gpio_name2 = "37";//start stop stream

    char *gpio_name3 = "128";//we use this gpio to the watchdog
    char *gpio_name4 = "129"; //open the watchdog 

enum{
	START=0,
	LOGGING=1,
	PROCESSING=2,
	HEARTBEAT=3
};

struct circular_buffer{
	char buf[MAXBUF*2];
	int read_pos;//used by reader
	int write_pos;//used by writer
	int empty_len;
}cbuffer;

int login_c(int fd);
int process_command(char buffer[],int sockfd);
int get_emptylen(struct circular_buffer* cubuffer);
int writecommand(char buffer[],struct circular_buffer *cbuffer,int len);
int getcommand(char buffer[],struct circular_buffer *cbuffer,int len);

int main(int argc,char *argv[])  
{
	int sockfd = 0;
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
	int state_logging_c,state_processing_c,state_processing_fail,  state_connect_fail=0;
	char buffer[MAXBUF] = {0};
	char heartbeat[] = {0xF2,0x07,0x05,0x01,0xFF};//heartbeat 
	char reset[] = {0xF2,0x13,0x05,0x01,0x0B};//reset that  logging
	char change[] = {0xF2,0x13,0x05,0xF1,0xFB};//change the state status

	fd_set rfds;
	struct timeval tv;
	int retval, maxfd = -1;
 
	if(argc != 3) {  
		printf("Usage: fileclient <address> <port>/n");
		return 0;  
	}

	sockfd = 0;
	state = START;
	cbuffer.read_pos=0;
	cbuffer.write_pos=0;
	cbuffer.empty_len=MAXBUF*2;
	
  	atcommand_process(); //send the message to 10010

 	watchdog = 0;//crate the watchdog thread
  	watchdog_process();  //we process the watchdog
        
	while(1)
	{
		switch(state){
			case START:
				if(sockfd !=0)
				close(sockfd);
				if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1) 
				{  
					perror("sock");  
					state = START;
					break;
				} 
				bzero(&address,sizeof(address));  
				address.sin_family = AF_INET;  
				address.sin_port = htons(atoi(argv[2]));  
				inet_pton(AF_INET,argv[1],&address.sin_addr);  
				len = sizeof(address);  
   
				if((result = connect(sockfd, (struct sockaddr *)&address, len))==-1) 
				{  
					sleep(2);
					perror("connect"); 
					state_connect_fail ++ ;
					printf("connect's count is %d\n",state_connect_fail);
					if(state_connect_fail > 5)
					{
						watchdog = 1;
					}
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
					{
						state = START;
						break;
					}
					else
					{
						state = LOGGING;
						break;
					}
				}
			printf("LOGIN SUCESS!\n");	
				if(restart_flag == 0)
				{
					printf("the network is wrong,reset\n");
					send(sockfd,reset,5/*strlen(reset)*/,0);
				}
				else
				{
					printf("change the state logging\n");
					send(sockfd,change,5/*strlen(change)*/,0);
				}
			state_connect_fail=0;

			state = PROCESSING;	
			state_processing_c = 0;
      		state_processing_fail = 0;

      		restart_flag = 0;
			break;
	
		case PROCESSING:
			state_processing_c++;
			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			maxfd = 0;

			FD_SET(sockfd, &rfds);
			if (sockfd > maxfd)
				maxfd = sockfd;

			tv.tv_sec = 4;//if server don't send any commnad to us within 5s, then we send a hearbeat to the server///////////////////
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
			{
				state = HEARTBEAT;
				break;
			}
			//now retval >0 ,we have command to dealwith
			if (FD_ISSET(sockfd, &rfds))
			{
				bzero(buffer, MAXBUF+1);
				len = recv(sockfd, buffer, sizeof(buffer),0);
        printf("len = %d\n",len);


				if(len > 0){
					keepflag = 0;
					int len_command;
					if(len<=get_emptylen(&cbuffer)){
						//the the empty len is enough,so write the buffer to the circular buffer
						writecommand(buffer,&cbuffer,len);
					}
					//now get the command from the circular buffer 
					
					len_command=getcommand(buffer,&cbuffer,MAXBUF);	
					while(len_command >0){		
						process_command(buffer,sockfd);
						len_command=getcommand(buffer,&cbuffer,MAXBUF);
					}
					
				}
				else
				{
					sleep(3);
            			state_processing_fail++;
            			if(state_processing_fail > 10) 
            			{
							if(stop_stream_flag != 0)
							{			
								system(" /home/media/work/stop_stream.sh ");
							}
            				state=START;
            				printf("we switch to START state because we cannot receive message from server\n");
            				restart_flag = 1;
            				break;
            			}
						printf("Failed to receive the message! \n");
				}
			}
		    
			state = state;//keep state on processing state	
			break;
	
		case HEARTBEAT:
			signal(SIGPIPE, SIG_IGN);//ignore the signal of the SIGPIPE
			
			if(atcommand_to_server_pending==1)
			{
				send(sockfd,atcommand_to_server,strlen(atcommand_to_server),0);//take the same effect as a heartbeat
				atcommand_to_server_pending=0;
				send(sockfd, heartbeat, strlen(heartbeat), 0);
			}
			else if(atcommand_to_server_allmessage == 1)
			{
				send(sockfd,atcommand_to_servermesg,strlen(atcommand_to_servermesg),0);
				atcommand_to_server_allmessage=0;
				send(sockfd,heartbeat,strlen(heartbeat),0);
			}
			else /*if(atcommand_to_server_pending == 0 || atcommand_to_server_allmessage == 0)*/
			{
				len = send(sockfd, heartbeat, strlen(heartbeat), 0);
				if (len <= 0)
				{
					printf("failed to send heartbeat !\n");//in this case, we also add keepflag by 1,to switch the state to START
					state =START;//it is recommended to set START state
					break;
				}
				else			
				{
					printf("keepflag = %d\n",keepflag);
					printf("heartbeat send!\n");
					keepflag += 1;
					if(keepflag > 10)
					{
						state = START;
						printf("we switch to START state because we sent 10 times heartbeat that but the server not sent us\n");
						keepflag = 0;
						restart_flag = 1;
						break;
					}
				}
			}
			state = PROCESSING;
			state_processing_c =0;
			state_processing_fail=0;
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
		err = pthread_create(&tid,NULL,operation,(void *)fd);//at 10.12
    if(err != 0)
       printf("error creat");	
}

void * operation(void * arg)
{
	int sockfd = (int)arg; //at 10.12
	char command[] = {0xF2,0x08,0x05,0xF1,0xF0};//faild
	char command1[] = {0xF2,0x08,0x05,0x01,0x00};//success
	stop_stream_flag = 1; //when we start the stream .we turn the flag to 1
	int status;
	int val; //used by mutex
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
		startstreamgpio();
		printf("run command successful\n");
	
		sleep(300);//if the stream isn't stop in 1800'
		if(stop_stream_flag != 0)
		{
			stop_stream(sockfd);
			printf("ok ok ok ok ok !!!!\n");
		}
		else
		{
			printf("the stream is stop!\n");
		}
		pthread_exit((void *)1);
	}
//pthread_mutex_unlock(&mutex);//unlock the mutex
}


//停止推视频流
int stop_stream(int fd)
{
	int sockfd = fd;
	char command[] = {0xF2,0x09,0x05,0xF1,0xF1};//faild
	char command1[] = {0xF2,0x09,0x05,0x01,0x01};//success
	int err;
	err = system(" /home/media/work/stop_stream.sh ");
	if((err == -1))
	{
		send(sockfd,command,sizeof(command),0);
		printf("system stop stream error\n");
	}
	else
	{
		printf("RUN the stop stream successful!\n");
		stop_stream_flag = 0; //at 1106,when we stop the stream , we make the flag to 0
		send(sockfd,command1,sizeof(command1),0);
		stopstreamgpio();
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

	printf("HA HA HA HA the server is alive\n\n");
	heartbeatgpio();
}

//Can be remotely updated
int remote(int fd)
{
        int sockfd = fd;
        char command[] = {0xF2,0x11,0x05,0xF1,0xF9};//faild
        char command1[] = {0xF2,0x11,0x05,0x01,0x09};//success
        int err;
        err = system(" /home/media/work/remote.sh ");
        if((err == -1)){
            send(sockfd,command,sizeof(command),0);
            printf("system remote error\n");
        }
        else{
            if(WIFEXITED(err)){
                if(0 == WEXITSTATUS(err)){
						printf("RUN the remote successful!\n");
						send(sockfd,command1,sizeof(command1),0);
                }
                else{
						printf("RUN the remote faild! code is:\n",WEXITSTATUS(err));
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
//close the remote
int closeremote(int fd)
{
        int sockfd = fd;
        char command[] = {0xF2,0x12,0x05,0xF1,0xFA};//faild
        char command1[] = {0xF2,0x12,0x05,0x01,0x0A};//success
        int err;
        err = system(" /home/media/work/closeremote.sh ");
        if((err == -1)){
                send(sockfd,command,sizeof(command),0);
                printf("system closeremote error\n");
        }
        else{
                if(WIFEXITED(err)){
                    if(0 == WEXITSTATUS(err)){
                            printf("RUN the closeremote successful!\n");
                            send(sockfd,command1,sizeof(command1),0);
                    }
                    else{
                            printf("RUN the closeremote faild! code is:\n",WEXITSTATUS(err));
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


//when we reach a heartbeat,the gpio is light 2 times
int heartbeatgpio()
{
	sprintf(gpio_direction_name1,"/sys/class/gpio/gpio%s/direction",gpio_name1); //heartbeat
	sprintf(gpio_value_name1,"/sys/class/gpio/gpio%s/value",gpio_name1);         //heartbeat

    int a=0,b=1;
    if((access(gpio_direction_name1,F_OK)==0)||(access(gpio_value_name1,F_OK)==0))
        goto run;
    if((fd_class=open(GPIO_CLASS_NAME,O_WRONLY))<0){
        perror("failed open gpio class\n");
        goto err_open_class;
    }
    if(write(fd_class,gpio_name1,strlen(gpio_name1))<0){
        printf("falied write gpio name (%s)\n",gpio_name1);
        goto err_write;
    }
    if((fd_direction=open(gpio_direction_name1,O_WRONLY))<0){
        perror("failed open gpio direction\n");
        goto err_open_direction;
    }   
    if(write(fd_direction,gpio_dir[0],strlen(gpio_dir[0]))<0){
        printf("falied write gpio (%s) direction \n",gpio_name1);
        goto err_write;
    }
run:
    if((fd_value=open(gpio_value_name1,O_RDWR))<0){
        perror("falied open gpio value\n");
        goto err_open_value;
    }
    sprintf(bu,"%d",a);
    write(fd_value,bu,1);
    sleep(1);
    sprintf(bu,"%d",b);
    write(fd_value,bu,1);

    err_open_class:
    err_open_direction:
    err_open_value:
    err_write:
    if(fd_class)
        close(fd_class);
    if(fd_direction)
        close(fd_direction);
    if(fd_value)
        close(fd_value);
    if(gpio_name1)
    	return 0;
}
//when we pull the stream ,the light is lighting
int startstreamgpio()
{
	sprintf(gpio_direction_name2,"/sys/class/gpio/gpio%s/direction",gpio_name2);  //start stop stream
	sprintf(gpio_value_name2,"/sys/class/gpio/gpio%s/value",gpio_name2);         //start stop stream

    int a=0;
    if((access(gpio_direction_name2,F_OK)==0)||(access(gpio_value_name2,F_OK)==0))
        goto run; 
    if((fd_class=open(GPIO_CLASS_NAME,O_WRONLY))<0){
        perror("failed open gpio class\n");
        goto err_open_class;
    }    
    if(write(fd_class,gpio_name2,strlen(gpio_name2))<0){
        printf("falied write gpio name (%s)\n",gpio_name2);
        goto err_write;
    }    
    if((fd_direction=open(gpio_direction_name2,O_WRONLY))<0){
        perror("failed open gpio direction\n");
        goto err_open_direction;
    }    
    if(write(fd_direction,gpio_dir[0],strlen(gpio_dir[0]))<0){
        printf("falied write gpio (%s) direction \n",gpio_name2);
        goto err_write;
    }    
run:
    if((fd_value=open(gpio_value_name2,O_RDWR))<0){
        perror("falied open gpio value\n");
        goto err_open_value;
    }    
    sprintf(bu,"%d",a);
    write(fd_value,bu,1);

    err_open_class:
    err_open_direction:
    err_open_value:
    err_write:
    if(fd_class)
        close(fd_class);
    if(fd_direction)
        close(fd_direction);
    if(fd_value)
        close(fd_value);
    if(gpio_name2)
    	return 0;
}
//when we stop the stream ,the light is off
int stopstreamgpio()
{
	sprintf(gpio_direction_name2,"/sys/class/gpio/gpio%s/direction",gpio_name2);  //start stop stream
	sprintf(gpio_value_name2,"/sys/class/gpio/gpio%s/value",gpio_name2);         //start stop stream

    int b=1;
    if((access(gpio_direction_name2,F_OK)==0)||(access(gpio_value_name2,F_OK)==0))
        goto run; 
    if((fd_class=open(GPIO_CLASS_NAME,O_WRONLY))<0){
        perror("failed open gpio class\n");
        goto err_open_class;
    }    
    if(write(fd_class,gpio_name2,strlen(gpio_name2))<0){
        printf("falied write gpio name (%s)\n",gpio_name2);
        goto err_write;
    }    
    if((fd_direction=open(gpio_direction_name2,O_WRONLY))<0){
        perror("failed open gpio direction\n");
        goto err_open_direction;
    }    
    if(write(fd_direction,gpio_dir[0],strlen(gpio_dir[0]))<0){
        printf("falied write gpio (%s) direction \n",gpio_name2);
        goto err_write;
    }    
run:
    if((fd_value=open(gpio_value_name2,O_RDWR))<0){
        perror("falied open gpio value\n");
        goto err_open_value;
    }    
    sprintf(bu,"%d",b);
    write(fd_value,bu,1);

    err_open_class:
    err_open_direction:
    err_open_value:
    err_write:
    if(fd_class)
        close(fd_class);
    if(fd_direction)
        close(fd_direction);
    if(fd_value)
        close(fd_value);
    if(gpio_name1)
    	return 0;
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
	char user[16] = {"board1"};
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
						if(stop_stream_flag == 0)
						{
							start_stream(sockfd);
						}
						else
						{
							printf("the stream is pulling\n");
						}
					}
					else if (buffer[1] == 0x09){
						stop_stream(sockfd);
					}
					else if(buffer[1] == 0x10){ //the command is to check the flow,then the globle is change
						check_flow = 1;
					}
                    	else if(buffer[1] == 0x11){
                        	remote(sockfd);
                    	}
					else if(buffer[1] == 0x12){
						closeremote(sockfd);
					}				
					return 0;
}

/*
   we can receive the message from the 10010
   when we receive the command ,we begin to check the flow
   when we couldn't receive and receive the command to check the flow,then we check the flow by ourselves
*/
//send the message
void *send_gsm(void *arg);
int atcommand_process(void)
{

        pthread_t tid1;
        int err;
        err = pthread_create(&tid1,NULL,send_gsm,(void *)NULL);//at 10.12
	if(err != 0)
		printf("error creat"); 
}

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
		OPEN=7,
		WAIT=8
	};
		int stat;
 //       int sockfd = (int)arg;//at 10.12
        int portfd;
        int nwrite, nread;
        char buff[BUFFER_SIZE];
        char buffer1[20] = {'A','T','\r'};  //confirm the command
        char buffer2[20] = {'A','T','+','C','M','G','F','=','1','\r'}; //send the message's style,1->text,0->PDU
        char buffer3[20] = {'A','T','+','C','M','G','S','=','"','1','0','0','1','0','"','\r'};//send the message
        char buffer4[20] = {'C','X','L','L','\r',0x1A,'\r'};//send the message that check the stream
        char buffer5[20] = {'A','T','+','C','M','G','L','=','"','A','L','L','"','\r'}; //read the whole message
        char buffer6[20] = {'A','T','+','C','M','G','D','=','1',',','4','\r'}; //delete all the message
		char buffer7[20] = {'A','T','E','0','\r'};
		int checktotal = 0; //the num is that we check the mode is empty's times
		portfd = 0;
		stat = OPEN;
	while(1)
	{
		switch(stat){
			case OPEN:    //open the seriport 
				if(portfd != 0)
					close(portfd);
				if((portfd = open("/dev/ttyUSB4",O_RDWR|O_NOCTTY|O_NDELAY)) < 0) //open the serial port
				{
					perror("open_port\n");
					stat = OPEN;
					break;
				}
				printf("open port ok!\n");
				write(portfd,buffer7,strlen(buffer7));
				printf("close ATE0\n");
				stat = CONFIRM;
				break;

			case CONFIRM:  //AT confirm the seriport 
				nwrite = write(portfd,buffer1,strlen(buffer1));
					printf("write the confirm of message len is:%d\n",nwrite);
					printf("buffer1 is:%s\n",buffer1);
				sleep(2);
				nread = read(portfd, buff, BUFFER_SIZE);
					printf("receive the buff Len is:%d\n", nread);
	printf("confirm's buff is %s\n",buff);
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
					printf("the mode is empty!!\n");
					checktotal++; //when the mode is empty ,the checktotal is plus 1
					stat = WAIT;
					break;
				}
				else{
					checktotal = 0;
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
				sleep(40);
					stat = CHECK;
					break;
			
			case PROCESS: //we process the string
				printf("receive the buff Len is:%d\n", nread);
				printf("buff is:%s\n",buff); //printf the message that receive GSM

				memcpy(atcommand_to_servermesg,buff,strlen(buff)); //send the whole message to server
					atcommand_to_server_allmessage =1;
				sleep(3);

				char *string = NULL;
				char buffer[10];
				char comm[20];
				char checksum = 0x00;
				char *ptr;
				char *strbegin = "52694F596D4191CF0020",*strend = "004D0042000D000A0020672C6570";
				int beginindex,endindex,beginstrlength = strlen(strbegin);

				ptr = strstr(buff, strbegin);
					beginindex = ptr-buff-1;
					ptr = strstr(buff, strend);
					endindex = ptr-buff;
				int n = endindex - beginindex - beginstrlength;
				if(n>0)
				{
					string=(char*)malloc((n)*sizeof(char));
					strncpy(string, buff+beginindex+beginstrlength+1, n-1); 
					string[n-1]='\0';
					printf("string:%s\n", string); //打印剩余流量的字符串

                		int len = strlen(string); ///// 
					printf("%d\n",len); //打印剩余流量字符串长度
                		int i, j=0, k=0;
                		for(i=0;i<len;i++)
                		{ //处理剩余流量字符串string,string是收到的剩余流量的字符串,buffer是处理后的字符串
						if((i+1)%4 == 0)
						{ 
							if(string[i] != 'E' )
							{
								buffer[j]=string[i];
								j++;
							}   
							else
							{
								buffer[j] = '.';
								j++;
							}  
					   		buffer[len / 4] = '\0';	
						}   
					}   
          			printf("buffer is:%s\n",buffer);//打印处理后的字符串，显示剩余流量
          			free(string);
					sprintf(comm,"%c%c%c%-12s",0xF2,0x10,0x0F,buffer);//we make this command's length is 15
					for( i=0;i<14;i++ )
						checksum += comm[i];
						comm[14] = checksum;
						comm[15] = '\0';
					signal(SIGPIPE, SIG_IGN);  //ignore the sigpipe,because the sigpipe will kill the whole process by default
					if(strlen(comm) != 0)
					{
						memcpy(atcommand_to_server,comm,strlen(comm));
						atcommand_to_server_pending=1;
						sleep(3);					
						stat = DELETE;
						break;
					}
					else
					{
						stat = PROCESS;
						break;
					}
				}//n >0
				else
				{
					stat = DELETE;
					break;
				}//n<=0

		case DELETE: //AT+CMGD=1,4 ,delete all message
			nwrite = write(portfd,buffer6,strlen(buffer6));  //delete all the message
				printf("write the delete all the message's length is:%d\n",nwrite);
				printf("buffer6 is:%s\n",buffer6);
			int k = 0;
            sleep(2);
            nread = read(portfd, buff, BUFFER_SIZE);
				printf("receive the buff Len is:%d\n", nread);
            if(buff[2] == 'O' && buff[3] == 'K')
             	{
                printf("delete ALL the message is ok!!\n");
                stat = CHECK;
                break;
			}	
            else
            {
                	printf("delete message is wrong!!\n");
                	stat = DELETE;
				break;
            }

		case WAIT: //we check the message mode ,if the mode is empty ,jump it
				if(check_flow == 1)
				{  //when we receive the command to check flow
					check_flow = 0;
					stat = SENDNUM;
					break;
                	}
                	else
                	{
                        sleep(30);
                printf("##################checktotal=%d\n",checktotal);
                        if(checktotal > 2880){ //when we check the mode 1440 times(every time is 60',total is a day) is empty we sent the message to 10010
                        stat = SENDNUM;
                        break;
                   }
                  stat = CHECK;
		    		sleep(2);
                  break;
                }

		default:
			stat = OPEN;
			break;
		}//switch the machine
	}//while(1)
	exit(0);
}

		
int get_emptylen(struct circular_buffer* cbuffer)
{
	return cbuffer->empty_len;
}

int writecommand(char buffer[],struct circular_buffer *cbuffer,int len)
{
	int i;
	if(len>cbuffer->empty_len){
		printf("not enough space for writing command\n");
		return -1;
	}
	
	for(i=0;i<len;i++){
		cbuffer->buf[cbuffer->write_pos]=buffer[i];
		cbuffer->write_pos++;
		if(cbuffer->write_pos>=2*MAXBUF)
			cbuffer->write_pos=0;
	}
	cbuffer->empty_len-=len;
	return 0;
}

int getcommand( char buffer[], struct circular_buffer *cbuffer, int len)
{
	int read_pos_backup = 0;
	
	int i;
	int command_len;
	char ch;
	int cbuflen = 2*MAXBUF - cbuffer->empty_len;//the effective character on the circular buffer
	if(cbuflen <= 0)				//没有有效数据；
	return -1;
//printf("\nread_pos:%d\n",cbuffer->read_pos); 
//printf("empty_len:%d\n",cbuffer->empty_len);

	//searching the sync word
	for(i=0;i<cbuflen;i++) {
		ch = cbuffer->buf[cbuffer->read_pos];
		if(ch == 0xF1)	
			break;
		else {
			printf("discard this ch:%02x\n",ch);
			cbuffer->read_pos++;
			if(cbuffer->read_pos >= 2*MAXBUF)
				cbuffer->read_pos = 0;
			cbuffer->empty_len++;  
	//		printf("empty_len1:%d\n",cbuffer->empty_len); 
			if(cbuffer->empty_len == 2*MAXBUF)
				return -1;
        	}
	}
    
	cbuflen = 2*MAXBUF - cbuffer->empty_len;//the effective character on the circular buffer
	command_len = cbuffer->buf[(cbuffer->read_pos+2)%(MAXBUF*2)];//the length of this command
//	printf("command_len:%3d  cbuflen:%3d\n", command_len, cbuflen);
	if(cbuflen < MIN_COMMAND_LEN)				//uncomplete command remains on the circular buffer
		return -1;

	if(command_len > MAX_COMMAND_LEN || command_len < MIN_COMMAND_LEN) {
		printf("the command_len is invailed!\n");
		printf("discard this char(command_len):%02x\n",cbuffer->buf[cbuffer->read_pos]);
		cbuffer->read_pos++;
		if(cbuffer->read_pos >= 2*MAXBUF)
			cbuffer->read_pos = 0;
		
		cbuffer->empty_len++;
//		printf("empty_len2:%d\n",cbuffer->empty_len);
		return 1;
	}
	
	if(command_len > cbuflen) {
		printf("uncomplete command remains on the circular buffer\n");
		return -1;
    }
    else {
		read_pos_backup = cbuffer->read_pos + 1;				//备份当前读指针+1；
		if(read_pos_backup >= 2*MAXBUF)
			read_pos_backup = 0;
		
		for(i=0;i<command_len;i++){				//copy the command;
			buffer[i] = cbuffer->buf[cbuffer->read_pos];
//	printf("%3x",buffer[i]);
			
			cbuffer->read_pos++;
			if(cbuffer->read_pos >= 2*MAXBUF)
				cbuffer->read_pos = 0;
		}
	printf("\n");
		
		if(checksum_check(buffer, command_len) == 0) {			//校验和正确
			cbuffer->empty_len += command_len;
//			printf("empty_len3:%d\n",cbuffer->empty_len);
			return command_len;
		}
		else {													//校验和错误，恢复读指针；
			cbuffer->read_pos = read_pos_backup;
//	printf("read_pos(checksum_check fail):%d\n",cbuffer->read_pos);
//	printf("CHECKSUM ERROR!\n");
			cbuffer->empty_len++;
//			printf("empty_len4:%d\n",cbuffer->empty_len);
			return 1;
		}
	}
}


int checksum_check(char command[], int n)
{
	char checksum = 0x00;
	int i;
	char m;
	if(n >= 5){		//命令最小长度为5;  （起始字 命令字 长度 数据 校验和）
		m = command[2];     //命令长度;
		for( i=0;i<m-1;i++ )
			checksum += command[i];
		if( checksum == command[m-1] )
			return 0;
		else {
			return -1;
		}
	}
	else
		return -1;
}

//the watchdog function
void *watchdog_function(void *arg);
watchdog_process(void)
{
	  	pthread_t tid3;
        int err;
        err = pthread_create(&tid3,NULL,watchdog_function,(void*)NULL);//at 11.16
		if(err != 0)
			printf("error creat"); 
}
void *watchdog_function(void *arg)
{
	enum
	{
		CIRCLE=0,
		SENDGPIO=1
	};
	openwatchdog(); //make the watchdog's EN work
	int stat;
     stat = CIRCLE;
     while(1)
     {
     	switch(stat)
     	{
     		case CIRCLE:
     			if(watchdog == 0)
     			{
   					watchdoggpio();						
    				}//if(watchdog == 0)
    				else
    				{
    					stat = SENDGPIO;
    					break;
    				}
    				stat = CIRCLE;
    				break ;
    			case SENDGPIO:
    				sleep(20); //we couldn't send the signal to the gpio,then the watchadog is resert
    				stat = CIRCLE;
    				break;
     	}//switch(stat)
     }//while(1)
}//watchdog_function()

int watchdoggpio()
{
	sprintf(gpio_direction_name1,"/sys/class/gpio/gpio%s/direction",gpio_name3); //heartbeat
	sprintf(gpio_value_name1,"/sys/class/gpio/gpio%s/value",gpio_name3);         //heartbeat

    int a=0,b=1;
    if((access(gpio_direction_name1,F_OK)==0)||(access(gpio_value_name1,F_OK)==0))
        goto run;
    if((fd_class=open(GPIO_CLASS_NAME,O_WRONLY))<0){
        perror("failed open gpio class\n");
        goto err_open_class;
    }
    if(write(fd_class,gpio_name3,strlen(gpio_name3))<0){
        printf("falied write gpio name (%s)\n",gpio_name3);
        goto err_write;
    }
    if((fd_direction=open(gpio_direction_name1,O_WRONLY))<0){
        perror("failed open gpio direction\n");
        goto err_open_direction;
    }   
    if(write(fd_direction,gpio_dir[0],strlen(gpio_dir[0]))<0){
        printf("falied write gpio (%s) direction \n",gpio_name3);
        goto err_write;
    }
run:
    if((fd_value=open(gpio_value_name1,O_RDWR))<0){
        perror("falied open gpio value\n");
        goto err_open_value;
    }
    sprintf(bu,"%d",a);
    write(fd_value,bu,1);
    usleep(5000);
    sprintf(bu,"%d",b);
    write(fd_value,bu,1);
    usleep(5000);
    sprintf(bu,"%d",a);
    write(fd_value,bu,1);
    usleep(5000);
    sprintf(bu,"%d",b);
    write(fd_value,bu,1);

    err_open_class:
    err_open_direction:
    err_open_value:
    err_write:
    if(fd_class)
        close(fd_class);
    if(fd_direction)
        close(fd_direction);
    if(fd_value)
        close(fd_value);
    if(gpio_name3)
    	return 0;
}

int openwatchdog()
{
	sprintf(gpio_direction_name1,"/sys/class/gpio/gpio%s/direction",gpio_name4); //heartbeat
	sprintf(gpio_value_name1,"/sys/class/gpio/gpio%s/value",gpio_name4);         //heartbeat

    int a=0,b=1;
    if((access(gpio_direction_name1,F_OK)==0)||(access(gpio_value_name1,F_OK)==0))
        goto run;
    if((fd_class=open(GPIO_CLASS_NAME,O_WRONLY))<0){
        perror("failed open gpio class\n");
        goto err_open_class;
    }
    if(write(fd_class,gpio_name4,strlen(gpio_name4))<0){
        printf("falied write gpio name (%s)\n",gpio_name4);
        goto err_write;
    }
    if((fd_direction=open(gpio_direction_name1,O_WRONLY))<0){
        perror("failed open gpio direction\n");
        goto err_open_direction;
    }   
    if(write(fd_direction,gpio_dir[0],strlen(gpio_dir[0]))<0){
        printf("falied write gpio (%s) direction \n",gpio_name4);
        goto err_write;
    }
run:
    if((fd_value=open(gpio_value_name1,O_RDWR))<0){
        perror("falied open gpio value\n");
        goto err_open_value;
    }
    sprintf(bu,"%d",a);
    write(fd_value,bu,1);

    err_open_class:
    err_open_direction:
    err_open_value:
    err_write:
    if(fd_class)
        close(fd_class);
    if(fd_direction)
        close(fd_direction);
    if(fd_value)
        close(fd_value);
    if(gpio_name4)
    	return 0;
}
