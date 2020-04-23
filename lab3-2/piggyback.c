#include <cnet.h>
#include <stdlib.h>
#include <string.h>

#define SIFS_UNUSED -1
#define ACK_TIMER EV_TIMER1
#define TIMEOUT_TIMER EV_TIMER2

typedef struct {
	char data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
	size_t len;
	int checksum;
	int seqno; 
	int ackno;
	MSG msg;
} FRAME;

#define FRAME_HEADER_SIZE (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f) (FRAME_HEADER_SIZE + f.len)


MSG *lastmsg;
size_t lastlength = 0;
CnetTimerID timeout_timer = NULLTIMER;
CnetTimerID ack_timer = NULLTIMER;

int ackexpected  = 0;
int nextframetosend = 0;
int  frameexpected = 0;
int acktosend = SIFS_UNUSED;

void transmit_frame(MSG *msg, size_t length, int seqno) {
	FRAME f; 
	int link = 1;

	f.seqno = seqno;
	f.ackno = acktosend;
	f.checksum = 0;
	f.len = length;

	printf("Transmitting frame seq=%d\nAckno=%d\n",f.seqno, f.ackno);
	if(seqno != SIFS_UNUSED) {
		memcpy(&f.msg, msg, (int)length);
		CnetTime timeout;
		timeout = FRAME_SIZE(f) *  ((CnetTime) 8000000 / linkinfo[link].bandwidth) +
			linkinfo[link].propagationdelay;
		timeout_timer = CNET_start_timer(TIMEOUT_TIMER, 3 * timeout, 0); 
	} 

	if(acktosend != SIFS_UNUSED) {
		CNET_stop_timer(ack_timer);
		acktosend = SIFS_UNUSED;
	}

	length = FRAME_SIZE(f);
	f.checksum  = CNET_ccitt((unsigned char *)&f, (int)length);
	CHECK(CNET_write_physical(link, &f, &length));
}

EVENT_HANDLER(application_ready) {
	CnetAddr destaddr;
	lastlength = sizeof(MSG);
	CHECK(CNET_read_application(&destaddr, lastmsg, &lastlength));
	CNET_disable_application(ALLNODES);

	printf("down from the application, seq=%d\n", nextframetosend);
	transmit_frame(lastmsg, lastlength, nextframetosend);
	nextframetosend = 1 - nextframetosend;
}

EVENT_HANDLER(physical_ready) {
	FRAME f; 
	size_t len;
	int link, checksum;
	len = sizeof(FRAME);
	CHECK(CNET_read_physical(&link, &f, &len));
	checksum = f.checksum;
	f.checksum = 0;
	CnetTime timeout;
	timeout = FRAME_SIZE(f) *  ((CnetTime) 8000000 / linkinfo[link].bandwidth) +
			linkinfo[link].propagationdelay;

	printf("Frame received seq=%d\nack=%d\n", f.seqno, f.ackno);
	if(CNET_ccitt((unsigned char *)&f, (int)len) != checksum) {
        printf("\t\t\t\tBAD checksum - frame ignored\n");
        return;           // bad checksum, ignore frame
    }

	if(f.ackno != SIFS_UNUSED) {
		if(f.ackno == ackexpected) {
			printf("Ack reveived ack=%d\n", f.ackno);
			CNET_stop_timer(timeout_timer);
			ackexpected = 1 - ackexpected;
			CNET_enable_application(ALLNODES);
		} else {
			printf("Received unexpected ack=%d\n", f.ackno);
		}
	}

	if(f.seqno != SIFS_UNUSED) {
		acktosend = f.seqno;
		ack_timer = CNET_start_timer(ACK_TIMER, timeout,  0);
		if(f.seqno == frameexpected) {
			printf("up to the application\n");
			len = f.len;
			CHECK(CNET_write_application(&f.msg, &len));
			frameexpected = 1 - frameexpected;
		} else {
			printf("Unexpected frame received\n");
		}
	}
}

EVENT_HANDLER(timeouts) {
	printf("timeout seq=%d\n", ackexpected);
	transmit_frame(lastmsg, lastlength, ackexpected);
}

EVENT_HANDLER(ack_timeouts) {
	printf("ack timeout seq=%d\n", acktosend);
	transmit_frame(NULL, 0, SIFS_UNUSED);
}

EVENT_HANDLER(showstate)
{
    printf(
    "\n\tackexpected\t= %d\n\tnextframetosend\t= %d\n\tframeexpected\t= %d\n",
		    ackexpected, nextframetosend, frameexpected);
}

EVENT_HANDLER(reboot_node) {
	if(nodeinfo.nodenumber > 1) {
		fprintf(stderr, "This is not a two node network\n");
	}

	lastmsg = calloc(1, sizeof(MSG));

	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(TIMEOUT_TIMER, timeouts, 0));
	CHECK(CNET_set_handler(ACK_TIMER, ack_timeouts, 0));
	CHECK(CNET_set_handler(EV_DEBUG0, showstate, 0));

	CHECK(CNET_set_debug_string(EV_DEBUG0, "State"));

	if(nodeinfo.nodenumber == 0) {
		CNET_enable_application(ALLNODES);
	}
}
