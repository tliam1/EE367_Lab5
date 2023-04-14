/*
  * switch.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#include "main.h"
#include "net.h"
#include "man.h"
#include "host.h"
#include "packet.h"
#include "switch.h"

#define TENMILLISEC 10000   /* 10 millisecond sleep */
#define MAX_PORT_TABLE_LENGTH 100

// Helper function used to see if something exist within the table and returns at what index
int check_switch_port_forwarding_table(struct switch_port_forwarding table [], int Value)
{
    for (int i = 0; i < MAX_PORT_TABLE_LENGTH; i++)
    {
        if (table[i].valid == Valid && table[i].dst == Value)
        {
            return table[i].port;
        }
    }
    return -1;
}

int assign_entry_in_table(struct switch_port_forwarding table [], char dst, int src_port)
{
    for (int i = 0; i < MAX_PORT_TABLE_LENGTH; i++)
    {
        if (table[i].valid == NotValid)
        {
            table[i].valid = Valid;
            table[i].dst = dst;
            table[i].port = src_port;
            return i;
        }
    }
	return -1;
}

void switch_main(int switch_id)
{

/* State */
struct net_port *node_port_list;
struct net_port **node_port;  // Array of pointers to node ports
int node_port_num;            // Number of node ports

int i, k, n;

struct packet *in_packet; /* Incoming packet */

struct net_port *p;
struct host_job *new_job;
struct host_job *new_job2;

struct job_queue job_q;

struct switch_port_forwarding MAC_Address_Table[MAX_PORT_TABLE_LENGTH];


/*
 * Initialize Table 
 * Sets the entries as not valid until assigned
 */
for (int i=0; i<MAX_PORT_TABLE_LENGTH; i++)
	MAC_Address_Table[i].valid = NotValid;

/*
 * Create an array node_port[ ] to store the network link ports
 * at the switch.  The number of ports is node_port_num
 */
node_port_list = net_get_port_list(switch_id);

	/*  Count the number of network link ports */
node_port_num = 0;
for (p=node_port_list; p!=NULL; p=p->next) {
	node_port_num++;
}
	/* Create memory space for the array */
node_port = (struct net_port **)
	malloc(node_port_num*sizeof(struct net_port *));

	/* Load ports into the array */
p = node_port_list;

for (k = 0; k < node_port_num; k++) {
	node_port[k] = p;
	p = p->next;
}

/* Initialize the job queue */
job_q_init(&job_q);

while(1) {

	for (k = 0; k < node_port_num; k++) { /* Scan all ports */

		in_packet = (struct packet *) malloc(sizeof(struct packet));
		n = packet_recv(node_port[k], in_packet);

		if (n > 0) {
			new_job = (struct host_job *)
				malloc(sizeof(struct host_job));
			new_job->in_port_index = k;
			new_job->packet = in_packet;

			job_q_add(&job_q, new_job);

		}
		else {
			free(in_packet);
		}
	}

	/*
 	 * Execute one job in the job queue
 	 */

	if (job_q_num(&job_q) > 0) {

		/* Get a new job from the job queue */
		new_job = job_q_remove(&job_q);

		if (check_switch_port_forwarding_table(MAC_Address_Table, new_job->packet->src) == -1)
		{
			assign_entry_in_table(MAC_Address_Table, new_job->packet->src, new_job->in_port_index);
		}

        int destination_port = check_switch_port_forwarding_table(MAC_Address_Table, new_job->packet->dst);

		if (destination_port == -1)
		{
            for (i=0; i<node_port_num; i++)
			{
				//printf("%d\n",node_port[i]->pipe_send_fd);
				if (i != new_job->in_port_index) {
					packet_send(node_port[i], new_job->packet);
				}
			}
		}
		else
		{
			packet_send(node_port[destination_port], new_job->packet);
		}

		free(new_job->packet);
		free(new_job);
	}

	/* The switch goes to sleep for 10 ms */
	usleep(TENMILLISEC);

} /* End of while loop */

}
