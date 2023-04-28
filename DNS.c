#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "main.h"
#include "net.h"
#include "man.h"
#include "host.h"
#include "packet.h"
#include "switch.h"
#include "DNS.h"

#define TENMILLISEC 10000

void AssignToTable(int id, char * name){
   Naming_Table[id].domainName = name;
}

int RequestID(char * name){
   for(int i = 0; i < MAXSTORAGE; i++){
      if(Naming_Table[i].domainName = name)
         return i;
   }
   return -1;
}


void DNS_main(int DNS_id){

struct net_port *node_port_list;
struct net_port **node_port;  // Array of pointers to node ports
int node_port_num;            // Number of node ports

int i, k, n;

struct packet *in_packet; /* Incoming packet */

struct net_port *p;
struct host_job *new_job;
struct host_job *new_job2;
struct man_port_at_host *man_port;
struct job_queue job_q;

struct DNS_Table Naming_Table[MAXSTORAGE];

for(int i = 0; i < MAXSTORAGE; i++){
   Naming_Table[i].domainName = "";

}

man_port = net_get_host_port(DNS_id);

/*
 * Create an array node_port[ ] to store the network link ports
 * at the host.  The number of ports is node_port_num
 */

node_port_list = net_get_port_list(DNS_id);

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
   //printf("type = %d\n",node_port[k]->type);
   node_port[k] = p;
   p = p->next;
}

/* Initialize the job queue */
job_q_init(&job_q);


/*
 * Wait till we revieve a job and then send back info or store info depeninding
 * on the info in the packet
 * 
 */

while(1){
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
      

   if (job_q_num(&job_q) > 0){

      //we grab the job and do an operation



   }

   /* The switch goes to sleep for 10 ms */
   usleep(TENMILLISEC);

} //end of while loop


}
