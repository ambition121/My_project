#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#define MAXBUF 1024


int main(int argc, char **argv)
{
  int sockfd, len;
  struct sockaddr_in dest;
  char buffer[MAXBUF];
  char heartbeat[20] = "hello server";
  fd_set rfds;
  struct timeval tv;
  int retval, maxfd = -1;

  if (argc != 3)
  {
    printf("error! the right format should be : \
          \n\t\t%s IP port\n\t eg:\t%s127.0.0.1 80\n",
          argv[0], argv[0]);
    exit(0);
  }

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("Socket");
    exit(errno);
  }

  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(atoi(argv[2]));
  memset(&(dest.sin_zero), 0, 8);
  if (inet_aton(argv[1], (struct in_addr*)&dest.sin_addr.s_addr) == 0)
  {
    perror(argv[1]);
    exit(errno);
  }

  if (connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0)
  {
    perror("Connect");
    exit(errno);
  }
  printf("\nReady to start chatting.\n\
        Direct input messages and \n\
        enter to send messages to the server\n");

  while (1)
  {
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    maxfd = 0;

    FD_SET(sockfd, &rfds);
    if (sockfd > maxfd)
      maxfd = sockfd;

    tv.tv_sec = 2;
    tv.tv_usec = 0;

    retval = select(maxfd+1, &rfds, NULL, NULL, &tv);
    if (retval == -1)
    {
      printf("Will exit and the select is error! %s", strerror(errno));
      break;
    }
    else if (retval == 0)
    {
      //printf("No message comes, no buttons, continue to wait ...\n");
      len = send(sockfd, heartbeat, strlen(heartbeat), 0);
      if (len < 0)
      {
        printf("Message '%s' failed to send ! \
              The error code is %d, error message '%s'\n",
              heartbeat, errno, strerror(errno));
        break;
      }
      else
      {
        printf("News: %s \t send, sent a total of %d bytes!\n",
              heartbeat, len);
      }

      continue;
    }
    else
    {
      if (FD_ISSET(sockfd, &rfds))
      {
        bzero(buffer, MAXBUF+1);
        len = recv(sockfd, buffer, MAXBUF, 0);


        if (len > 0)
        {
          printf("Successfully received the message: '%s',%d bytes of data\n",
                  buffer, len);
        }
        else
        {
          if (len < 0)
              printf("Failed to receive the message! \
                    The error code is %d, error message is '%s'\n",
                    errno, strerror(errno));
          else
              printf("Chat to terminate!\n");


          break;
        }
      }


      if (FD_ISSET(0, &rfds))
      {
        bzero(buffer, MAXBUF+1);
        fgets(buffer, MAXBUF, stdin);


        if (!strncasecmp(buffer, "quit", 4))
        {
          printf("Own request to terminate the chat!\n");
          break;
        }


        len = send(sockfd, buffer, strlen(buffer)-1, 0);
        if (len < 0)
        {
          printf("Message '%s' failed to send ! \
                The error code is %d, error message '%s'\n",
                buffer, errno, strerror(errno));
          break;
        }
        else
        {
          printf("News: %s \t send, sent a total of %d bytes!\n",
                buffer, len);
        }
      }
    }
  }


  close(sockfd);
  return 0;
}
