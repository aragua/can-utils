/*
 * Author : Fabien Lahoudere (fabien.lahoudere@openwide.fr)
 *
 * Tested on host with:
 * sudo ip link set can0 up txqueuelen 1000 type can bitrate 500000
 * echo 'ahahahahahahahahohohohohohohohohohoohohoh' | ./host_cansend -i can0 -d 0x100 -l
 * cat <file> | ./host_cansend -i can0 -d 0x100 -l
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/param.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#define NO_CAN_ID 0xFFFFFFFFU

/* define how many frame to send each millisecond */
#define FPMS 10

static void Usage()
{
  fprintf( stderr, "\n");
  fprintf( stderr, "cansend [options]\n");
  fprintf( stderr, "\tOptions:\n");
  fprintf( stderr, "\t\t-i <interface>\tset the interface to listen\n");
  fprintf( stderr, "\t\t-c <count>\twait for count frames\n");
  fprintf( stderr, "\t\t-s <canid>\tset the source can id\n");
  fprintf( stderr, "\t\t-d <canid>\tset the destination can id\n");
  fprintf( stderr, "\t\t-t\tinterval between two write  \n");
  fprintf( stderr, "\t\t-v\tverbose mode\n");
  fprintf( stderr, "\t\t-p\tprint id\n");
  fprintf( stderr, "\t\t-z <protnbr>\tchange protocol number\n");
  fprintf( stderr, "\n");

  exit(-1);
}

int
main (int argc, char **argv)
{
  char *iface = "can0";
  int s, proto = CAN_RAW;			/* can raw socket */
  int count = 0, interval = 0, iter, wait = 0;
  int mtu;
  int can_id = 0;
  struct sockaddr_can addr;
  struct can_frame frame;
  struct ifreq ifr;
  unsigned long long data;
  int opt, verbose = 0;
  int ret = 0;

  addr.can_addr.tp.tx_id = addr.can_addr.tp.rx_id = NO_CAN_ID;

  while ((opt = getopt (argc, argv, "i:c:s:d:t:v")) != -1)
    {
      switch (opt)
	{
	case 'i':
	  iface = optarg;
	  break;
	case 'c':
	  count = atoi (optarg);
	  break;
	case 's':
	  addr.can_addr.tp.tx_id = strtoul (optarg, (char **) NULL, 16);
	  if ( addr.can_addr.tp.tx_id&0x1fffffff > 0x7ff)
	    addr.can_addr.tp.tx_id |= CAN_EFF_FLAG;
	  break;
	case 'd':
	  addr.can_addr.tp.rx_id = strtoul (optarg, (char **) NULL, 16);
	  if ( addr.can_addr.tp.rx_id&0x1fffffff > 0x7ff)
	    addr.can_addr.tp.rx_id |= CAN_EFF_FLAG;
	  break;
	case 't':
	  interval = atoi (optarg);
	  if (interval < 100)
	    interval = 100;
	  break;
        case 'z':
            proto = atoi(optarg);
            break;
	case 'v':
	  verbose = 1;
	  break;
	default:
	  Usage();
	  break;
	}
    }

  if (!iface)
    {
      fprintf (stderr, "No interface specified\n");
      Usage();
    }

  if (count > 0 && interval < 0 )
    interval = 0;

  /* open socket */
  if (verbose)
    printf ("[CANSEND]Open first socket");
  if ((s = socket (PF_CAN, SOCK_RAW, proto)) < 0)
    {
      perror ("socket");
      Usage();
    }
  if (verbose)
    printf (":socket num %d\n", s);

  addr.can_family = AF_CAN;
  strcpy (ifr.ifr_name, iface);
  if (ioctl (s, SIOCGIFINDEX, &ifr) < 0)
    {
      fprintf (stderr, "bad interface %s\n", iface);
      perror ("SIOCGIFINDEX");
      Usage();
    }
  addr.can_ifindex = ifr.ifr_ifindex;

  if (bind (s, (struct sockaddr *) &addr, sizeof (addr)) < 0)
    {
      perror ("bind");
      Usage();
    }

  frame.can_id = addr.can_addr.tp.rx_id;
  frame.can_dlc = 8;
  *((unsigned long long *) frame.data) = 0;

  if (count > 0)
    {
      /* send frame */
      for (iter = 0; iter < count; iter++)
	{
	  ret = write (s, &frame, sizeof (frame)); 
	  if ( ret != sizeof (frame) )
	    {
	      perror ("write");
	      return ret;
	    }
	  if (interval)
	    usleep (interval);
	  (*((unsigned long long *) frame.data))++;
	}
    }
  else
    {
      char buf[FPMS * 8];

      if (verbose)
	fprintf (stdout, "Ready to read\n");
      
      ret = 0;

      while ((ret = read (STDIN_FILENO, buf, sizeof (buf))) > 0)
	{
	  int idx;

	  if (verbose)
	    fprintf (stdout, "read %d bytes\n", ret);
	  for (idx = 0; idx < FPMS * 8 && ret > 0; idx += 8)
	    {
	      int str = MIN (ret, 8);
	      memcpy (frame.data, buf + idx, str);
	      frame.can_dlc = str;
	      ret -= str;
	      if (write (s, &frame, sizeof (frame)) != sizeof (frame))
		{
		  perror ("write");
                  return ret;
		}
	      if(interval)
		usleep (interval);
	      if (verbose)
		fprintf (stdout, "frame writen\n");
	    }
	  if (verbose)
	    fprintf (stdout, "Quit for loop ret = %d and idx = %d \n", ret,
		     idx);
	}
      if (verbose)
	fprintf (stdout, "Quit read loop\n");
    }
/* flush socket before to close */
  sleep (1);

  close (s);

  return 0;
}
