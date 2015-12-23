/* GStreamer use the socket to commit
 * Copyright (C) 2009 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <gst/gst.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

#define UDP_TEST_PORT   9800 //port is 9800
/*
 * A simple RTP server 
 *  sends the output of alsasrc as alaw encoded RTP on port 5002, RTCP is sent on
 *  port 5003. The destination is 127.0.0.1.
 *  the receiver RTCP reports are received on port 5007
 *
 * .-------.    .-------.    .-------.      .----------.     .-------.
 * |alsasrc|    |alawenc|    |pcmapay|      | rtpbin   |     |udpsink|  RTP
 * |      src->sink    src->sink    src->send_rtp send_rtp->sink     | port=5002
 * '-------'    '-------'    '-------'      |          |     '-------'
 *                                          |          |      
 *                                          |          |     .-------.
 *                                          |          |     |udpsink|  RTCP
 *                                          |    send_rtcp->sink     | port=5003
 *                           .-------.      |          |     '-------' sync=false
 *                RTCP       |udpsrc |      |          |               async=false
 *              port=5007    |     src->recv_rtcp      |                       
 *                           '-------'      '----------'              
 */

/* change this to send the RTP data and RTCP to another host */
#define DEST_HOST "123.57.250.247"
//extern FILE* stream_record_fd;
static GstElement *g_pipeline;

//static int push_timing=0;
/* print the stats of a source */
static void
print_source_stats (GObject * source)
{
  GstStructure *stats;
  gchar *str;

  /* get the source stats */
  g_object_get (source, "stats", &stats, NULL);

  /* simply dump the stats structure */
  str = gst_structure_to_string (stats);
  g_print ("source stats: %s\n", str);

  gst_structure_free (stats);
  g_free (str);
}

/* this function is called every second and dumps the RTP manager stats */
static gboolean
print_stats (GstElement * rtpbin)
{
  GObject *session;
  GValueArray *arr;
  GValue *val;
  guint i;

  g_print ("***********************************\n");
 
/* 
  printf("push_timeing is:%d\n",push_timing++);
  push_timing++;
  if(push_timing>60)
  	gst_element_set_state(g_pipeline,GST_STATE_NULL);//we force it to stop when the timing of videopushing is larger than 600 seconds
*/

  /* get session 0 */
  g_signal_emit_by_name (rtpbin, "get-internal-session", 0, &session);

  /* print all the sources in the session, this includes the internal source */
  g_object_get (session, "sources", &arr, NULL);

  for (i = 0; i < arr->n_values; i++) {
    GObject *source;

    val = g_value_array_get_nth (arr, i);
    source = g_value_get_object (val);

    print_source_stats (source);
  }
  g_value_array_free (arr);

  g_object_unref (session);

  return TRUE;
}

gboolean file_watch_fun(GIOChannel *source, GIOCondition condition, void *data)
{
	gchar *str;
	gsize byte_read;
        printf("file_watch_fun is called\n"); 
	if(g_io_channel_read_line(source,&str,&byte_read,NULL,NULL)==G_IO_STATUS_NORMAL){

		if(str[0]=='s'){
			gst_element_set_state(g_pipeline,GST_STATE_NULL);//GST_STATE_PLAYING or GST_STATE_PAUSED
			printf("set the gst state to NULL to stop the stream\n");
		}
    if(str[0]=='r'){
 //   	push_timing=0;
      gst_element_set_state(g_pipeline,GST_STATE_PLAYING);
      printf("set the gst state to playing \n");
    }
    else
      printf("other characters,ingonring\n");
		
		return TRUE;
	}
}


/* build a pipeline equivalent to:
 *
 * gst-launch -v gstrtpbin name=rtpbin \
 *    $AUDIO_SRC ! audioconvert ! audioresample ! $AUDIO_ENC ! $AUDIO_PAY ! rtpbin.send_rtp_sink_0  \
 *           rtpbin.send_rtp_src_0 ! udpsink port=5002 host=$DEST                      \
 *           rtpbin.send_rtcp_src_0 ! udpsink port=5003 host=$DEST sync=false async=false \
 *        udpsrc port=5007 ! rtpbin.recv_rtcp_sink_0
 */
 /*
 sudo gst-launch gstrtpbin name=rtpbin latency=50 v4l2src device=/dev/video3  
 sensor_name="adv7280m_a 3-0020"  inputnum=1 brightnessex=30 framerate=30 name=v4l2src1 saturationex=20 ! 
 video/x-raw-yuv,width=640,height=480,framerate=30/1 ! queue ! ducatih264enc bitrate=1024 ! queue ! 
 rtph264pay pt=96 ! rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 ! 
 udpsink name=udpsink1 host=123.57.250.247 port=8554 async=true  rtpbin.send_rtcp_src_0 ! queue2 ! 
 udpsink name=udpsink2 host=123.57.250.247 port=8555 async=true udpsrc port=5005 ! rtpbin.recv_rtcp_sink_0 > /dev/null

 */
 
/*arguments:
fd: is used to receive command to stop the pipeline when the pipeline is running
*/ 
int
main (int argc,char* argv[])
{
  GstElement *videosrc;
  GstElement *filter,*videoenc,*videopay;
  GstCaps * caps;
  GIOChannel *input;
  GstElement *rtpbin, *rtpsink, *rtcpsink, *rtcpsrc;
  
  GMainLoop *loop;
  GstPad *srcpad, *sinkpad;
  int sock;
  char buffer[10];
  int len;
  int index;
  int udpport;
  struct sockaddr_in servaddr; 
  int addr_len=sizeof(struct sockaddr_in);
 
  /* always init first */
  sock = socket(AF_INET,SOCK_DGRAM,0);
  g_assert(sock>0);

  int SO_REUSEADDR_on = 1;  //process the TIME_WAIT
  setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &SO_REUSEADDR_on, sizeof(SO_REUSEADDR_on));

  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  //servaddr.sin_addr.s_addr = htonl("127.0.0.1"/*INADDR_ANY*/);
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY) ;
  //servaddr.sin_port = htonl(9800);
  servaddr.sin_port = htons(UDP_TEST_PORT);
  if(bind(sock,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
    printf("bind udp port:9800 error\n");
  }

  bzero(buffer,sizeof(buffer));
  len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&servaddr ,&addr_len);
  
  printf("$$$-receive the lentgth is %d, boardname is:%s\n",len,buffer);
  //now to calculate the udp port to which the video data is sent to
  index=atoi(&buffer[5]);//buffer:board1,or board2,or board3, or board4
  udpport=index*2+8552;//8554,or 8556, or 8558, or 8560
  
  gst_init (&argc, &argv);

  /* the pipeline to hold everything */
  g_pipeline = gst_pipeline_new (NULL);
  g_assert (g_pipeline);
  
  //set the iochannel
  input=g_io_channel_unix_new(sock);//create the iochannel
	g_io_channel_set_encoding(input,NULL,NULL); //setting data encoding
	g_io_add_watch(input,G_IO_IN | G_IO_PRI,(GIOFunc)file_watch_fun,NULL); //the specified event add to the event loop
  //There is data to read. There is urgent data to read. (file_watch_fun is callback function)

  /** the audio capture and format conversion 
  gst_element_factory_make == gst_element_factory_find and gst_element_factory_create
  create the factory object
  */
  videosrc = gst_element_factory_make ("v4l2src", "videosrc");
  g_assert (videosrc);
  
  g_object_set(G_OBJECT(videosrc),"device","/dev/video3",NULL);
	g_object_set(G_OBJECT(videosrc),"inputnum",1,NULL);
  g_object_set(G_OBJECT(videosrc),"sensor_name","adv7280m_a 3-0020",NULL);




	filter=gst_element_factory_make("capsfilter","filter");
	g_assert(filter);
	/*new caps */
	
	caps=gst_caps_new_simple("video/x-raw-yuv","width",G_TYPE_INT,640,"height",G_TYPE_INT,480,
	"framerate",GST_TYPE_FRACTION,30,1, "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('N','V','1','2'),NULL);
	
	/*set filter caps*/
	g_object_set(G_OBJECT(filter),"caps",caps,NULL);



	
  videoenc = gst_element_factory_make ("ducatih264enc", "the ti encoder");//create the element of video compression
  g_assert (videoenc);
  g_object_set(G_OBJECT(videoenc),"bitrate",1024,NULL);
 
  

  videopay = gst_element_factory_make ("rtph264pay", "videopay");
  g_assert (videopay);
   g_object_set(G_OBJECT(videopay),"pt",96,NULL);

  /** add capture and payloading to the pipeline and link 
    gst_bin_add_many--add the elements to the g_pipeling
    gst_element_link_many--link all the elements
  */
  gst_bin_add_many (GST_BIN (g_pipeline), videosrc, filter, videoenc,
      videopay, NULL);

  if (!gst_element_link_many (videosrc, filter, videoenc,
          videopay, NULL)) {
    g_error ("Failed to link videosrc, video encoder and video payloader");
  }




  /* the rtpbin element */
  rtpbin = gst_element_factory_make ("gstrtpbin", "rtpbin");
  g_assert (rtpbin);

  gst_bin_add (GST_BIN (g_pipeline), rtpbin);




  /* the udp sinks and source we will use for RTP and RTCP */
  rtpsink = gst_element_factory_make ("udpsink", "rtpsink");
  g_assert (rtpsink);
  g_object_set (rtpsink, "port", udpport, "host", DEST_HOST, NULL);

  rtcpsink = gst_element_factory_make ("udpsink", "rtcpsink");
  g_assert (rtcpsink);
  g_object_set (rtcpsink, "port", udpport+1, "host", DEST_HOST, NULL);
  /* no need for synchronisation or preroll on the RTCP sink */
  g_object_set (rtcpsink, "async", FALSE, "sync", FALSE, NULL);

  rtcpsrc = gst_element_factory_make ("udpsrc", "rtcpsrc");
  g_assert (rtcpsrc);
  g_object_set (rtcpsrc, "port", 5005, NULL);

  gst_bin_add_many (GST_BIN (g_pipeline), rtpsink, rtcpsink, rtcpsrc, NULL);




  /* now link all to the rtpbin, start by getting an RTP sinkpad for session 0 */
  sinkpad = gst_element_get_request_pad (rtpbin, "send_rtp_sink_0");
  srcpad = gst_element_get_static_pad (videopay, "src");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link audio payloader to rtpbin");
  gst_object_unref (srcpad);

  /* get the RTP srcpad that was created when we requested the sinkpad above and
   * link it to the rtpsink sinkpad*/
  srcpad = gst_element_get_static_pad (rtpbin, "send_rtp_src_0");
  sinkpad = gst_element_get_static_pad (rtpsink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link rtpbin to rtpsink");
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* get an RTCP srcpad for sending RTCP to the receiver */
  srcpad = gst_element_get_request_pad (rtpbin, "send_rtcp_src_0");
  sinkpad = gst_element_get_static_pad (rtcpsink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link rtpbin to rtcpsink");
  gst_object_unref (sinkpad);

  /* we also want to receive RTCP, request an RTCP sinkpad for session 0 and
   * link it to the srcpad of the udpsrc for RTCP */
  srcpad = gst_element_get_static_pad (rtcpsrc, "src");
  sinkpad = gst_element_get_request_pad (rtpbin, "recv_rtcp_sink_0");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link rtcpsrc to rtpbin");
  gst_object_unref (srcpad);

  /* set the pipeline to playing */
  g_print ("starting sender pipeline\n");
 // push_timing=0;
  gst_element_set_state (g_pipeline, GST_STATE_PLAYING);
  
  /* print stats every second */
  g_timeout_add (10000, (GSourceFunc) print_stats, rtpbin);

  /* we need to run a GLib main loop to get the messages */
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_print ("stopping sender pipeline\n");
  gst_element_set_state (g_pipeline, GST_STATE_NULL);
  
	g_io_channel_shutdown(input,TRUE,NULL);
	g_io_channel_unref(input);
	g_main_loop_unref(loop);
  return 0;
}

//int gst_stop_stream(int fd)
//{
//	write(fd,"s\n",2);//s:representing for stopping
//}

