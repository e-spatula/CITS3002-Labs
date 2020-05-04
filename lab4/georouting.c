#include <cnet.h>
#include <cnetsupport.h>
#include <string.h>
#include <time.h>

#define	DEFAULT_WALKING_SPEED	3.0
#define	DEFAULT_PAUSE_TIME	20
#define TIMEOUT_TIMER EV_TIMER0
#define APPLICATION_TIMER EV_TIMER2
#define TIMEOUT (5000000 + rand()%5000000)

typedef struct {
    CnetAddr	dest;
    int			src;
    CnetPosition	prevpos;	// position of previous node
    int			length;		// length of payload
} WLAN_HEADER;

typedef struct {
    char payload[2304];
}MSG;

typedef struct {
    WLAN_HEADER		header;
    MSG msg;
} WLAN_FRAME;

MSG *lastmsg;
CnetTimerID timeout_timer;
CnetTimerID application_timer;

//  EACH NODE WILL REPORT WHAT IT IS DOING IF verbose IS SET TO TRUE 
static	bool		verbose		= true;

//  THESE VARIABLES DEFINE 2 SHARED-MEMORY SEGMENTS BETWEEN NODES
//	http://www.csse.uwa.edu.au/cnet/shmem.html
static	int		*stats		= NULL;
static	CnetPosition	*positions	= NULL;
size_t lastlength = 0;
CnetAddr lastaddr; 

/* ----------------------------------------------------------------------- */

void transmit(MSG *msg, size_t length, CnetAddr dest) 
{
    WLAN_FRAME	frame;
    int		link	= 1;

//  POPULATE A NEW FRAME    
    frame.header.src		= nodeinfo.nodenumber;
    frame.header.prevpos	= positions[nodeinfo.nodenumber];	// me!
    frame.header.dest	= dest;    

    memcpy(&frame.msg, lastmsg, length);
    frame.header.length	= length; // send NUL too

//  TRANSMIT THE FRAME
    size_t len	= sizeof(WLAN_HEADER) + frame.header.length;
    CHECK(CNET_write_physical(link, &frame, &len));
    ++stats[0];

    CnetTime timeout;
    timeout = sizeof(frame)*((CnetTime)8000000 / linkinfo[link].bandwidth) +
				linkinfo[link].propagationdelay;
    
    application_timer = CNET_start_timer(APPLICATION_TIMER, timeout, 0);

    printf("Transmitting to %d", lastaddr);
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
	fprintf(stdout, "\t%5s: received @%.3fdBm\n",
			    nodeinfo.nodename, rx_signal);
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

EVENT_HANDLER(application_ready) {
    lastlength = sizeof(MSG);
    fprintf(stdout, "Down from the application\n");
    CHECK(CNET_read_application(&lastaddr, lastmsg, &lastlength));
    transmit(lastmsg, lastlength, lastaddr);
    CNET_disable_application(ALLNODES);
}

EVENT_HANDLER(collision_handler) {
    fprintf(stdout, "OUCH! a collision, see you in a little while\n");
    timeout_timer = CNET_start_timer(TIMEOUT_TIMER, TIMEOUT, 0);
}

EVENT_HANDLER(timeouts) {
    fprintf(stdout, "Timer timed out, trying to resend frame\n");
    transmit(lastmsg, lastlength, lastaddr);
}

EVENT_HANDLER(frame_sent) {
    fprintf(stdout, "Timer expired, re-enabling application layer");
    CNET_enable_application(ALLNODES);
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

    CNET_enable_application(ALLNODES);

//  SET HANDLERS FOR EVENTS FROM THE PHYSICAL LAYER
    CHECK(CNET_set_handler(EV_PHYSICALREADY,  receive, 0));

// SET HANDLER FOR EVENTS FROM THE APPLICATION LAYER
    CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));

// SET HANDLER FOR FRAME COLLISION
    CHECK(CNET_set_handler(EV_FRAMECOLLISION, collision_handler, 0));

// SET HANDLER FOR TIMER TIMEOUT
    CHECK(CNET_set_handler(TIMEOUT_TIMER, timeouts, 0));
// SET HANDLER FOR APPLICATION LAYER RE-ENABLING
    CHECK(CNET_set_handler(APPLICATION_TIMER, frame_sent, 0));
//  NODE-0 WILL PERIODICALLY REPORT THE STATISTICS (if with   cnet -f 10sec...)
    if(nodeinfo.nodenumber == 0)
	CHECK(CNET_set_handler(EV_PERIODIC,  report, 0));
}
