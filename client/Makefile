all:
	gcc -o watchdog watchdog.c -lpthread
	gcc  -g gst-start-stream.c `pkg-config --cflags --libs gstreamer-0.10` -o videopush.out -lpthread
