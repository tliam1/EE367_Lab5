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

#define MAX_FILE_BUFFER 1000
#define MAX_MSG_LENGTH 100
#define MAX_DIR_NAME 100
#define MAX_FILE_NAME 100
#define PKT_PAYLOAD_MAX 100
#define TENMILLISEC 10000   /* 10 millisecond sleep */

#define MAX_TABLE_SIZE 100

//indecies for the forwarding table 2d array
#define V 0 //valid dest port 
#define D 1
#define P 2 // port in config file

void switch_main(int switch_id){

   struct net_port *node_port_list; // list/arr of all ports
   struct net_port **node_port;  // Array of pointers to node ports
   int node_port_num;            // Number of node ports

   int i, k, n;
   int src, dst;
   int forwarding_table[MAX_TABLE_SIZE][3]; //this is for "memory" of past hosts that got sent a packet

   struct packet *in_packet; /* Incoming packet */
   struct packet *new_packet;
   struct net_port *p;
   struct host_job *new_job;
   struct job_queue job_q;



/*
 * Remove all the ports for the switch from linked list g_port_list.
 * and create another linked list.  Return the pointer to this
 * linked list.
 */
   node_port_list = net_get_port_list(switch_id);
   node_port_num = 0;
   for (p=node_port_list; p!=NULL; p=p->next) { // finds the total number of nodes in the port list (switch connections)
      node_port_num++;
   }
   node_port = (struct net_port **) malloc(node_port_num*sizeof(struct net_port *));
   p = node_port_list;
   //store all ports in the node_port array
   for (k = 0; k < node_port_num; k++) {
      node_port[k] = p;
      p = p->next;
   }

   for(int x = 0; x < MAX_TABLE_SIZE; x++) {
      for(int y = 0; y < 3; y++) {
         forwarding_table[x][y] = -1; //initializes all forwarding teble entries to -1
      }
   }

   /* Initialize job queue for the switch node (find info in host.c)*/
   job_q_init(&job_q);




   //main loop


   while(1) {
      /*
       * Get packets from incoming links and translate to jobs
       * Put jobs in job queue
       */

      for (k = 0; k < node_port_num; k++) { /* Scan all ports */
         in_packet = (struct packet *) malloc(sizeof(struct packet));
         n = packet_recv(node_port[k], in_packet);

         if(n > 0) {
            printf("\nswitch received a packet\n");
            new_job = (struct host_job *) malloc(sizeof(struct host_job));
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
      //find the source and destination insite the forwarding table
      //(if they are in there aready) and they are valid
      src = -1;
      dst = -1;
      if (job_q_num(&job_q) > 0) {
         /* Get a new job from the job queue */
         new_job = job_q_remove(&job_q);

         for(i = 0; forwarding_table[i][V] != -1; i++) {
            if((new_job->packet->src == forwarding_table[i][D]) 
                  && (new_job->in_port_index == forwarding_table[i][P])) {
               src = forwarding_table[i][P]; 
            }

            if(new_job->packet->dst == forwarding_table[i][D]) {
               dst = forwarding_table[i][P];
            }
         }


         // the destination was known and therefore sent to
         // the specific host
         if(dst >= 0) {
            printf("Dst Found! Sending from switch to host %d\n", dst);
            packet_send(node_port[dst], new_job->packet);
         } 
         // the destination was unknown and therefore sent to all hosts 
         else {
            printf("Dst Unknown! Sending from switch to all hosts\n");
            for(k = 0; k < node_port_num; k++) {
               if(new_job->in_port_index != k) {
                  packet_send(node_port[k], new_job->packet);
               }
            }
         }

         //if source is unknown, input it into the forwarding table and mark it as valid
         if(src < 0) {
            forwarding_table[i][V] = 1;
            forwarding_table[i][D] = new_job->packet->src;
            forwarding_table[i][P] = new_job->in_port_index;

            printf("new FT entry: forwarding_table[%d]: Host = %d, port_on_switch = %d\n"
                  ,i,new_job->packet->src,new_job->in_port_index);
         }

         free(new_job->packet);
         free(new_job);
      }
      usleep(TENMILLISEC);
   }
}

