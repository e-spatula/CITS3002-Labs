#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define SIFS_UNUSED -1
#define ACK_TIMER EV_TIMER1
#define TIMEOUT_TIMER EV_TIMER2
#define WINDOW_SIZE 2

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
CnetTimerID timeout_timers[WINDOW_SIZE];
CnetTimerID ack_timer = NULLTIMER;
MSG *sending_window[WINDOW_SIZE];

int ackexpected  = 0; // next acknowledgement expected
int nextframetosend = 0; // seqno of the next frame to send
int  frameexpected = 0; // the seqno of the next frame expected to be received
int frametoack = SIFS_UNUSED; // the seqno of the next frame to ack
int lastacksent = 0; // the last ack sent


bool between(int start, int val, int end) {
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
		memcpy(&f.msg, msg, (int)length);
		int sending_index = f.seqno % WINDOW_SIZE;
		CnetTime timeout;
		timeout = FRAME_SIZE(f) *  ((CnetTime) 8000000 / linkinfo[link].bandwidth) +
			linkinfo[link].propagationdelay;
		timeout_timers[sending_index] = CNET_start_timer(TIMEOUT_TIMER, 3 * timeout, f.seqno); 
	} 

	if(frametoack != SIFS_UNUSED) {
		CNET_stop_timer(ack_timer);
		lastacksent = frametoack;
		frametoack = SIFS_UNUSED;
	}

	length = FRAME_SIZE(f);
	f.checksum  = CNET_ccitt((unsigned char *)&f, (int)length);
	CHECK(CNET_write_physical(link, &f, &length));
}

EVENT_HANDLER(application_ready) {
	CnetAddr destaddr;
	lastlength = sizeof(MSG);
	printf("down from the application, seq=%d\n", nextframetosend);
	CHECK(CNET_read_application(&destaddr, lastmsg, &lastlength));
	int sending_index = nextframetosend % WINDOW_SIZE;
	free(sending_window[sending_index]);
	sending_window[sending_index] = malloc(lastlength);
	memcpy(sending_window[sending_index], lastmsg, lastlength);
	transmit_frame(lastmsg, lastlength, nextframetosend);
	
	nextframetosend++;
	if((nextframetosend - ackexpected) >= WINDOW_SIZE) {
		printf("\nDisabling application layer\n");
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
			int startindex = ackexpected % WINDOW_SIZE;
			int stopindex = f.ackno % WINDOW_SIZE;
			for(int i = startindex; i <= stopindex; ++i) {
				CnetData *data = malloc(sizeof(int) * 1);
				CNET_timer_data(timeout_timers[i], data);
				int timer_data = (int)(*data);
				printf("\n\t\tStopping timer for seq=%d\n", timer_data);
				CNET_stop_timer(timeout_timers[i]);
			}
			ackexpected = f.ackno + 1;
			// If last batch of messages have been acknowledged, restart app layer
			if((nextframetosend - ackexpected) == 0) {
				printf("\n Enabling application layer\n");
				CNET_enable_application(ALLNODES);
			}
		} else {
			printf("Received unexpected ack=%d\texpected=%d\n", f.ackno, ackexpected);
		}
	}

	if(f.seqno != SIFS_UNUSED) {
		// check if the timer is already ticking, if not start it
		if(ack_timer == NULLTIMER) {
			ack_timer = CNET_start_timer(ACK_TIMER, timeout,  0);
		}
		if(f.seqno == frameexpected) {
			printf("up to the application seq=%d\n", f.seqno);
			len = f.len;
			CHECK(CNET_write_application(&f.msg, &len));
			frametoack = f.seqno;
			frameexpected++;
		} else {
			if(frametoack == SIFS_UNUSED) {
				frametoack = lastacksent;
				ack_timer = CNET_start_timer(ACK_TIMER, timeout, 0);
			}
			printf("Unexpected frame received seq=%d\n", f.seqno);
		}
	}
}

EVENT_HANDLER(timeouts) {
	printf("timeout seq=%ld\n", data);
	int sending_index = data % WINDOW_SIZE;
	MSG *msgptr = sending_window[sending_index];
	transmit_frame(msgptr, lastlength, data);
}

EVENT_HANDLER(ack_timeouts) {
	printf("ack timeout seq=%d\n", frametoack);
	transmit_frame(NULL, 0, SIFS_UNUSED);
	ack_timer = NULLTIMER;
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
