#include "cnet.h"
#include <stdlib.h>

typedef struct {
	int link;
	int min_hops;
} DISTANCE;

DISTANCE *distance_vector;


EVENT_HANDLER(reboot_node) {
	distance_vector = malloc(sizeof(DISTANCE) * NNODES);
	if(distance_vector == NULL) {
		printf("Couldn't allocate enough memory to contain distance\
			vector rip\n");
		exit(EXIT_FAILURE);
	}

	for(int i = 0; i < NNODES; ++i) {
		
	}
 }
