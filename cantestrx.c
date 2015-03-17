/*
 * Author : Fabien Lahoudere (fabien.lahoudere@openwide.fr)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#define NO_CAN_ID (0xFFFFFFFFU)

static void Usage()
{
  fprintf( stderr, "\n");
  fprintf( stderr, "canrcv [options]\n");
  fprintf( stderr, "\tDefault: receive all frames from all can devices\n");
  fprintf( stderr, "\tOptions:\n");
  fprintf( stderr, "\t\t-i <interface>\tset the interface to listen\n");
  fprintf( stderr, "\t\t-c <count>\twait for count frames\n");
  fprintf( stderr, "\t\t-f <filter>\tset the id to filter\n");
  fprintf( stderr, "\t\t-v\tverbose mode\n");
  fprintf( stderr, "\t\t-p\tprint id:\n");
  fprintf( stderr, "\t\t-z <protnbr>\tchange protocol number\n");
  fprintf( stderr, "\n");

  exit(-1);
}

int
main (int argc, char **argv)
{
  char * iface = NULL;
  int s, count=1, iter=0, opt, verbose = 0, usefilter=0, printids=0, proto = CAN_RAW;
  struct sockaddr_can addr;
  struct can_frame frame;
  struct ifreq ifr;
  struct can_filter filter[1];
  canid_t id = 0;
  char * mask=NULL;

  while ((opt = getopt (argc, argv, "i:c:f:vph")) != -1)
    {
      switch (opt)
	{
	case 'h':
	  Usage();
	  break;
	case 'i':
	  iface = optarg;
	  break;
	case 'c':
	  count = atoi (optarg);
	  break;
	case 'f':
	  mask = strchr(optarg,',');
          if ( mask )
          {
            *mask=0;
            mask++;
          }

	  filter[0].can_id = strtoul (optarg, (char **) NULL, 16);
	  if ( (filter[0].can_id&0x1fffffff) > 0x7ff)
	    filter[0].can_id |= CAN_EFF_FLAG;
	  
	  if( mask )
	    filter[0].can_mask = strtoul (mask, (char **) NULL, 16);
	  else
	    {
	      if ( (filter[0].can_id&0x1fffffff) > 0x7ff)
		filter[0].can_mask = 0xdfffffff;
	      else
		filter[0].can_mask = 0x7ff;
	    }
	  usefilter=1;
	  break;
	case 'v':
	  verbose = 1;
	  break;
	case 'p':
	  printids = 1;
	  break;
        case 'z':
          proto = atoi(optarg);
          break;
	default:
	  Usage();
	  break;
	}
    }

  /* open socket */
  if ((s = socket (PF_CAN, SOCK_RAW, proto)) < 0)
    {
      perror ("socket");
      Usage();
    }

  if (usefilter)
    {
      if(verbose)
	fprintf(stderr,"Set filter can_id %08x, mask %08x\n",filter[0].can_id,filter[0].can_mask);
      if (setsockopt (s, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof (filter)) < 0)
	{
	  perror ("setsockopt");
	  Usage();
	}
    }

  addr.can_family = AF_CAN;
  if ( !iface )
    addr.can_ifindex = 0;
  else
    {
      strcpy (ifr.ifr_name, iface);
      if (ioctl (s, SIOCGIFINDEX, &ifr) < 0)
	{
	  perror ("SIOCGIFINDEX");
	  Usage();
	}
      addr.can_ifindex = ifr.ifr_ifindex;
    }

  if (bind (s, (struct sockaddr *) &addr, sizeof (addr)) < 0)
    {
      perror ("bind");
      Usage();
    }

  /* rcv frame */
  while (iter < count)
    {
      if ((read (s, &frame, sizeof (frame))) < 0)
	{
	  fprintf (stderr,"Error reading\n");
	  return 0;		/* quit */
	}
      write(STDOUT_FILENO, frame.data, frame.can_dlc);

      if (printids && (id!=frame.can_id))
	{
	  id = frame.can_id;
	  fprintf(stderr,"Id: %08x\n", id);
	}
      
      if( count>1 )
	iter++;

      //printf ("[%d]%04x-%llu\n", iter, frame.can_id, (*((unsigned long long *) frame.data)));
    }

  close (s);

  return 0;
}
