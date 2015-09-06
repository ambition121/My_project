#include<stdio.h>      /*标准输入输出定义*/        
#include<stdlib.h>     /*标准函数库定义*/  
#include<unistd.h>     /*Unix 标准函数定义*/  
#include<sys/types.h>   
#include<sys/stat.h>     
#include<fcntl.h>      /*文件控制定义*/  
#include<termios.h>    /*PPSIX 终端控制定义*/  
#include<errno.h>      /*错误号定义*/  
#include<string.h>  
   
//宏定义  
#define FALSE  -1  
#define TRUE   0 
#define BUFFER_SIZE 1024

/******************************************************************* 
* 名称：                setbit 
* 功能：                设置串口数据位，停止位和效验位 
* 入口参数：        fd       串口文件描述符 
*                   speed     串口波特率 
*                   flow_ctrl   数据流控制 
*                   databits   数据位   取值为5,6,7或者8 
*                   stopbits   停止位   取值为 1 或者2 
*                   parity     奇偶校验位 取值为N,E,O,S 
*出口参数：          正确返回为1，错误返回为0 
*******************************************************************/  

int setbit(int fd,int speed,int flow_ctrl,int databits,int stopbits,int parity)  
{  
     
      int   i;  
         int   status;  
         int   speed_arr[] = { B115200, B19200, B9600, B4800, B2400, B1200, B300};  
     int   name_arr[] = {115200,  19200,  9600,  4800,  2400,  1200,  300};  
           
    struct termios options;  
     
    /*tcgetattr(fd,&options)得到与fd指向对象的相关参数，并将它们保存于options,该函数还可以测试配置是否正确，该串口是否可用等。若调用>成功，函数返回值为0，若调用失败，函数返回值为1. 
    */  
    if  ( tcgetattr( fd,&options)  !=  0)  
       {  
          perror("SetupSerial 1");      
          return(FALSE);   
       }  
    
    //设置串口输入波特率和输出波特率  
    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)  
                {  
                     if  (speed == name_arr[i])  
                            {               
                                 cfsetispeed(&options, speed_arr[i]);   
                                 cfsetospeed(&options, speed_arr[i]);    
                            }  
              }       
     
    //修改控制模式，保证程序不会占用串口  
    options.c_cflag |= CLOCAL;  
    //修改控制模式，使得能够从串口中读取输入数据  
    options.c_cflag |= CREAD;  
    
    //设置数据流控制  
    switch(flow_ctrl)  
    {  
        
       case 0 ://不使用流控制  
              options.c_cflag &= ~CRTSCTS;  
              break;     
        
       case 1 ://使用硬件流控制  
              options.c_cflag |= CRTSCTS;  
              break;  
       case 2 ://使用软件流控制  
              options.c_cflag |= IXON | IXOFF | IXANY;  
              break;  
    }  
    //设置数据位  
    //屏蔽其他标志位,指明发送和接收的每个字节的位数  
    options.c_cflag &= ~CSIZE;  
    switch (databits)  
    {    
       case 5    :  
                     options.c_cflag |= CS5;  
                     break;  
       case 6    :  
                     options.c_cflag |= CS6;  
                     break;  
       case 7    :      
                 options.c_cflag |= CS7;  
                 break;  
       case 8:      
                 options.c_cflag |= CS8;  
                 break;    
       default:     
                 fprintf(stderr,"Unsupported data size\n");  

                return (FALSE);   
    }  
    //设置校验位  
    switch (parity)  
    {    
       case 'n':  
       case 'N': //无奇偶校验位。  
                 options.c_cflag &= ~PARENB;   
                 options.c_iflag &= ~INPCK;     //INPCK使奇偶校验起作用 
                 break;   
       case 'o':    
       case 'O'://设置为奇校验      
                 options.c_cflag |= (PARODD | PARENB);   
                 options.c_iflag |= INPCK;               
                 break;   
       case 'e':   
       case 'E'://设置为偶校验    
                 options.c_cflag |= PARENB;         
                 options.c_cflag &= ~PARODD;         
                 options.c_iflag |= INPCK;        
                 break;  
       case 's':  
       case 'S': //设置为空格   
                 options.c_cflag &= ~PARENB;  
                 options.c_cflag &= ~CSTOPB;  
                 break;   
        default:    
                 fprintf(stderr,"Unsupported parity\n");      
                 return (FALSE);   
    }   
    // 设置停止位   
    switch (stopbits)  
    {    
       case 1:     
                 options.c_cflag &= ~CSTOPB; break;   
       case 2:     
                 options.c_cflag |= CSTOPB; break;  
       default:     
                       fprintf(stderr,"Unsupported stop bits\n");   
                       return (FALSE);  
    }  
     
  //修改输出模式，原始数据输出  
  options.c_oflag &= ~OPOST;  
    
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  
//  options.c_lflag &= ~(ISIG | ECHOE | ECHO); 
//  options.c_lflag &= ~ICANON; 
     
    //设置等待时间和最小接收字符  
    options.c_cc[VTIME] = 1; /* 读取一个字符等待1*(1/10)s */    
    options.c_cc[VMIN] = 1; /* 读取字符的最少个数为1 */  
     
    //如果发生数据溢出，接收数据，但是不再读取 刷新收到的数据但是不读  
    tcflush(fd,TCIFLUSH);  
     
    //激活配置 (将修改后的termios数据设置到串口中）  
    if (tcsetattr(fd,TCSANOW,&options) != 0)    
           {  
               perror("com set error!\n");    
              return (FALSE);   
           }  
    return (TRUE);   
} 



int main(int argc , char *argv[])
{
        int fd;
        char buff[BUFFER_SIZE];
		char buf1[]={0xff,0x01,0x00,0x08,0x00,0xff,0x08};//up
		char buf2[]={0xff,0x01,0x00,0x10,0x00,0xff,0x10};//down
		char buf3[]={0xff,0x01,0x00,0x04,0x15,0x00,0x1a};//left
		char buf4[]={0xff,0x01,0x00,0x02,0x15,0x00,0x18};//right
		char buf5[]={0xff,0x01,0x00,0x20,0x00,0x00,0x21};//zoom short
		char buf6[]={0xff,0x01,0x00,0x40,0x00,0x00,0x41};//zoom long
		char buf7[]={0xff,0x01,0x00,0x80,0x00,0x00,0x81};//focus close
		char buf8[]={0xff,0x01,0x01,0x00,0x00,0x00,0x02};//focus far
		char buf9[]={0xff,0x01,0x00,0x00,0x00,0x00,0x01};//stop

        if((fd = open("/dev/ttyUSB0",O_RDWR|O_NOCTTY|O_NDELAY)) < 0) /* 打开串口 */
        {
            perror("open_port");
            return 1;
        }
        if(setbit(fd, 9600,0,8,1,'N') < 0) /* 配置串口 */
        {
            perror("setbit");
            return 1;
        }


//向上
    if(0 == (strcmp(argv[1],"up"))){
				write(fd,buf1,sizeof(buf1));
                sleep(2);
        }

	//向下转动
	if(0 == (strcmp(argv[1],"down"))){
                write(fd,buf2,sizeof(buf2));
                sleep(2);
        }

	//向左移动
	if(0 == (strcmp(argv[1],"left"))){
				write(fd, buf3, sizeof(buf3));
				sleep(2);
				
		}
	
	//向右移动
	if(0 == (strcmp(argv[1],"right"))){
				write(fd,buf4,sizeof(buf4));
				sleep(2);
	}

	//变倍短
	if(0 == (strcmp(argv[1],"zoomshort"))){
				write(fd,buf5,sizeof(buf5));
				sleep(2);
	}

	//变倍长
    if(0 == (strcmp(argv[1],"zoomlong"))){
				write(fd,buf6,sizeof(buf6));
			    sleep(2);
        }
    
	//聚焦近
	if(0 == (strcmp(argv[1],"focusclose"))){
                write(fd,buf7,sizeof(buf7));
                sleep(2);
        }
    
	//聚焦远
	if(0 == (strcmp(argv[1],"focusfar"))){
                write(fd,buf8,sizeof(buf8));
                sleep(2);
        }

	//停止命令
	if(0 == (strcmp(argv[1],"stop"))){
				write(fd,buf9,sizeof(buf9));
				sleep(2);
		}


    close(fd);
    return 0;
		 
}
