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
#include <dirent.h>
#include <fat.h>

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
  server.sin_port = htons (4321);
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

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void *
initialise ()
{
  void *framebuffer;

  VIDEO_Init();
  PAD_Init();

  rmode = VIDEO_GetPreferredMode(NULL);
  framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  console_init (framebuffer, 20, 20, rmode->fbWidth, rmode->xfbHeight,
		rmode->fbWidth * VI_DISPLAY_PIX_SZ);

  VIDEO_Configure (rmode);
  VIDEO_SetNextFramebuffer (framebuffer);
  //VIDEO_SetPostRetraceCallback (copy_to_xfb);
  VIDEO_SetBlack (FALSE);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();

  if (rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync ();

  return framebuffer;
}

static void
return_to_loader (void)
{
  void (*reload)() = (void(*)()) 0x80001800;
  srv_disconnect ();
  reload ();
}

int
main (int argc, char **argv)
{
  s32 ret;
  char command[1024];

  char localip[16] = {0};
  char gateway[16] = {0};
  char netmask[16] = {0};

  xfb = initialise();

  /* Hopefully this will trigger if we exit unexpectedly...  */
  atexit (return_to_loader);

  strcpy (localip, "192.168.2.254");
  strcpy (gateway, "192.168.2.251");
  strcpy (netmask, "192.168.2.248");

  // Configure the network interface
  ret = if_config (localip, netmask, gateway, FALSE);

  if (ret >= 0)
    printf ("network configured, ip: %s, gw: %s, mask %s\n", localip,
	    gateway, netmask);
  else
    printf ("network configuration failed!\n");

  printf ("waiting for connection...\n");
  srv_wait_for_connection ();
  printf ("got connection!\n");

  if (fatInitDefault ())
    srv_printf ("initialised libfat\n");
  else
    srv_printf ("fatInitDefault() failure\n");

  while (1)
    {
      ssize_t fetched = net_recv (connected_to, command, sizeof (command), 0);

      while (fetched > 0 && (command[fetched - 1] == '\r'
			     || command[fetched - 1] == '\n'))
	command[--fetched] = '\0';

      if (strcmp (command, "quit") == 0)
        break;
      else if (strcmp (command, "ls") == 0)
        {
	  DIR *dir = opendir ("/");
	  
	  while (1)
	    {
	      struct dirent *ent = readdir (dir);

	      if (!ent)
	        break;
	      
	      srv_printf ("%s\n", ent->d_name);
	    }
	  
	  closedir (dir);
	}
      else if (strncmp (command, "put", 3) == 0)
        {
	  unsigned int length;
	  char filename[1024];
	  //char buf[4096];
	  FILE *fh;
	  
	  sscanf (command, "put %d %s", &length, filename);
	  
	  srv_printf ("putting file '%s' of length %d\n", filename, length);
	  
	  fh = fopen (filename, "w");
	  
	  fprintf (fh, "some contents\n");
	  
	  fclose (fh);

	  srv_printf ("done\n");
	}
      else if (strncmp (command, "rm", 2) == 0)
        {
	  char filename[1024];
	  int ret;
	  
	  sscanf (command, "rm %s", filename);
	  
	  ret = remove (filename);
	  
	  if (ret == 0)
	    printf ("successfully removed '%s'\n", filename);
	  else
	    printf ("couldn't remove '%s'\n", filename);
	}
    }
  
  fatUnmount ("/");

  return_to_loader ();

  return 0;
}
