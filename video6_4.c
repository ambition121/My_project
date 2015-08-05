#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>   
#include <assert.h>   
#include <getopt.h>             /* getopt_long() */   
#include <fcntl.h>              /* low-level i/o */   
#include <unistd.h>   
#include <errno.h>   
#include <malloc.h>   
#include <sys/stat.h>   
#include <sys/types.h>   
#include <sys/time.h>   
#include <sys/mman.h>   
#include <sys/ioctl.h>   
#include <asm/types.h>          /* for videodev2.h */   
#include <linux/videodev2.h>   
#define CLEAR(x) memset (&(x), 0, sizeof (x)) //定义一个初始化的函数  
typedef   enum  {  
    IO_METHOD_READ, 
	IO_METHOD_MMAP, 
	IO_METHOD_USERPTR,  
	} io_method;  
struct  buffer {  
    void  * start;  
    size_t  length; //buffer's length is different from cap_image_size   
};  
//static   char  * dev_name = NULL;  
static  char *     dev_name = "/dev/video0";
static  io_method io = IO_METHOD_MMAP; //IO_METHOD_READ;//IO_METHOD_MMAP;   
static   int  fd = -1;  
struct  buffer * buffers = NULL;  
static  unsigned  int  n_buffers = 0;  
static   FILE  * outf = 0;  
static  unsigned  int  cap_image_size = 0; //to keep the real image size!! 

static   void  open_device( void );
static   void  init_device( void );
static   void  init_mmap( void ) ;
static   void  start_capturing( void );
static   void  mainloop( void );
static   int  read_frame( void );
static   void  process_image( const   void  * p,  int  len);
static   void  errno_exit( const   char  * s);
static   void  stop_capturing( void );
static   void  uninit_device( void );
static   void  close_device( void );

static   void  open_device( void ) {     //打开一个设备 
    struct  stat st;  
    if  (-1 == stat(dev_name, &st)) {  
        fprintf(stderr, "Cannot identify '%s': %d, %s\n" , dev_name, errno,  
                strerror(errno));  
        exit(EXIT_FAILURE);  
    }  
    if  (!S_ISCHR(st.st_mode)) {  
        fprintf(stderr, "%s is no device\n" , dev_name);  
        exit(EXIT_FAILURE);  
    }  
    fd = open("/dev/video0", O_RDWR /* required */ | O_NONBLOCK, 0); //以非阻塞方式打开 
    if  (-1 == fd) {  
        fprintf(stderr, "Cannot open '%s': %d, %s\n" , dev_name, errno,  
                strerror(errno));  
        exit(EXIT_FAILURE);  
    }  
}


static   void  init_device( void ) {      //取得设备的capability，看看设备具有什么样 的功能
    struct  v4l2_capability cap;  //获取设备的一些功能
    struct  v4l2_cropcap cropcap; //设置设备的一些格式，比如摄像头捕获的左上角坐标和高度等
    struct  v4l2_crop crop;      //设置设备的缩放，有些设备不支持
    struct  v4l2_format fmt;	//设置图片或视频的width和height等 
    unsigned int  min;  
	if  (-1 == ioctl(fd,VIDIOC_QUERYCAP, &cap))   //VIDIOC_QUERYCAP查询视频设备的一些功能 
   {  
        if  (EINVAL == errno)
       {  
            fprintf(stderr, "%s is no V4L2 device\n" , dev_name);  
            exit(EXIT_FAILURE);  
        }
        else
       {  
            errno_exit("VIDIOC_QUERYCAP" );  
        }  
    }  

	if  (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
   {  
        fprintf(stderr, "%s is no video capture device\n" , dev_name);  
        exit(EXIT_FAILURE);  
    }  
    
    if  (!(cap.capabilities & V4L2_CAP_STREAMING))
   {  
            fprintf(stderr, "%s does not support streaming i/o\n" , dev_name);  
            exit(EXIT_FAILURE);  
    }  
      
    //////not all capture support crop!!!!!!!   
    /* Select video input, video standard and tune here. */   
    printf("-#-#-#-#-#-#-#-#-#-#-#-#-#-\n" );  
    CLEAR (cropcap);  //struct v4l2_cropcap
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
	  if  (0 == ioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
#ifndef CROP_BY_JACK   
        crop.c = cropcap.defrect; /* reset to default *///defrect默认的摄像头的捕捉范围   
#else   
        crop.c.left = cropcap.defrect.left;  
        crop.c.top = cropcap.defrect.top;  
        crop.c.width = 352;  
 		crop.c.height = 288;
#endif   
        printf("----->has ability to crop!!\n" );  
        printf("cropcap.defrect = (%d, %d, %d, %d)\n" , cropcap.defrect.left, cropcap.defrect.top, cropcap.defrect.width, cropcap.defrect.height);  
		if  (-1 == ioctl(fd, VIDIOC_S_CROP, &crop)) {
            switch  (errno) {  
            case  EINVAL:  
                /* Cropping not supported. */   
                break ;  
            default :  
                /* Errors ignored. */   
                break ;  
            }  
            printf("-----!!but crop to (%d, %d, %d, %d) Failed!!\n" ,  
                    crop.c.left, crop.c.top, crop.c.width, crop.c.height);  
        } else  {  
            printf("----->sussess crop to (%d, %d, %d, %d)\n" , crop.c.left,  
                    crop.c.top, crop.c.width, crop.c.height);  
        }  
    } else  {  
        /* Errors ignored. */   
        printf("!! has no ability to crop!!\n" );  
    }																		
    printf("-#-#-#-#-#-#-#-#-#-#-#-#-#-\n" );  
    printf("/n" );  
    ////////////crop finished!   
    //////////set the format   
    CLEAR (fmt);  //这是我们设置的捕捉视频的格式等
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    fmt.fmt.pix.width = 640;  
    fmt.fmt.pix.height = 480;  
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//V4L2_PIX_FMT_YUV420;//V4L2_PIX_FMT_YUYV;   
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED; //包含top和bottom field，隔行交替，场序依赖于当前video的标准是PAL还是NTSC，PAL中先传top场，NTSC则向反 
    {  
        printf("-#-#-#-#-#-#-#-#-#-#-#-#-#-\n" );  
        printf("=====will set fmt to (%d, %d)--" , fmt.fmt.pix.width,  
                fmt.fmt.pix.height);  
        if  (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {  
            printf("V4L2_PIX_FMT_YUYV\n" );  
        } else   if  (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420) {  
            printf("V4L2_PIX_FMT_YUV420\n" );  
        } else   if  (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12) {  
            printf("V4L2_PIX_FMT_NV12\n" );  
        }  
    }  
		if  (-1 == ioctl(fd, VIDIOC_S_FMT, &fmt))
        errno_exit("VIDIOC_S_FMT" );  
    {  
        printf("=====after set fmt\n" );  
        printf("    fmt.fmt.pix.width = %d\n" , fmt.fmt.pix.width);  
        printf("    fmt.fmt.pix.height = %d\n" , fmt.fmt.pix.height);  
        printf("    fmt.fmt.pix.sizeimage = %d\n" , fmt.fmt.pix.sizeimage);  
        cap_image_size = fmt.fmt.pix.sizeimage;  
        printf("    fmt.fmt.pix.bytesperline = %d\n" , fmt.fmt.pix.bytesperline);  
        printf("-#-#-#-#-#-#-#-#-#-#-#-#-#-\n" );  
        printf("\n" );  
    }  
    cap_image_size = fmt.fmt.pix.sizeimage;  
    /* Note VIDIOC_S_FMT may change width and height. */   
    printf("-#-#-#-#-#-#-#-#-#-#-#-#-#-\n" );  
    /* Buggy driver paranoia. */   
    min = fmt.fmt.pix.width * 2;  
    if  (fmt.fmt.pix.bytesperline < min)  
        fmt.fmt.pix.bytesperline = min;  
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;  
    if  (fmt.fmt.pix.sizeimage < min)  
        fmt.fmt.pix.sizeimage = min;  
    printf("After Buggy driver paranoia\n" );  
    printf("    >>fmt.fmt.pix.sizeimage = %d\n" , fmt.fmt.pix.sizeimage);  
    printf("    >>fmt.fmt.pix.bytesperline = %d\n" , fmt.fmt.pix.bytesperline);  
    printf("-#-#-#-#-#-#-#-#-#-#-#-#-#-\n" );  
    printf("\n" );  
    
   
    init_mmap();  
      
}

static   void  init_mmap( void )
{  
    struct  v4l2_requestbuffers req;  
    CLEAR (req);  
    req.count = 4;  
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    req.memory = V4L2_MEMORY_MMAP;  
		if  (-1 == ioctl(fd, VIDIOC_REQBUFS, &req)) { 
        if  (EINVAL == errno) {  
            fprintf(stderr, "%s does not support "   
                "memory mapping\n" , dev_name);  
            exit(EXIT_FAILURE);  
        } else  {  
            errno_exit("VIDIOC_REQBUFS" );  
        }  
    }  
    if  (req.count < 2) {  
        fprintf(stderr, "Insufficient buffer memory on %s\n" , dev_name);  
        exit(EXIT_FAILURE);  
    }  
    buffers = calloc(req.count, sizeof (*buffers));  
    if  (!buffers) {  
        fprintf(stderr, "Out of memory\n" );  
        exit(EXIT_FAILURE);  
    }  
    for  (n_buffers = 0; n_buffers < req.count; ++n_buffers) {  
        struct  v4l2_buffer buf;  
        CLEAR (buf);  
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        buf.memory = V4L2_MEMORY_MMAP;  
        buf.index = n_buffers;  
		if  (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF" );  
        buffers[n_buffers].length = buf.length;  
        buffers[n_buffers].start = mmap(NULL /* start anywhere */ , buf.length,  
                PROT_READ | PROT_WRITE /* required */ ,  
                MAP_SHARED /* recommended */ , fd, buf.m.offset);  
        if  (MAP_FAILED == buffers[n_buffers].start)  
            errno_exit("mmap" );  
    }  
}
static   void  start_capturing( void ) {  
    unsigned int  i;  
    enum  v4l2_buf_type type;  
    for  (i = 0; i < n_buffers; ++i)
      {  
            struct  v4l2_buffer buf;  
            CLEAR (buf);  
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
            buf.memory = V4L2_MEMORY_MMAP;  
            buf.index = i;  
			if  (-1 == ioctl(fd, VIDIOC_QBUF, &buf))
                errno_exit("VIDIOC_QBUF" );  
        }  
     type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
	 if  (-1 == ioctl(fd, VIDIOC_STREAMON, &type))
            errno_exit("VIDIOC_STREAMON" );  
      
} 

static   void  mainloop( void )
 {  
    unsigned int  count;  
    count = 100;  
    while  (count-- > 0)
    {  
        for  (;;)
        {  
          
            if  (read_frame())  
                break ;  
            /* EAGAIN - continue select loop. */   
        }  
    }  
}   

static   int  read_frame( void ) {  
    struct  v4l2_buffer buf;  
    unsigned int  i;  
  
   
        CLEAR (buf);  
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        buf.memory = V4L2_MEMORY_MMAP;  
		if  (-1 == ioctl(fd, VIDIOC_DQBUF, &buf)) {
            switch  (errno) {  
            case  EAGAIN:  
                return  0;  
            case  EIO:  
                /* Could ignore EIO, see spec. */   
                /* fall through */   
            default :  
                errno_exit("VIDIOC_DQBUF" );  
            }  
        }  
        assert(buf.index < n_buffers);  
        //      printf("length = %d/r", buffers[buf.index].length);   
        //      process_image(buffers[buf.index].start, buffers[buf.index].length);   
        printf("image_size = %d,\t IO_METHOD_MMAP buffer.length=%d\r" ,  
                cap_image_size, buffers[0].length);  
        process_image(buffers[0].start, cap_image_size);  
		if  (-1 == ioctl(fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF" );  
     
      
   
    return  1;  
}  
static   void  process_image( const   void  * p,  int  len) {  
    //  static char[115200] Outbuff ;   
    fputc('.' , stdout);  
    if  (len > 0) {  
        fputc('.' , stdout);  
        fwrite(p, 1, len, outf);  
    }  
    fflush(stdout);  
}
static   void  errno_exit( const   char  * s) {  
    fprintf(stderr, "%s error %d, %s\n" , s, errno, strerror(errno));  
    exit(EXIT_FAILURE);  
}  
//static   int  xioctl( int  fd,  int  request,  void  * arg) {  
static   int  xioctl( int  fd,  int  request,  void  * arg) {
    int  r;  
    do   
        r = ioctl(fd, request, arg);  
    while  (-1 == r && EINTR == errno);  
    return  r;  
}  
static   void  stop_capturing( void ) {  
    enum  v4l2_buf_type type;  
    
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        if  (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))  
            errno_exit("VIDIOC_STREAMOFF" );  
   
}  
static   void  uninit_device( void ) {  
    unsigned int  i;  
  
        for  (i = 0; i < n_buffers; ++i)  
            if  (-1 == munmap(buffers[i].start, buffers[i].length))  
                errno_exit("munmap" );  
      
    free(buffers);  
}  
static   void  close_device( void ) {  
    if  (-1 == close(fd))  
        errno_exit("close" );  
    fd = -1;  
}  
int main()
{
    dev_name = "/dev/video0" ;  
    outf = fopen("video6_4.yuv" ,  "w+" );  
    open_device();  
    init_device();  
    start_capturing();  
    mainloop();  
    printf("\n" );  
    stop_capturing();  
    fclose(outf);  
    uninit_device();  
    close_device();  
    exit(EXIT_SUCCESS);  
    return  0;  

}
