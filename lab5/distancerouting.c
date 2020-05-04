#include "cnet.h"
#include <stdlib.h>
#include <string.h>

#define ALL_LINKS -1
#define UNKNOWN_HOPS -1
#define ROUTING_TIMER EV_TIMER0
#define THIRTY_SECONDS 30000000
#define DIST_VEC_SIZE (sizeof(DISTANCE) * NNODES)
#define ROUTING_FRAME_SIZE (sizeof(ROUTING_FRAME) + DIST_VEC_SIZE)
typedef struct {
	int link;
	int min_hops;
} DISTANCE;

typedef struct { 
	int src;
	size_t length;
	int n_distances; 
	DISTANCE dist_vec[0];  
} ROUTING_FRAME;


DISTANCE *distance_vector;
CnetTimerID routing_timer = NULLTIMER;

void transmit_routing_frame(int dest, DISTANCE *dist_vec) {
	ROUTING_FRAME *rf; 
	rf = malloc(ROUTING_FRAME_SIZE);
	if(rf == NULL) {
		printf("Memory allocation failed\n");
		exit(EXIT_FAILURE);
	}	
	size_t length = ROUTING_FRAME_SIZE; 
	rf->src = nodeinfo.nodenumber;
	rf->n_distances = NNODES;

	for(int i = 0; i < NNODES; ++i) {
		// COPY THE DISTANCE VECTOR INTO THE FRAMES DISTANCE VECTOR 
		rf->dist_vec[i].link = distance_vector[i].link;
		rf->dist_vec[i].min_hops = distance_vector[i].min_hops;
	}
	CNET_write_physical_reliable(dest, rf, &length);
	free(rf);
}
void adjust_distance(int node, int link, int distance) {
	if(distance_vector[node].min_hops == UNKNOWN_HOPS ||\
		distance_vector[node].min_hops > distance) {
			distance_vector[node].min_hops = distance;
			distance_vector[node].link = link;
	}
		return;
}

EVENT_HANDLER(receive_frame) {
	ROUTING_FRAME *rf = malloc(ROUTING_FRAME_SIZE);
	size_t length;
	length = ROUTING_FRAME_SIZE;
	int link;
	CHECK(CNET_read_physical(&link, rf, &length));
	int src = rf->src;
	printf("Received frame from %d\n", src);
	adjust_distance(src, src, 1);

	// Start with 1, we don't care about the distance to us 
	for(int i = 1; i < rf->n_distances; ++i) {
		if(i == src) continue;
		int src_node_distance = rf->dist_vec[i].min_hops;
		if(src_node_distance == UNKNOWN_HOPS) {
			continue;
		}
		adjust_distance(i, src, (src_node_distance + 1));
	}
	free(rf);
}

EVENT_HANDLER(send_routing_frame) {
	printf("Sending routing information frames to all links\n");
	for(int i = 1; i < NNODES; ++i) {
		transmit_routing_frame(i, distance_vector);
	}
	routing_timer = CNET_start_timer(ROUTING_TIMER, THIRTY_SECONDS, 0);
}

EVENT_HANDLER(show_distances) {
	for(int i = 0; i < NNODES; ++i) {
		printf("Node: %d\n", i);
		printf("Min hops: %d\n", distance_vector[i].min_hops);
		printf("Link: %d\n", distance_vector[i].link);
		printf("------------------\n");
	}
}
EVENT_HANDLER(reboot_node) {
	distance_vector = malloc(sizeof(DISTANCE) * NNODES);
	if(distance_vector == NULL) {
		printf("Couldn't allocate enough memory to contain distance\
			vector rip\n");
		exit(EXIT_FAILURE);
	}

	for(int i = 0; i < NNODES; ++i) {
		(distance_vector + i) -> link = ALL_LINKS;
		(distance_vector + i) -> min_hops = UNKNOWN_HOPS;
	}

	CHECK(CNET_set_handler(ROUTING_TIMER, send_routing_frame, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, receive_frame, 0));
	CHECK(CNET_set_handler(EV_DEBUG0, show_distances, 0));
	CHECK(CNET_set_debug_string(EV_DEBUG0, "Show Distances"));
	// I know where I am
	adjust_distance(nodeinfo.nodenumber, nodeinfo.nodenumber, 0);
	routing_timer = CNET_start_timer(ROUTING_TIMER, THIRTY_SECONDS, 0);
 }