#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <network.h>
#include <debug.h>
#include <errno.h>
#include <unistd.h>

static volatile int net_connected = 0;
static volatile s32 connected_to;

int
srv_wait_for_connection (void)
{
  s32 sock, csock;
  int ret;
  u32 clientlen;
  struct sockaddr_in client;
  struct sockaddr_in server;

  clientlen = sizeof (client);

  sock = net_socket (AF_INET, SOCK_STREAM, IPPROTO_IP);

  if (sock == INVALID_SOCKET)
    {
      printf ("Cannot create a socket!\n");
      return 0;
    }

  memset (&server, 0, sizeof (server));
  memset (&client, 0, sizeof (client));

  server.sin_family = AF_INET;
  server.sin_port = htons (2000);
  server.sin_addr.s_addr = INADDR_ANY;
  ret = net_bind (sock, (struct sockaddr *) &server, sizeof (server));

  if (ret)
    {
      printf("Error %d binding socket!\n", ret);
      return 0;
    }

  if ( (ret = net_listen (sock, 5)) )
    {
      printf("Error %d listening!\n", ret);
      return 0;
    }

  csock = net_accept (sock, (struct sockaddr *) &client, &clientlen);

  if (csock < 0)
    {
      printf("Error connecting socket %d!\n", csock);
      return 0;
    }

  printf ("Connecting port %d from %s\n", client.sin_port,
	  inet_ntoa (client.sin_addr));

  net_connected = 1;
  connected_to = csock;

  return 1;
}

int
srv_printf (const char *fmt, ...)
{
  char temp[1026];
  va_list ap;
  int rc;
  
  if (net_connected)
    {
      va_start (ap, fmt);
      rc = vsnprintf (temp, sizeof (temp), fmt, ap);
      va_end (ap);

      net_send (connected_to, temp, strlen (temp), 0);

      return rc;
    }
  
  return 0;
}

void
srv_disconnect (void)
{
  if (net_connected)
    {
      net_close (connected_to);
      net_connected = 0;
    }
}

