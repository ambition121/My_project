#include <signal.h>
#include <strings.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <stdio.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <unistd.h>  
#include <stdlib.h>  
#include <pthread.h>  
void *rec_data(void *fd);  
int main(int argc,char *argv[])  
{  
       int server_sockfd;  
	   int *client_sockfd;  
       int server_len, client_len;  
       struct sockaddr_in server_address;  
       struct sockaddr_in client_address;  
       struct sockaddr_in tempaddr;  
       int i,byte;  
       char char_recv,char_send;  
       socklen_t templen;  
       server_sockfd = socket(AF_INET, SOCK_STREAM, 0);//创建套接字  
   
       server_address.sin_family = AF_INET;  
//       server_address.sin_addr.s_addr =  htonl(INADDR_ANY);
	 server_address.sin_addr.s_addr =  inet_addr("192.168.1.100"); //change the IP to the network byte order 
       server_address.sin_port = htons(9734); //主机的字节顺序转换成网络字节顺序 
       server_len = sizeof(server_address);  
        
       bind(server_sockfd, (struct sockaddr *)&server_address, server_len);//绑定套接字  
//	   listen(server_sockfd,5);
	   templen = sizeof(struct sockaddr);  
   
       printf("server waiting for connect\n");  
	   listen(server_sockfd,5); //监听

       while(1){  
              pthread_t thread;//创建不同的子线程以区别不同的客户端  
              client_sockfd = (int *)malloc(sizeof(int));  
              client_len = sizeof(client_address);  
              *client_sockfd = accept(server_sockfd,(struct sockaddr *)&client_address, (socklen_t *)&client_len); //accept返回一个新的套接字描述符 
              if(-1==*client_sockfd){  
                     perror("accept");  
                     //continue;
				   exit(1);	 
              }  
              if(pthread_create(&thread, NULL, rec_data, client_sockfd)!=0)//创建子线程  
              {  
                     perror("pthread_create");  
  					 break;  
              }  
 
		 	  }  
       shutdown(*client_sockfd,2);  
       shutdown(server_sockfd,2);  
}  
/***************************************** 
* 函数名称：rec_data 
* 功能描述：接受客户端的数据 
* 参数列表：fd——连接套接字 
* 返回结果：void 
*****************************************/  
void *rec_data(void *fd)  
{  
       int client_sockfd;  
       int i,byte;  
       char char_recv[100];//存放数据  
       client_sockfd=*((int*)fd);  
       for(;;)  
       {  
              if((byte=recv(client_sockfd,char_recv,100,0))==-1)  
              {  
                     perror("recv");  
                     exit(EXIT_FAILURE);   
              }  
//              if(strcmp(char_recv, "exit")==0)//接受到exit时，跳出循环  
//                   break;  
  //            printf("receive from client is %s\n",char_recv);//打印收到的数据  
			
			  if((strcmp(char_recv,"exit") != 0) ){		//
				printf("receive from client is %s\n",char_recv);//
			  }  //
			  else{ //
				exit(1);   //
			  }  //


       }  
       free(fd);  
       close(client_sockfd);  
       pthread_exit(NULL);  
}
