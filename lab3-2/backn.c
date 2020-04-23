#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define SIFS_UNUSED -1
#define WINDOW_SIZE 1
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

CnetTimerID ack_timers[WINDOW_SIZE] = { NULLTIMER };
CnetTimerID timeout_timers[WINDOW_SIZE] = { NULLTIMER };
FRAME *sending_window[WINDOW_SIZE];

int ackexpected  = 0;
int nextframetosend = 0;
int  frameexpected = 0;
int lastacked = 0;
int frames_acked = 0;
int frametoack = SIFS_UNUSED;


int between(int start, int val, int end) {
	if(val >= start && val <= end) {
		return(true);
	}
	return(false);
}

void transmit_frame(MSG *msg, size_t length, int seqno) {
	FRAME f; 
	int link = 1;

	f.seqno = seqno;
	f.ackno = frametoack;
	f.checksum = 0;
	f.len = length;

	printf("Transmitting frame seq=%d\nAckno=%d\n",f.seqno, f.ackno);
	if(seqno != SIFS_UNUSED) {
		int frame_index = seqno % WINDOW_SIZE;
		free(sending_window[frame_index]);
		sending_window[frame_index] = malloc(FRAME_SIZE(f));
		memcpy(sending_window[frame_index], &f, FRAME_SIZE(f));	
		memcpy(&f.msg, msg, (int)length);
		CnetTime timeout;
		timeout = FRAME_SIZE(f) *  ((CnetTime) 8000000 / linkinfo[link].bandwidth) +
			linkinfo[link].propagationdelay;
		int timer_index = nextframetosend % WINDOW_SIZE;
		timeout_timers[timer_index] = CNET_start_timer(TIMEOUT_TIMER, 3 * timeout, nextframetosend); 
	} 

	int ack_index = frames_acked % WINDOW_SIZE;
	if(frametoack != SIFS_UNUSED) {
		CNET_stop_timer(ack_timers[ack_index]);
		frames_acked++;
	}

	length = FRAME_SIZE(f);
	f.checksum  = CNET_ccitt((unsigned char *)&f, (int)length);
	CHECK(CNET_write_physical(link, &f, &length));
}

EVENT_HANDLER(application_ready) {
	CnetAddr destaddr;
	lastlength = sizeof(MSG);
	CHECK(CNET_read_application(&destaddr, lastmsg, &lastlength));
	printf("down from the application, seq=%d\n", nextframetosend);
	transmit_frame(lastmsg, lastlength, nextframetosend);
	nextframetosend++;
	if((nextframetosend + 1) - ackexpected >= WINDOW_SIZE) {
		printf("\nDisabling application layer, sending window full\n");
		CNET_disable_application(ALLNODES);
	}
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
		if(between(ackexpected, f.ackno, (nextframetosend - 1))) {
			printf("Ack reveived ack=%d\n", f.ackno);
			int ackindex = f.ackno % WINDOW_SIZE;
			int startindex = ackexpected % WINDOW_SIZE;
			for(int i = startindex; i <= ackindex; i++) {
				CNET_stop_timer(timeout_timers[i]);
			}
			ackexpected = f.ackno + 1;
			if((nextframetosend - ackexpected) < WINDOW_SIZE) {
				printf("\nEnabling application layer\n");
				CNET_enable_application(ALLNODES);
			}
			
		} else {
			printf("Received unexpected ack=%d\n\t\texpected ack=%d\n", f.ackno, ackexpected);
		}
	}

	if(f.seqno != SIFS_UNUSED) {
		int ack_index = f.seqno % WINDOW_SIZE;
		if(f.seqno == frameexpected) {
			frametoack = f.seqno;
			ack_timers[ack_index] = CNET_start_timer(ACK_TIMER, timeout,  0);
			printf("\t\t%d up to the application\n", f.seqno);
			len = f.len;
			CHECK(CNET_write_application(&f.msg, &len));
			frameexpected++;
		} else if(f.seqno < frameexpected) {
			printf("Receiving a frame again, expected=%d\n", frameexpected);
			ack_timers[ack_index] = CNET_start_timer(ACK_TIMER, timeout / 2, 0);
		} else {
			printf("Unexpected frame received, expected=%d\n", frameexpected);
		}
	}
}

EVENT_HANDLER(timeouts) {
	int sending_index = data % WINDOW_SIZE;
	FRAME *fr = sending_window[sending_index];
	MSG *fr_msg = &(fr->msg);
	printf("timeout seq=%d\n", fr->seqno);
	transmit_frame(fr_msg, lastlength, fr->seqno);
}

EVENT_HANDLER(ack_timeouts) {
	printf("ack timeout seq=%d\n", frametoack);
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

