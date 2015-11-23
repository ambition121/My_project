#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#define MAXBUF 100
#define count 8
#define proxyhost "123.57.250.247"
#define proxyport "9734"

struct job
{
    void* (*callback_function)(void *arg);    //线程回调函数
    void *arg;                                     //回调函数参数
    struct job *next;                            //指向下一个job的指针
};

struct threadpool
{
    int thread_num;                   //线程池中开启线程的个数
    int queue_max_num;                //队列中最大job的个数
    struct job *head;                 //指向job的头指针
    struct job *tail;                 //指向job的尾指针
    pthread_t *pthreads;              //线程池中所有线程的pthread_t
    pthread_mutex_t mutex;            //互斥信号量
    pthread_cond_t queue_empty;       //队列为空的条件变量
    pthread_cond_t queue_not_empty;   //队列不为空的条件变量
    pthread_cond_t queue_not_full;    //队列不为满的条件变量
    int queue_cur_num;                //队列当前的job个数
    int queue_close;                  //队列是否已经关闭
    int pool_close;                   //线程池是否已经关闭
};

struct user       //定义一个结构体，包含用户名和密码
{
    char name[16];
    char passwd[16];
};

struct user server[count] = {
    {"board1","1"},
    {"board2","1"},
    {"board3","1"},
    {"board4","1"},
    {"board5","1"},
    {"board6","1"},
    {"board7","1"},
    {"board8","1"}
};

//random send the command to server
struct num
{
    char sendcommand[10];
};
struct num sendbuffer[12] = {
    {0xF1,0x03,0x05,0x01,0xFA}, //login out
    {0xF1,0x10,0x05,0x01,0x07}, //pull the stream
    {0xF1,0x11,0x05,0x01,0x08}, //up
    {0xF1,0x11,0x05,0x02,0x09}, //down
    {0xF1,0x11,0x05,0x03,0x0A}, //left
    {0xF1,0x11,0x05,0x04,0x0B}, //right
    {0xF1,0x12,0x05,0x01,0x09}, //zoomlong
    {0xF1,0x12,0x05,0x02,0x0A}, //zoomshort
    {0xF1,0x13,0x05,0x01,0x0A}, //focusclose
    {0xF1,0x13,0x05,0x02,0x0B}, //focusfar
    {0xF1,0x15,0x05,0x01,0x0C}, //stop make the camera
    {0xF1,0x16,0x05,0x01,0x0D}  //stop pull the stream
};

//================================================================================================
//函数名：                   threadpool_init
//函数描述：                 初始化线程池
//输入：                    [in] thread_num     线程池开启的线程个数
//					   [in] queue_max_num  队列的最大job个数 
//输出：                    无
//返回：                    成功：线程池地址 失败：NULL
//================================================================================================
struct threadpool* threadpool_init(int thread_num, int queue_max_num);


//================================================================================================
//函数名：                    threadpool_add_job
//函数描述：                  向线程池中添加任务
//输入：                    [in] pool                  线程池地址
//					   [in] callback_function     回调函数
//                          [in] arg                     回调函数参数
//输出：                     无
//返回：                     成功：0 失败：-1
//================================================================================================
int threadpool_add_job(struct threadpool *pool, void* (*callback_function)(void *arg), void *arg);


//================================================================================================
//函数名：                    threadpool_destroy
//函数描述：                  销毁线程池
//输入：                      [in] pool       线程池地址
//输出：                      无
//返回：                      成功：0 失败：-1
//================================================================================================
int threadpool_destroy(struct threadpool *pool);


//================================================================================================
//函数名：                    threadpool_function
//函数描述：                  线程池中线程函数
//输入：                     [in] arg                  线程池地址
//输出：                     无  
//返回：                     无
//================================================================================================
void* threadpool_function(void* arg);


struct threadpool* threadpool_init(int thread_num, int queue_max_num)//初始化线程池
{
    struct threadpool *pool = NULL;
    do 
    {
        pool = malloc(sizeof(struct threadpool)); //动态分配线程池的大小
        if (NULL == pool)
        {
            printf("failed to malloc threadpool!\n");
            break;
        }
        pool->thread_num = thread_num;
        pool->queue_max_num = queue_max_num;
        pool->queue_cur_num = 0;
        pool->head = NULL;
        pool->tail = NULL;
        if (pthread_mutex_init(&(pool->mutex), NULL))
        {
            printf("failed to init mutex!\n");
            break;
        }
        if (pthread_cond_init(&(pool->queue_empty), NULL))
        {
            printf("failed to init queue_empty!\n");
            break;
        }
        if (pthread_cond_init(&(pool->queue_not_empty), NULL))
        {
            printf("failed to init queue_not_empty!\n");
            break;
        }
        if (pthread_cond_init(&(pool->queue_not_full), NULL))
        {
            printf("failed to init queue_not_full!\n");
            break;
        }
        pool->pthreads = malloc(sizeof(pthread_t) * thread_num);
        if (NULL == pool->pthreads)
        {
            printf("failed to malloc pthreads!\n");
            break;
        }
        pool->queue_close = 0;//工作队列关闭的标志初始化为0
        pool->pool_close = 0; //线程池关闭的标志初始化为0
        int i;
        for (i = 0; i < pool->thread_num; ++i)//初始化时候建立一定数量的线程,根据thread_num的大小，循环创建线程
        {
            pthread_create(&(pool->pthreads[i]), NULL, threadpool_function, (void *)pool);
        }
        
        return pool;    
    } while (0);
    
    return NULL;
}

int threadpool_add_job(struct threadpool* pool, void* (*callback_function)(void *arg), void *arg)
{
    assert(pool != NULL);
    assert(callback_function != NULL);
    assert(arg != NULL);

    pthread_mutex_lock(&(pool->mutex));
    while ((pool->queue_cur_num == pool->queue_max_num) && !(pool->queue_close || pool->pool_close))
    {
        pthread_cond_wait(&(pool->queue_not_full), &(pool->mutex));   //队列满的时候就等待
    }
    if (pool->queue_close || pool->pool_close)    //队列关闭或者线程池关闭就退出
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    struct job *pjob =(struct job*) malloc(sizeof(struct job));
    if (NULL == pjob)
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    } 
    pjob->callback_function = callback_function;    
    pjob->arg = arg;
    pjob->next = NULL;
    if (pool->head == NULL)   
    {
        pool->head = pool->tail = pjob;
        pthread_cond_broadcast(&(pool->queue_not_empty));  //队列空的时候，有任务来时就通知线程池中的线程：队列非空
    }
    else
    {
        pool->tail->next = pjob;
        pool->tail = pjob;    
    }
    pool->queue_cur_num++;
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

void* threadpool_function(void* arg)
{
    struct threadpool *pool = (struct threadpool*)arg;
    struct job *pjob = NULL;
    while (1)  //死循环
    {
        pthread_mutex_lock(&(pool->mutex));
        while ((pool->queue_cur_num == 0) && !pool->pool_close)   //队列为空时，就等待队列非空
        {
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->mutex));
        }
        if (pool->pool_close)   //线程池关闭，线程就退出
        {
            pthread_mutex_unlock(&(pool->mutex));
            pthread_exit(NULL);
        }
        pool->queue_cur_num--;
        pjob = pool->head;
        if (pool->queue_cur_num == 0)
        {
            pool->head = pool->tail = NULL;
        }
        else 
        {
            pool->head = pjob->next;
        }
        if (pool->queue_cur_num == 0)
        {
            pthread_cond_signal(&(pool->queue_empty));        //队列为空，就可以通知threadpool_destroy函数，销毁线程函数
        }
        if (pool->queue_cur_num == pool->queue_max_num - 1)
        {
            pthread_cond_broadcast(&(pool->queue_not_full));  //队列非满，就可以通知threadpool_add_job函数，添加新任务
        }
        pthread_mutex_unlock(&(pool->mutex));
        
        (*(pjob->callback_function))(pjob->arg);   //线程真正要做的工作，回调函数的调用
        free(pjob);
        pjob = NULL;    
    }
}
int threadpool_destroy(struct threadpool *pool)
{
    assert(pool != NULL);
    pthread_mutex_lock(&(pool->mutex));
    if (pool->queue_close || pool->pool_close)   //线程池已经退出了，就直接返回
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    
    pool->queue_close = 1;        //置队列关闭标志
    while (pool->queue_cur_num != 0)
    {
        pthread_cond_wait(&(pool->queue_empty), &(pool->mutex));  //等待队列为空
    }    
    
    pool->pool_close = 1;      //置线程池关闭标志
    pthread_mutex_unlock(&(pool->mutex));
    pthread_cond_broadcast(&(pool->queue_not_empty));  //唤醒线程池中正在阻塞的线程
    pthread_cond_broadcast(&(pool->queue_not_full));   //唤醒添加任务的threadpool_add_job函数
    int i;
    for (i = 0; i < pool->thread_num; ++i)
    {
        pthread_join(pool->pthreads[i], NULL);    //等待线程池的所有线程执行完毕
    }
    
    //pthread_cond_destroy------需要注意的是只有在没有线程在该条件变量上等待时，才可以注销条件变量，否则会返回EBUSY。
    //同时Linux在实现条件变量时，并没有为条件变量分配资源，所以在销毁一个条件变量时，只要注意该变量是否仍有等待线程即可
    pthread_mutex_destroy(&(pool->mutex));          //清理资源
    pthread_cond_destroy(&(pool->queue_empty));
    pthread_cond_destroy(&(pool->queue_not_empty));   
    pthread_cond_destroy(&(pool->queue_not_full));    
    free(pool->pthreads);
    struct job *p;
    while (pool->head != NULL)
    {
        p = pool->head;
        pool->head = p->next;
        free(p);
    }
    free(pool);
    return 0;
}

void* work(void* arg)  //线程真正要执行的函数（thredpool_add_job加入进去的函数）
{
    char *p = (char*) arg;
    printf("threadpool callback fuction : %d\n", atoi(p));

    enum{ //we use the state machine,we create the struct
        START=0,
        LOGGING=1,
        PROCESSING=2,
        SENDCOMMAND=3
    };

    int state;
    state = START;
    struct sockaddr_in address;
    int result, len, ret;
    int sockfd = 0;
    fd_set rfds;
    int maxfd = -1;
    int state_logging_c, state_processing_c, state_processing_fail;
    struct timeval tv;
    int retval;
    char buffer[MAXBUF] = {0}; //receive the command
    int number;//random number随机数
    while(1)
    {
        switch(state)
        {
            case START:
                if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1)  //create the socker
                {  
                    perror("sock"); 
                    state =  START;
                    break;
                }

                printf("*******************sockfd%d's sock is:%d\n",atoi(p),sockfd) ; 
                bzero(&address,sizeof(address));  
                address.sin_family = AF_INET;  
                address.sin_port = htons(atoi(proxyport));  
                inet_pton(AF_INET,proxyhost,&address.sin_addr);  
                len = sizeof(address);  
   
                if((result = connect(sockfd, (struct sockaddr *)&address, len))==-1) //try to connect the server
                {  
                    perror("connect");  
                    state = START;
                    break;
                }
                state = LOGGING;
                state_logging_c=0;
                break;

            case LOGGING:
                signal(SIGPIPE, SIG_IGN);//ignore the signal of the sigpipe
                ret = login_app(sockfd,atoi(p)); //登录APP
                state_logging_c += 1;
                if(ret < 0)
                {
                    sleep(2);
                    if(state_logging_c > 10) //if we couldn't login the server 10 times,we START,every time is 2'
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
                printf("board%d LOGIN SUCCESS!\n",atoi(p));
                state = PROCESSING; 
                state_processing_c = 0;
                state_processing_fail = 0;
                break;

            case PROCESSING:
                state_processing_c++;
                FD_ZERO(&rfds);
                FD_SET(sockfd, &rfds);
                if (sockfd > maxfd)
                    maxfd = sockfd;

                tv.tv_sec = 5;//if server don't send any commnad to us within 5s, then we send a hearbeat to the server///////////////////
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
                    state = SENDCOMMAND;
                    break;
                }

                if (FD_ISSET(sockfd, &rfds)) //detail the command that we receive
                {
                    bzero(buffer, MAXBUF+1); //we put the command into the buffer
             //     len = recv(sockfd, buffer, MAXBUF, 0);
                    len = recv(sockfd, buffer, sizeof(buffer),0);
                    printf("receive the command's len = %d\n",len);
                    if (len > 0)
                    {
                        //处理服务端发过来的命令上下左右   
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
                }//FD_ISSET,we receive the command

            case SENDCOMMAND:
                srand((unsigned) time(NULL)); //用时间做种，每次产生随机数不一样
                number = rand() % 12; //产生0-11的随机数,total 12 commands 0-11
                len = send(sockfd,sendbuffer[number].sendcommand,strlen(sendbuffer[number].sendcommand),0);
                if (len < 0)
                {
                    printf("failed to send heartbeat !\n");//in this case, we also add keepflag by 1,to switch the state to START
                }
                else
                {
                    printf("we send the command's length is %d\n",len);
                }

                state = PROCESSING;
                state_processing_c =0;
                state_processing_fail =0;
                break;

            default:
                state = START;
                break;
        }//switch(state)
    }//while(1)
    exit(0);
}

//////// 0 stand success
//////// 1 stand fail(the argc is wrong, socket error, connect error)
//int main(void)

int main(int argc,char *argv[])
{     
        struct threadpool *pool = threadpool_init(8, 10);
        threadpool_add_job(pool, work, "1");
 /*     threadpool_add_job(pool, work, "2");
        threadpool_add_job(pool, work, "3");
        threadpool_add_job(pool, work, "4");
        threadpool_add_job(pool, work, "5");
        threadpool_add_job(pool, work, "6");
        threadpool_add_job(pool, work, "7");
        threadpool_add_job(pool, work, "8");
*/

    sleep(10);
    threadpool_destroy(pool);
    return 0;
}



login_app(int fd,int y)
{
    int sockfd = fd;
    char checksum = 0x00; //校验和
    char command[40]; //登录的命令长度是36
    char copy[] = {0xF1,0x01,0x24};
    int j;
    int m;
    bzero(command,40);
    memcpy(command,copy,3);
    memcpy(&command[3],server[y-1].name,16);
    memcpy(&command[19],server[y-1].passwd,16);
//    sprintf(command,"%c%c%c%-16s%-16s",0xF1,0x01,0x24,server[num++].name,server[num++].passwd);
    for(j=0;j<35;j++)
        checksum += command[j];
    command[35] = checksum;
    command[36] = '\0';

    m = send(sockfd, command, 36, 0);
        printf("m= %d\n",m);
    recv(sockfd, command, 5, 0);
    if( command[3] == 0x01 )
        return 1;
    else
        printf("The user or passwd is Wrong board%d have login!\n",y);
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
                    if(buffer[1] == (char)0x02)
                    {
                        switch(buffer[3])
                        {
                            case (char)0x01:
                                printf("HA HA HA the server is alive\n");
                                break;
                        }
                    }         
                    else if(buffer[1] == (char)0x03)
                    {
                        switch(buffer[3])
                        {
                            case (char)0x01:
                                printf("the APP is login out!\n");
                                break;
                        }
                    }
                    else if(buffer[1] == (char)0x10)
                    {
                        switch(buffer[3])
                        {
                            case (char)0x00:
                                printf("pull the stream is failed\n" );
                                break;
                            default:
                                printf("pull the stream is SUCCESS\n");
                                break;
                        }   
                    }//stop the camera's up down left right and so on
                    else if(buffer[1] == (char)0x11)
                    {
                        switch(buffer[3])
                        {
                            case (char)0x01:
                                printf("make the camera UP is SUCCESS\n");
                                break;
                            case (char)0xF1:
                                printf("make the camera UP is FAILED\n");
                                break;
                            case (char)0x02:
                                printf("make the camera DOWN is SUCCESS\n");
                                break;
                            case (char)0xF2:
                                printf("make the camera DOWN is FAILED\n");
                                break;
                            case (char)0x03:
                                printf("make the camera LEFT is SUCCESS\n");
                                break;
                            case (char)0xF3:
                                printf("make the camera LEFT is FAILED\n");
                                break;
                            case (char)0x04:
                                printf("make the camera RIGHT is SUCCESS\n");
                                break;
                            case (char)0xF4:
                                printf("make the camera RIGHT is FAILED\n");
                                break;
                        }
                    }//add the keepalive
                    else if(buffer[1] == (char)0x12)
                    {
                        switch(buffer[3])
                        {
                            case (char)0x01:
                                printf("make the camera ZOOMLONG is SUCCESS\n");
                                break;
                            case (char)0xF1:
                                printf("make the camera ZOOMLONG is FAILED\n");
                                break;
                            case (char)0x02:
                                printf("make the camera ZOOMSHORT is SUCCESS\n");
                                break;
                            case (char)0xF2:
                                printf("make the camera ZOOMSHORT is FAILED\n");
                                break;
                        }
                    }
                    else if (buffer[1] == (char)0x13)
                    {
                        switch(buffer[3])
                        {
                            case (char)0x01:
                                printf("make the camera FOCUSCLOSE is SUCCESS\n");
                                break;
                            case (char)0xF1:
                                printf("make the camera FOCUSCLOSE is FAILED\n");
                                break;
                            case (char)0x02:
                                printf("make the camera FOCUSLONG is SUCCESS\n");
                                break;
                            case (char)0xF2:
                                printf("make the camera FOCUSLONG is FAILED\n");
                                break;
                        }
                    }
                    else if (buffer[1] == (char)0x15)
                    {
                        printf("stop the make the camera\n");
                    }
                    else if(buffer[1] == (char)0x16)
                    { 
                        printf("stop the stream !\n");
                    }
                    else if(buffer[1] == (char)0x17)
                    {
                        switch(buffer[3])
                        {
                            case (char)0x00:
                                printf("the camera NOT online!\n");
                                break;
                            case (char)0x01:
                                printf("the camera is online!\n");
                                break;
                        }
                    }             
                    return 0;
}