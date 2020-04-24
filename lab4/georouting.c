#include <cnet.h>
#include <cnetsupport.h>
#include <string.h>
#include <time.h>

#define	DEFAULT_WALKING_SPEED	3.0
#define	DEFAULT_PAUSE_TIME	20

#define	TX_NEXT			(5000000 + rand()%5000000)

typedef struct {
    int			dest;
    int			src;
    CnetPosition	prevpos;	// position of previous node
    int			length;		// length of payload
} WLAN_HEADER;

typedef struct {
    WLAN_HEADER		header;
    char		payload[2304];
} WLAN_FRAME;


//  EACH NODE WILL REPORT WHAT IT IS DOING IF verbose IS SET TO TRUE 
static	bool		verbose		= true;

//  THESE VARIABLES DEFINE 2 SHARED-MEMORY SEGMENTS BETWEEN NODES
//	http://www.csse.uwa.edu.au/cnet/shmem.html
static	int		*stats		= NULL;
static	CnetPosition	*positions	= NULL;

/* ----------------------------------------------------------------------- */

static EVENT_HANDLER(transmit)
{
    WLAN_FRAME	frame;
    int		link	= 1;

//  POPULATE A NEW FRAME
    do {
	frame.header.dest	= rand() % NNODES;
    } while(frame.header.dest == nodeinfo.nodenumber);
    frame.header.src		= nodeinfo.nodenumber;
    frame.header.prevpos	= positions[nodeinfo.nodenumber];	// me!

    sprintf(frame.payload, "hello from %d", nodeinfo.nodenumber);
    frame.header.length	= strlen(frame.payload) + 1; // send NUL too

//  TRANSMIT THE FRAME
    size_t len	= sizeof(WLAN_HEADER) + frame.header.length;
    CHECK(CNET_write_physical_reliable(link, &frame, &len));
    ++stats[0];

    if(verbose) {
	fprintf(stdout, "\n%s: transmitting '%s' to %d\n",
			nodeinfo.nodename, frame.payload, frame.header.dest);
    }

//  SCHEDULE OUR NEXT TRANSMISSION
    CNET_start_timer(EV_TIMER1, TX_NEXT, 0);
}

//  DETERMINE 2D-DISTANCE BETWEEN TWO POSITIONS
static double distance(CnetPosition p0, CnetPosition p1)
{
    int	dx	= p1.x - p0.x;
    int	dy	= p1.y - p0.y;

    return sqrt(dx*dx + dy*dy);
}

static EVENT_HANDLER(receive)
{
    WLAN_FRAME	frame;
    size_t	len;
    int		link;

//  READ THE ARRIVING FRAME FROM OUR PHYSICAL LINK
    len	= sizeof(frame);
    CHECK(CNET_read_physical(&link, &frame, &len));
    if(verbose) {
	double	rx_signal;
	CHECK(CNET_wlan_arrival(link, &rx_signal, NULL));
	fprintf(stdout, "\t%5s: received '%s' @%.3fdBm\n",
			    nodeinfo.nodename, frame.payload, rx_signal);
    }

//  IS THIS FRAME FOR ME?
    if(frame.header.dest == nodeinfo.nodenumber) {
	++stats[1];
	if(verbose)
	    fprintf(stdout, "\t\tfor me!\n");
    }

//  NO; RETRANSMIT FRAME IF WE'RE CLOSER TO THE DESTINATION THAN THE PREV NODE
    else {
	CnetPosition	dest	= positions[frame.header.dest];
	double		prev	= distance(frame.header.prevpos, dest);
	double		now	= distance(positions[nodeinfo.nodenumber],dest);

	if(now < prev) {	// closer?
	    frame.header.prevpos = positions[nodeinfo.nodenumber]; // me!
	    len			 = sizeof(WLAN_HEADER) + frame.header.length;
	    CHECK(CNET_write_physical_reliable(link, &frame, &len));
	    if(verbose)
		fprintf(stdout, "\t\tretransmitting\n");
	}
    }
}


//  THIS HANDLER IS CALLED PERIODICALLY BY node-0 TO REPORT STATISTICS
static EVENT_HANDLER(report)
{
    fprintf(stdout, "messages generated:\t%d\n", stats[0]);
    fprintf(stdout, "messages received:\t%d\n", stats[1]);
    if(stats[0] > 0)
	fprintf(stdout, "delivery ratio:\t\t%.1f%%\n",
	    (stats[0] > 0) ? (100.0*stats[1]/stats[0]) : 0);
}


EVENT_HANDLER(reboot_node)
{
    extern void init_mobility(double walkspeed_m_per_sec, int pausetime_secs);

    char	*env;
    double	value	= 0.0;

    if(NNODES == 0) {
	fprintf(stderr, "simulation must be invoked with the -N switch\n");
	exit(EXIT_FAILURE);
    }

//  ENSURE THAT WE'RE USING THE CORRECT VERSION OF cnet
    CNET_check_version(CNET_VERSION);
    srand(time(NULL) + nodeinfo.nodenumber);

//  INITIALIZE MOBILITY PARAMETERS
    env = getenv("WALKING_SPEED");
    if(env)
	value	= atof(env);
    double WALKING_SPEED    = (value > 0.0) ? value : DEFAULT_WALKING_SPEED;

    env = getenv("PAUSE_TIME");
    if(env)
	value	= atof(env);
    double PAUSE_TIME       = (value > 0.0) ? value : DEFAULT_PAUSE_TIME;

    init_mobility(WALKING_SPEED, PAUSE_TIME);

//  ALLOCATE MEMORY FOR SHARED MEMORY SEGMENTS
    stats	= CNET_shmem2("s", 2*sizeof(int));
    positions	= CNET_shmem2("p", NNODES*sizeof(CnetPosition));

//  PREPARE FOR OUR MESSAGE GENERATION AND TRANSMISSION
    CHECK(CNET_set_handler(EV_TIMER1, transmit, 0));
    CNET_start_timer(EV_TIMER1, TX_NEXT, 0);

//  SET HANDLERS FOR EVENTS FROM THE PHYSICAL LAYER
    CHECK(CNET_set_handler(EV_PHYSICALREADY,  receive, 0));

//  NODE-0 WILL PERIODICALLY REPORT THE STATISTICS (if with   cnet -f 10sec...)
    if(nodeinfo.nodenumber == 0)
	CHECK(CNET_set_handler(EV_PERIODIC,  report, 0));
}
