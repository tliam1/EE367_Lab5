
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include  <netinet/in.h>
#include <sys/socket.h>
#include <limits.h>
#include <arpa/inet.h>




#include "main.h"
#include "packet.h"
#include "net.h"
#include "host.h"

//int socket_node, socket_port0, socket_port1;
  // char domain0[MAX_DOMAIN_NAME], domain1[MAX_DOMAIN_NAME];

void packet_send(struct net_port *port, struct packet *p)
{
char msg[PAYLOAD_MAX+4];
int i;

if (port->type == PIPE) {
	msg[0] = (char) p->src; 
	msg[1] = (char) p->dst;
	msg[2] = (char) p->type;
	msg[3] = (char) p->length;
	for (i=0; i<p->length; i++) {
		msg[i+4] = p->payload[i];
	}
	write(port->pipe_send_fd, msg, p->length+4);
//printf("PACKET SEND, src=%d dst=%d p-src=%d p-dst=%d\n", 
//		(int) msg[0], 
//		(int) msg[1], 
//		(int) p->src, 
//		(int) p->dst);
}else if(port->type == SOCKET){
   // send to other port on config file using send()
   msg[0] = (char) p->src;
   msg[1] = (char) p->dst;
   msg[2] = (char) p->type;
   msg[3] = (char) p->length;
   for (i=0; i<p->length; i++) {
      msg[i+4] = p->payload[i];
   }

   printf("SENT MESSAGE ACCROSS PORT");

   size_t len = strlen(msg);
   if (send(get_socket(), msg, len, 0) == -1)
      perror("send ");

}

return;
}


void packet_send_socket(struct packet *p){
   char msg[PAYLOAD_MAX+4];
   int i;

   if(get_socket() != -1){
      // send to other port on config file using send()
      msg[0] = (char) p->src;
      msg[1] = (char) p->dst;
      msg[2] = (char) p->type;
      msg[3] = (char) p->length;
      for (i=0; i<p->length; i++) {
         msg[i+4] = p->payload[i];
      }

      printf("SENT MESSAGE ACCROSS PORT");

   size_t len = strlen(msg);
   if (send(get_socket(), msg, len, 0) == -1)
      perror("send ");

   }else{
      printf("Packet.c: Socket Connection Unknown!\n");
   }
return;
}


int packet_recv(struct net_port *port, struct packet *p)
{
char msg[PAYLOAD_MAX+4];
int n=0;
int i=0;

if (port->type == PIPE) {
	n = read(port->pipe_recv_fd, msg, PAYLOAD_MAX+4);
	if (n>0) {
		p->src = (char) msg[0];
		p->dst = (char) msg[1];
		p->type = (char) msg[2];
		p->length = (int) msg[3];
		for (i=0; i<p->length; i++) {
			p->payload[i] = msg[i+4];
		}

 /*printf("PACKET RECV, src=%d dst=%d p-src=%d p-dst=%d\n", 
		(int) msg[0], 
		(int) msg[1], 
		(int) p->src, 
		(int) p->dst);
      */
	}
}else if (port->type == SOCKET){
   if(get_socket() != -1){

      if ((n = recv(get_socket(), msg, MAXDATASIZE-1, 0)) == -1)  {
         perror("recv");
      }

      if (n>0) {
         msg[n] = '\0';
         p->src = (char) msg[0];
         p->dst = (char) msg[1];
         p->type = (char) msg[2];
         p->length = (int) msg[3];
         for (i=0; i<p->length; i++) {
            p->payload[i] = msg[i+4];
         }

         printf("SENT RECV'D ACCROSS PORT");
      }
   }
}
return(n);
}

void packet_recv_socket(struct packet *p){
   char msg[PAYLOAD_MAX+4];
   int n;
   int i;
   if(get_socket() != -1){

      if ((n = recv(get_socket(), msg, MAXDATASIZE-1, 0)) == -1)  {
         perror("recv");
      }

      if (n>0) {
         msg[n] = '\0';
         p->src = (char) msg[0];
         p->dst = (char) msg[1];
         p->type = (char) msg[2];
         p->length = (int) msg[3];
         for (i=0; i<p->length; i++) {
            p->payload[i] = msg[i+4];
         }

         printf("SENT RECV'D ACCROSS PORT");
      }
      return;
   }
}

