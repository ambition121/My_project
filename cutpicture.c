#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
 
#include <getopt.h>           
 
#include <fcntl.h>            
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
 
#include <asm/types.h>        
#include <linux/videodev2.h>
 
#define CLEAR(x) memset (&(x), 0, sizeof (x))
 
struct buffer {
        void *                  start;
        size_t                  length;
};
 
static char *           dev_name        = "/dev/video0";//摄像头设备名
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480
#define VIDEO_FORMAT V4L2_PIX_FMT_MJPEG
                   //V4L2_PIX_FMT_JPEG
                   //V4L2_PIX_FMT_YUYV
                   //V4L2_PIX_FMT_YVU420
                   //V4L2_PIX_FMT_RGB32


FILE *file_fd;
#define CAPTURE_FILE "test.jpg"
static unsigned long file_length;

//////////////////////////////////////////////////////
//获取一帧数据
//从视频缓冲区的输出队列中取得一个已经保存有一帧视频数据的视频缓冲区
//////////////////////////////////////////////////////
static int read_frame (void)
{
     struct v4l2_buffer buf;
     CLEAR (buf);
     buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     buf.memory = V4L2_MEMORY_MMAP;
     if(ioctl (fd, VIDIOC_DQBUF, &buf) == -1)
       {
          printf("VIDIOC_DQBUF failture\n"); //出列采集的帧缓冲
          exit(1);
       }

     assert (buf.index < n_buffers);
   //  printf ("buf.index dq is %d,\n",buf.index);
 
  //将其写入文件中
     fwrite(buffers[buf.index].start, buffers[buf.index].length, 1, file_fd);
      printf("Capture one frame saved in %s\n", CAPTURE_FILE);
     //再将其入列
     if(ioctl (fd, VIDIOC_QBUF, &buf)<0)
     printf("failture VIDIOC_QBUF\n");
     return 1;
}
 
int main (int argc,char ** argv)
{
     file_fd = fopen(CAPTURE_FILE, "w");//图片文件名

     //打开设备,获取设备的文件描述符
     fd = open (dev_name, O_RDWR | O_NONBLOCK, 0);
     if(fd < 0)
       {
          printf("open %s failed\n",dev_name);
          exit(1);
       }

      //获取驱动信息//获取摄像头参数//查询驱动功能并打印
      struct v4l2_capability cap;
  
      if(ioctl (fd, VIDIOC_QUERYCAP, &cap) < 0)//VIDIOC_QUERYCAP,检查当前视频设备属性
        {
          printf("get vidieo capability error,error code: %d \n", errno);
          exit(1);
        }
       // Print capability infomations
          printf("\nCapability Informations:\n");
          printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\nCapabilities: X\n",
                      cap.driver,cap.card,cap.bus_info,
                      (cap.version>>16)&0XFF, (cap.version>>8)&0XFF,cap.version&0XFF,
                      cap.capabilities );
       //获取设备支持的视频格式
       struct v4l2_fmtdesc fmtdesc;     
       CLEAR (fmtdesc);
       fmtdesc.index = 0;
       fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("\nSupport format:\n");
       while ((ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0)//VIDIOC_ENUM_FMT获取当前视频支持的视频格式struct v4l2_fmtdesc
             {
                  printf("\t%d.\n{\npixelformat = '%c%c%c%c',\ndescription = '%s'\n }\n",
                  fmtdesc.index+1,
                  fmtdesc.pixelformat & 0xFF,
                  (fmtdesc.pixelformat >> 8) & 0xFF,
                  (fmtdesc.pixelformat >> 16) & 0xFF,
                  (fmtdesc.pixelformat >> 24) & 0xFF,
                  fmtdesc.description);  

                 fmtdesc.index++;
              }  
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
     {
                   fprintf (stderr, "%s is no video capture device\n", dev_name);
                   exit (EXIT_FAILURE);
              }
///////////取景窗口缩放/有的摄像头不支持/////////////////////////////////////////////////////////////////////////
 

                
   //检查是否支持某种帧格式   
/*   struct v4l2_format fmt2;
   fmt2.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;   
   fmt2.fmt.pix.pixelformat = VIDEO_FORMAT;

   if(ioctl(fd,VIDIOC_TRY_FMT,&fmt2)==-1)  
           if(errno==EINVAL)
           printf("not support format %s!\n",VIDEO_FORMAT);   
*/
     //设置视频捕获格式
     struct v4l2_format fmt;
     CLEAR (fmt);
     fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     fmt.fmt.pix.width       = 320;
     fmt.fmt.pix.height      = 240;
     fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
//	 fmt.fmt.pix.pixelformat = VIDEO_FORMAT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    //设置图像格式
     if(ioctl (fd, VIDIOC_S_FMT, &fmt) < 0)
       {
          printf("failture VIDIOC_S_FMT\n");
          exit(1);
       }
  ///////////////////////////////////////////////////////////////////
       

       
  
///////////////////////////////////////////////////////////////////////
    // 显示当前帧的相关信息
     struct v4l2_format fmt3;
     fmt3.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
     ioctl(fd,VIDIOC_G_FMT,&fmt3); 
     printf("\nCurrent data format information:\n twidth:%d\n theight:%d\n", fmt3.fmt.pix.width,fmt3.fmt.pix.height);

       struct v4l2_fmtdesc fmtdes;
       fmtdes.index=0;
       fmtdes.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
       while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdes)!=-1)
       {
         if(fmtdes.pixelformat & fmt3.fmt.pix.pixelformat)
          {
            printf(" tformat:%s\n",fmtdes.description);
            break;
          }
          fmtdes.index++;
       }
///////////////////////////////////////////////////////////////////////
     //视频分配捕获内存
     struct v4l2_requestbuffers req;
     CLEAR (req);
     req.count               = 4;
     req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     req.memory              = V4L2_MEMORY_MMAP;
 

	 //申请缓冲，count是申请的数量位于内核空间
     if(ioctl (fd, VIDIOC_REQBUFS, &req) < 0)
       {
         printf("failture VIDIOC_REQBUFS\n");
         exit(1);
        }
     if (req.count < 2)
        printf("Insufficient buffer memory\n");

      //内存中建立对应空间
      //获取缓冲帧的地址、长度
       buffers = calloc (req.count, sizeof (*buffers));//在内存的动态存储区中分配req.count个长度为size的连续空间，函数返回一个指向分配起始地址的指针
     if (!buffers)
       {
          fprintf (stderr, "Out of memory/n");
          exit (EXIT_FAILURE);
        }
  
     for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
         {
           struct v4l2_buffer buf;   //驱动中的一帧
           CLEAR (buf);
           buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
           buf.memory      = V4L2_MEMORY_MMAP;
           buf.index       = n_buffers;// 要获取内核视频缓冲区的信息编号
 
           if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buf)) //VIDIOC_QUERYBUF查询已经分配的V4L2的视频缓冲区的相关信息，包括视频缓冲区的使用状态、在内核空间的偏移地址、缓冲区长度等
              {
                printf ("VIDIOC_QUERYBUF error\n");
                exit(-1);
              }
               buffers[n_buffers].length = buf.length; 


     // 把内核空间缓冲区映射到用户空间缓冲区
       buffers[n_buffers].start = mmap (NULL ,buf.length,PROT_READ | PROT_WRITE , MAP_SHARED ,fd,buf.m.offset);  //通过mmap建立映射关系
 
          if (MAP_FAILED == buffers[n_buffers].start)
             {
               printf ("mmap failed\n");
            exit(1);
             }
        }



//投放一个空的视频缓冲区到视频缓冲区输入队列中
//把四个缓冲帧放入队列，并启动数据流
        unsigned int i;
    // 将缓冲帧放入队列
       enum v4l2_buf_type type;
       for (i = 0; i < n_buffers; ++i)
           {
               struct v4l2_buffer buf;
               CLEAR (buf);
               buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               buf.memory      = V4L2_MEMORY_MMAP;
               buf.index       = i; //指定要投放到视频输入队列中的内核空间视频缓冲区的编号;
 
               if (-1 == ioctl (fd, VIDIOC_QBUF, &buf))//申请到的缓冲进入列队
               printf ("VIDIOC_QBUF failed\n");
            }



    //开始捕捉图像数据  
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl (fd, VIDIOC_STREAMON, &type))  //VIDIOC_STREAMON启动采集命令
       {
          printf ("VIDIOC_STREAMON failed\n");
          exit(1);
       }

/////////////////////////////////////////////////////////////////////////
 for (;;) //这一段涉及到异步IO
 {
   fd_set fds;
   struct timeval tv;
   int r;
 
   FD_ZERO (&fds);//将指定的文件描述符集清空
   FD_SET (fd, &fds);//在文件描述符集合中增加一个新的文件描述符
 
  
   tv.tv_sec = 2;
   tv.tv_usec = 0;
 
   r = select (fd + 1, &fds, NULL, NULL, &tv);//判断是否可读（即摄像头是否准备好），tv是定时

   if (-1 == r)
    {
      if (EINTR == errno)
      continue;
      printf ("select err\n");
    }
   if (0 == r)
    {
      fprintf (stderr, "select timeout\n");
      exit (EXIT_FAILURE);
    }
   if (read_frame ())//如果可读，执行read_frame ()函数，并跳出循环
   break;
 }
 /////// Release the resource////////////////////////////////////////////////////  
   unsigned int ii;
    for (ii = 0; ii < n_buffers; ++ii)
   if (-1 == munmap (buffers[ii].start, buffers[ii].length))   
       free (buffers);
 
 /////////////////////////////////////////////////////////

close (fd);
printf("Camera test Done.\n");
fclose (file_fd);
//exit (EXIT_SUCCESS);
return 0;
}
