#include <cnet.h>
#include <stdlib.h>
#include <string.h>

/*  This is an implementation of a stop-and-wait data link protocol.
    It is based on Tanenbaum's `protocol 4', 2nd edition, p227.
    This protocol employs only data and acknowledgement frames -
    piggybacking and negative acknowledgements are not used.

    It is currently written so that only one node (number 0) will
    generate and transmit messages and the other (number 1) will receive
    them. This restriction seems to best demonstrate the protocol to
    those unfamiliar with it.
    The restriction can easily be removed by "commenting out" the line

	    if(nodeinfo.nodenumber == 0)

    in reboot_node(). Both nodes will then transmit and receive (why?).

    Note that this file only provides a reliable data-link layer for a
    network of 2 nodes.
 */


#define PIGGYBACK_TIMEOUT       1000000

typedef struct {
    char        data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
    size_t	 len;       	// the length of the msg field only
    int          checksum;  	// checksum of the whole frame
    int          seq;       	// only ever 0 or 1
    int          ack;

    MSG          msg;
} FRAME;

#define FRAME_HEADER_SIZE  (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE + f.len)
#define UNUSED_SEQ -1
#define PIGGYBACK_TIMER EV_TIMER1
#define DATA_TIMER      EV_TIMER2


MSG       	*lastmsg;
size_t		lastlength		= 0;
CnetTimerID data_timer      = NULLTIMER;
CnetTimerID piggyback_timer = NULLTIMER;

int       	ackexpected		= 0;
int		nextframetosend		= 0;
int		frameexpected		= 0;
int frametoack              = UNUSED_SEQ;


void transmit_frame(MSG *msg, size_t length, int seqno)
{
    FRAME       f;
    int		link = 1;

    f.seq       = seqno;
    f.checksum  = 0;
    f.len       = length;
    f.ack       = frametoack;

	CnetTime	timeout;

    
    memcpy(&f.msg, msg, (int)length);

	timeout = FRAME_SIZE(f)*((CnetTime)8000000 / linkinfo[link].bandwidth) +
				linkinfo[link].propagationdelay;
    if(seqno != UNUSED_SEQ) {
        printf(" DATA transmitted, seq=%d\n", seqno);
        data_timer = CNET_start_timer(DATA_TIMER, 3 * timeout, 0);
    } 

    length      = FRAME_SIZE(f);
    f.checksum  = CNET_ccitt((unsigned char *)&f, (int)length);
    CHECK(CNET_write_physical(link, &f, &length));

    if(frametoack != UNUSED_SEQ) {
        printf("ACK transmitted, seq=%d\n", frametoack);
        CNET_stop_timer(piggyback_timer);
        frametoack = UNUSED_SEQ;
    }
 
}

EVENT_HANDLER(application_ready)
{
    CnetAddr destaddr;
    lastlength  = sizeof(MSG);
    CHECK(CNET_read_application(&destaddr, lastmsg, &lastlength));
    CNET_disable_application(ALLNODES);
    printf("down from application, seq=%d\n", nextframetosend);
    transmit_frame(lastmsg, lastlength, nextframetosend);
    nextframetosend = 1-nextframetosend;
}

EVENT_HANDLER(physical_ready)
{
    FRAME        f;
    size_t	 len;
    int          link, checksum;

    len         = sizeof(FRAME);
    CHECK(CNET_read_physical(&link, &f, &len));

    checksum    = f.checksum;
    f.checksum  = 0;
    if(CNET_ccitt((unsigned char *)&f, (int)len) != checksum) {
        printf("\t\t\t\tBAD checksum - frame ignored\n");
        return;           // bad checksum, ignore frame
    }

    if(f.ack == ackexpected) {
            printf("\t\t\t\tACK received, seq=%d\n", f.seq);
            CNET_stop_timer(data_timer);
            ackexpected = 1-ackexpected;
            CNET_enable_application(ALLNODES);
    }

    if(f.seq == frameexpected) {
        printf("\t\t\t\tDATA received, seq=%d, ", f.seq);
        printf("up to application\n");
        len = f.len;
        CHECK(CNET_write_application(&f.msg, &len));
        frameexpected = 1-frameexpected;
    }
    if(f.seq != UNUSED_SEQ) {
        frametoack = f.seq;
        // // CnetTime	timeout;
        // timeout = FRAME_SIZE(f)*((CnetTime)8000000 / linkinfo[link].bandwidth) +
		// 		linkinfo[link].propagationdelay;
        piggyback_timer = CNET_start_timer(PIGGYBACK_TIMER,  PIGGYBACK_TIMEOUT, 0);
    }
}

EVENT_HANDLER(data_timeouts) {
    printf("timeout, seq=%d\n", ackexpected);
    transmit_frame(lastmsg, lastlength, ackexpected);
}

EVENT_HANDLER(piggyback_timeouts) {
    printf("Timer expired, sending ACK alone, seq=%d\n", frametoack);
    transmit_frame(NULL, 0, UNUSED_SEQ);
}

EVENT_HANDLER(showstate)
{
    printf(
    "\n\tackexpected\t= %d\n\tnextframetosend\t= %d\n\tframeexpected\t= %d\n",
		    ackexpected, nextframetosend, frameexpected);
}

EVENT_HANDLER(reboot_node)
{
    if(nodeinfo.nodenumber > 1) {
	fprintf(stderr,"This is not a 2-node network!\n");
	exit(1);
    }

    lastmsg	= calloc(1, sizeof(MSG));

    CHECK(CNET_set_handler( EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler( EV_PHYSICALREADY,    physical_ready, 0));
    CHECK(CNET_set_handler(DATA_TIMER, data_timeouts, 0));
    CHECK(CNET_set_handler(PIGGYBACK_TIMER, piggyback_timeouts, 0));
    CHECK(CNET_set_handler( EV_DEBUG0,showstate, 0));

    CHECK(CNET_set_debug_string( EV_DEBUG0, "State"));
	CNET_enable_application(ALLNODES);
    
}
