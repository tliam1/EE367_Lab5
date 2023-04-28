 /*
  * host.c
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

#define MAX_FILE_BUFFER 1000
#define MAX_MSG_LENGTH 100
#define MAX_DIR_NAME 100
#define MAX_FILE_NAME 100
#define PKT_PAYLOAD_MAX 100
#define TENMILLISEC 10000   /* 10 millisecond sleep */


struct DNS_Table {
   char * domainName;
};

/* Types of packets */

char oldFileName[MAX_FILE_NAME];

struct file_buf {
	char name[MAX_FILE_NAME];
	int name_length;
	char buffer[MAX_FILE_BUFFER+1];
	int head;
	int tail;
	int occ;
	FILE *fd;
};

struct file_dat{

   char heldFileName[MAX_FILE_NAME];
   
};

/*
 * File buffer operations
 */

/* Initialize file buffer data structure */
void file_buf_init(struct file_buf *f)
{
f->head = 0;
f->tail = MAX_FILE_BUFFER;
f->occ = 0;
f->name_length = 0;
}

/* 
 * Get the file name in the file buffer and store it in name 
 * Terminate the string in name with tne null character
 */
void file_buf_get_name(struct file_buf *f, char name[])
{
int i;

for (i=0; i<f->name_length; i++) {
	name[i] = f->name[i];
}
name[f->name_length] = '\0';
}

/*
 *  Put name[] into the file name in the file buffer
 *  length = the length of name[]
 */
void file_buf_put_name(struct file_buf *f, char name[], int length)
{
int i;

for (i=0; i<length; i++) {
	f->name[i] = name[i];
	oldFileName[i] = name[i];
}

oldFileName[length] = '\0';
f->name_length = length;
}

/*
 *  Add 'length' bytes n string[] to the file buffer
 */
int file_buf_add(struct file_buf *f, char string[], int length)
{
int i = 0;

while (i < length && f->occ < MAX_FILE_BUFFER) {
	f->tail = (f->tail + 1) % (MAX_FILE_BUFFER + 1);
	f->buffer[f->tail] = string[i];
	i++;
        f->occ++;
}
return(i);
}

/*
 *  Remove bytes from the file buffer and store it in string[] 
 *  The number of bytes is length.
 */
int file_buf_remove(struct file_buf *f, char string[], int length)
{
int i = 0;

while (i < length && f->occ > 0) {
	string[i] = f->buffer[f->head];
	f->head = (f->head + 1) % (MAX_FILE_BUFFER + 1);
	i++;
        f->occ--;
}

return(i);
}


/*
 * Operations with the manager
 */

int get_man_command(struct man_port_at_host *port, char msg[], char *c) {

int n;
int i;
int k;

n = read(port->recv_fd, msg, MAN_MSG_LENGTH); /* Get command from manager */
if (n>0) {  /* Remove the first char from "msg" */
	for (i=0; msg[i]==' ' && i<n; i++);
	*c = msg[i];
	i++;
	for (; msg[i]==' ' && i<n; i++);
	for (k=0; k+i<n; k++) {
		msg[k] = msg[k+i];
	}
	msg[k] = '\0';
}
return n;

}

/*
 * Operations requested by the manager
 */

/* Send back state of the host to the manager as a text message */
void reply_display_host_state(
		struct man_port_at_host *port,
		char dir[],
		int dir_valid,
		int host_id)
{
int n;
char reply_msg[MAX_MSG_LENGTH];

if (dir_valid == 1) {
	n =sprintf(reply_msg, "%s %d", dir, host_id);
}
else {
	n = sprintf(reply_msg, "None %d", host_id);
}

write(port->send_fd, reply_msg, n);
}



/* Job queue operations */

/* Add a job to the job queue */
void job_q_add(struct job_queue *j_q, struct host_job *j)
{
if (j_q->head == NULL ) {
	j_q->head = j;
	j_q->tail = j;
	j_q->occ = 1;
}
else {
	(j_q->tail)->next = j;
	j->next = NULL;
	j_q->tail = j;
	j_q->occ++;
}
}

/* Remove job from the job queue, and return pointer to the job*/
struct host_job *job_q_remove(struct job_queue *j_q)
{
struct host_job *j;

if (j_q->occ == 0) return(NULL);
j = j_q->head;
j_q->head = (j_q->head)->next;
j_q->occ--;
return(j);
}

/* Initialize job queue */
void job_q_init(struct job_queue *j_q)
{
j_q->occ = 0;
j_q->head = NULL;
j_q->tail = NULL;
}

int job_q_num(struct job_queue *j_q)
{
return j_q->occ;
}

/*
 *  Main 
 */

struct DNS_Table Naming_Table[MAXSTORAGE];
//DNS REGISTRATION
void AssignToTable(int id, char * name){
   if(Naming_Table[id].domainName == NULL){
      Naming_Table[id].domainName = (char*) malloc(strlen(name) + 1); // allocate memory for the new name
      strcpy(Naming_Table[id].domainName, name); // copy the name string to the new memory location
      printf("Registered Name: %s at id %d\n\n", Naming_Table[id].domainName, id);
   }
}

int RequestID(char * name){
   printf("Requesting ID: %s\n", name);
   for(int i = 0; i < MAXSTORAGE; i++){
      if(Naming_Table[i].domainName != NULL)
         printf("Entry[%d] is: %s\n", i, Naming_Table[i].domainName);
      if(Naming_Table[i].domainName != NULL && strcmp(name, Naming_Table[i].domainName) == 0){
         return i;
      }
   }
   
   //printf("%s\n\n", name);
   return -1;
}



void host_main(int host_id)
{
/* State */
char dir[MAX_DIR_NAME];
int dir_valid = 0;
struct file_dat dat;
char man_msg[MAN_MSG_LENGTH];
char man_reply_msg[MAN_MSG_LENGTH];
char man_cmd;
struct man_port_at_host *man_port;  // Port to the manager
//struct DNS_Table Naming_Table[MAXSTORAGE];

struct net_port *node_port_list;
struct net_port **node_port;  // Array of pointers to node ports
int node_port_num;            // Number of node ports

int ping_reply_received;

int i, k, n;
int dst;
char name[MAX_FILE_NAME];
char string[PKT_PAYLOAD_MAX+1]; 

FILE *fp;

struct packet *in_packet; /* Incoming packet */
struct packet *new_packet;

struct net_port *p;
struct host_job *new_job;
struct host_job *new_job2;

struct job_queue job_q;

struct file_buf f_buf_upload;  
struct file_buf f_buf_download; 

file_buf_init(&f_buf_upload);
file_buf_init(&f_buf_download);

/*
 * Initialize pipes 
 * Get link port to the manager
 */
//printf("host_id: %d\n",host_id);
man_port = net_get_host_port(host_id);

/*
 * Create an array node_port[ ] to store the network link ports
 * at the host.  The number of ports is node_port_num
 */

node_port_list = net_get_port_list(host_id);

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

while(1) {
	n = get_man_command(man_port, man_msg, &man_cmd);

		/* Execute command */
	if (n>0) {
		switch(man_cmd) {
			case 's':
				reply_display_host_state(man_port,
					dir, 
					dir_valid,
					host_id);
				break;	
			
			case 'm':
				dir_valid = 1;
				for (i=0; man_msg[i] != '\0'; i++) {
					dir[i] = man_msg[i];
				}
				dir[i] = man_msg[i];
				break;

			case 'p': // Sending ping request
				// Create new ping request packet
            sscanf(man_msg, "%d %s", &dst, name);
            new_job = (struct host_job *)
                  malloc(sizeof(struct host_job));
            new_job->type = JOB_REQUEST_ID_SEND;
            new_job->file_upload_dst = 100;
            for (i=0; name[i] != '\0'; i++) {
               new_job->domNameUp[i] = name[i];
            }
            new_job->domNameUp[i] = ' ';
            new_job->domNameUp[i+1] = 'p';
            //printf("Host: New_job->domNameUp: %s\n", new_job->domNameUp);
            new_job->domNameUp[i+2] = '\0';
            job_q_add(&job_q, new_job);


				//sscanf(man_msg, "%d", &dst);

            //printf("string literal of dst %s\n", (char)dst);
/*            dst = requestedID;
            printf("Requested ID is %d\n", dst);
				new_packet = (struct packet *) 
						malloc(sizeof(struct packet));	
				new_packet->src = (char) host_id;
				new_packet->dst = (char) dst;
				new_packet->type = (char) PKT_PING_REQ;
				new_packet->length = 0;
				new_job = (struct host_job *) 
						malloc(sizeof(struct host_job));
				new_job->packet = new_packet;
				new_job->type = JOB_SEND_PKT_ALL_PORTS;
				job_q_add(&job_q, new_job);

				new_job2 = (struct host_job *) 
						malloc(sizeof(struct host_job));
				ping_reply_received = 0;
				new_job2->type = JOB_PING_WAIT_FOR_REPLY;
				new_job2->ping_timer = 10;
				job_q_add(&job_q, new_job2);
  */          
				break;

			case 'u': /* Upload a file to a host */
				sscanf(man_msg, "%d %s %s", &dst, dat.heldFileName, name);
            new_job = (struct host_job *)
                  malloc(sizeof(struct host_job));
            new_job->type = JOB_REQUEST_ID_SEND;
            new_job->file_upload_dst = 100;
            for (i=0; name[i] != '\0'; i++) {
               new_job->domNameUp[i] = name[i];
            }
            new_job->domNameUp[i] = ' ';
            new_job->domNameUp[i+1] = 'u';
            //printf("Host: New_job->domNameUp: %s\n", new_job->domNameUp);
            new_job->domNameUp[i+2] = '\0';
            job_q_add(&job_q, new_job);





            /*new_job = (struct host_job *) 
						malloc(sizeof(struct host_job));
				new_job->type = JOB_FILE_UPLOAD_SEND;
				new_job->file_upload_dst = dst;	
				for (i=0; name[i] != '\0'; i++) {
					new_job->fname_upload[i] = name[i];
				}
				new_job->fname_upload[i] = '\0';
				job_q_add(&job_q, new_job);
            */
					
				break;

         case 'd': /* Dowload a file to a specified host */
//				printf("Download started\n");
			   sscanf(man_msg, "%d %s %s", &dst, dat.heldFileName, name);
            new_job = (struct host_job *)
                  malloc(sizeof(struct host_job));
            new_job->type = JOB_REQUEST_ID_SEND;
            new_job->file_upload_dst = 100;
            for (i=0; name[i] != '\0'; i++) {
               new_job->domNameUp[i] = name[i];
            }
            new_job->domNameUp[i] = ' ';
            new_job->domNameUp[i+1] = 'd';
            //printf("Host: New_job->domNameUp: %s\n", new_job->domNameUp);
            new_job->domNameUp[i+2] = '\0';
            job_q_add(&job_q, new_job);








            /*sscanf(man_msg, "%d %s", &dst, name);
				new_job = (struct host_job *) 
						malloc(sizeof(struct host_job));
				new_job->type = JOB_FILE_UPLOAD_SEND;
				new_job->file_download_dst = dst;	
				for (i=0; name[i] != '\0'; i++) {
					new_job->fname_download[i] = name[i];
				}
				new_job->fname_download[i] = '\0';
				job_q_add(&job_q, new_job);
				*/	
				break;
         case 'a': 
            //send alias to DNS node
            //printf("SENDING ALIAS\n");
            sscanf(man_msg, "%d %s", &dst, name);
            new_job = (struct host_job *)
                  malloc(sizeof(struct host_job));
            new_job->type = JOB_REGISTER_NAME_SEND;
            new_job->file_upload_dst = dst;
            for (i=0; name[i] != '\0'; i++) {
               new_job->domNameUp[i] = name[i];
            }
            new_job->domNameUp[i] = '\0';
            job_q_add(&job_q, new_job);
            break;
         case 'r':
            //printf("REQUEST STARTED\n");
            sscanf(man_msg, "%d %s", &dst, name);
            new_job = (struct host_job *)
                  malloc(sizeof(struct host_job));
            new_job->type = JOB_REQUEST_ID_SEND;
            new_job->file_upload_dst = dst;
            for (i=0; name[i] != '\0'; i++) {
               new_job->domNameUp[i] = name[i];
            }
            new_job->domNameUp[i] = '\0';
            job_q_add(&job_q, new_job);

               //request name through request packet
               //The DNS server will return a reply packet, which has an
               //indicator if the physical ID exists, and 
               //if it exists, the physical ID as well.
               //
            break;

			default:
			;
		}
	}
	
	/*
	 * Get packets from incoming links and translate to jobs
  	 * Put jobs in job queue
 	 */ 

   //printf("node port num = %d\n",node_port_num);

	for (k = 0; k < node_port_num; k++) { /* Scan all ports */

		in_packet = (struct packet *) malloc(sizeof(struct packet));
		n = packet_recv(node_port[k], in_packet);
      //printf("Host: %d is checking packets from host: %d and got: %d\n", 
      //host_id, k, n); 
		if ((n >= 0) && ((int) in_packet->dst == host_id)) {
         //printf("new job!\n");
			new_job = (struct host_job *) 
				malloc(sizeof(struct host_job));
			new_job->in_port_index = k;
			new_job->packet = in_packet;

			switch(in_packet->type) {
				/* Consider the packet type */

				/* 
				 * The next two packet types are 
				 * the ping request and ping reply
				 */
				case (char) PKT_PING_REQ: 
					new_job->type = JOB_PING_SEND_REPLY;
					job_q_add(&job_q, new_job);
					break;

				case (char) PKT_PING_REPLY:
					ping_reply_received = 1;
					free(in_packet);
					free(new_job);
					break;

				/* 
				 * The next two packet types
				 * are for the upload file operation.
				 *
				 * The first type is the start packet
				 * which includes the file name in
				 * the payload.
				 *
				 * The second type is the end packet
				 * which carries the content of the file
				 * in its payload
				 */
		
				case (char) PKT_FILE_UPLOAD_START:
					new_job->type 
						= JOB_FILE_UPLOAD_RECV_START;
					job_q_add(&job_q, new_job);
					break;

				case (char) PKT_FILE_UPLOAD_END:
					new_job->type 
						= JOB_FILE_UPLOAD_RECV_END;
					//printf("Receive packet\n");
					job_q_add(&job_q, new_job);
					break;

            case (char) PKT_FILE_DOWNLOAD_SEND:
					new_job->type
						= JOB_FILE_DOWNLOAD_RECV;
					job_q_add(&job_q, new_job);
					break;
            case (char) PKT_DNS_UPLOAD_END:
               new_job->type
                  = JOB_REGISTER_NAME_REC;
      //         printf("DNS Recieved Packet\n");
               job_q_add(&job_q, new_job);
               break;
            case (char) PKT_DNS_REQUEST_START:
               new_job->type
                  = JOB_REQUEST_ID_REC;
      //         printf("DNS Recieved Packet\n");
               job_q_add(&job_q, new_job);
               break;
            case (char) PKT_DNS_REQUEST_END:
               new_job->type
                  = JOB_REQUEST_ID_REC_END;
               //printf("HOST Recieved Packet DNS REQUEST\n");
               job_q_add(&job_q, new_job);
               break;
				default:
					free(in_packet);
					free(new_job);
			}
		}
		else {
			free(in_packet);
		}
	}

	/*
 	 * Execute one job in the job queue
 	 */

	if (job_q_num(&job_q) > 0) {

		//printf("Job queue size: %d\n",job_q_num(&job_q));

		/* Get a new job from the job queue */
		new_job = job_q_remove(&job_q);

		/* Send packet on all ports */
		switch(new_job->type) {

		/* Send packets on all ports */	
		case JOB_SEND_PKT_ALL_PORTS:
			for (k=0; k<node_port_num; k++) {
				packet_send(node_port[k], new_job->packet);
			}
			//printf("Sending packet\n");
			free(new_job->packet);
			free(new_job);
			break;

		/* The next three jobs deal with the pinging process */
		case JOB_PING_SEND_REPLY:
			/* Send a ping reply packet */

			/* Create ping reply packet */
			new_packet = (struct packet *) 
				malloc(sizeof(struct packet));
         //printf("new_job->packet->src: %d\n",new_job->packet->src);
			new_packet->dst = new_job->packet->src;
			new_packet->src = (char) host_id;
         //printf("new_packet->src: %d\n",new_packet->src);
			new_packet->type = PKT_PING_REPLY;
			new_packet->length = 0;

			/* Create job for the ping reply */
			new_job2 = (struct host_job *)
				malloc(sizeof(struct host_job));
			new_job2->type = JOB_SEND_PKT_ALL_PORTS;
			new_job2->packet = new_packet;

			/* Enter job in the job queue */
			job_q_add(&job_q, new_job2);

			/* Free old packet and job memory space */
			free(new_job->packet);
			free(new_job);
			break;

		case JOB_PING_WAIT_FOR_REPLY:
			/* Wait for a ping reply packet */

			if (ping_reply_received == 1) {
				n = sprintf(man_reply_msg, "Ping acked!"); 
				man_reply_msg[n] = '\0';
				write(man_port->send_fd, man_reply_msg, n+1);
				free(new_job);
			}
			else if (new_job->ping_timer > 1) {
				new_job->ping_timer--;
				job_q_add(&job_q, new_job);
			}
			else { /* Time out */
				n = sprintf(man_reply_msg, "Ping time out!"); 
				man_reply_msg[n] = '\0';
				write(man_port->send_fd, man_reply_msg, n+1);
				free(new_job);
			}

			break;	


		/* The next three jobs deal with uploading a file */

			/* This job is for the sending host */
		case JOB_FILE_UPLOAD_SEND:

			/* Open file */
//			if (dir_valid == 1) {
				n = sprintf(name, "./%s/%s", 
					dir, new_job->fname_upload);
				name[n] = '\0';
				fp = fopen(name, "r");
            printf("STARTING UPLOAD of: %s\n", name);
				if (fp != NULL) {
                printf("fileExists!\n\n");
				    /* 
					 * Create first packet which
					 * has the file name 
					 */
					new_packet = (struct packet *) 
						malloc(sizeof(struct packet));
					new_packet->dst 
						= new_job->file_upload_dst;
					new_packet->src = (char) host_id;
					new_packet->type 
						= PKT_FILE_UPLOAD_START;
					for (i=0; 
						new_job->fname_upload[i]!= '\0'; 
						i++) {
						new_packet->payload[i] = 
							new_job->fname_upload[i];
					}
					new_packet->length = i;

					new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
					new_job2->type = JOB_SEND_PKT_ALL_PORTS;
					new_job2->packet = new_packet;
					job_q_add(&job_q, new_job2);

					int continueFlag = 1;

					while(continueFlag){

						/* 
						* Create the second packet which
						* has the file contents
						*/
					
						new_packet = (struct packet *) malloc(sizeof(struct packet));
						new_packet->dst 
							= new_job->file_upload_dst;
						new_packet->src = (char) host_id;
						new_packet->type = PKT_FILE_UPLOAD_END;

						n = fread(string,sizeof(char),
						PKT_PAYLOAD_MAX, fp);	
						string[n] = '\0';

						if(n < PKT_PAYLOAD_MAX){
							//printf("s = %s\n",string);
							//printf("n = %d\n",n);
							continueFlag = 0;
						} else {
							//printf("s = %s\n",string);
							//printf("n = %d\n",n);
							continueFlag = 1;
						}

						for (i=0; i<n; i++) {
							new_packet->payload[i] 
								= string[i];
						}

						new_packet->length = n;

						/*
						* Create a job to send the packet
						* and put the job in the job queue
						*/

						new_job2 = (struct host_job *)
							malloc(sizeof(struct host_job));
						new_job2->type 
							= JOB_SEND_PKT_ALL_PORTS;
						new_job2->packet = new_packet;
						job_q_add(&job_q, new_job2);
					}
					
					fclose(fp);
					free(new_job);

/*				} else {  
               printf("HOST.c: DIR Not Valid\n\n");
					free(new_job);

				}
*/
			}
			
			break;

			/* The next two jobs are for the receving host */

		case JOB_FILE_UPLOAD_RECV_START:

			/* Initialize the file buffer data structure */
			file_buf_init(&f_buf_upload);

			/* 
			 * Transfer the file name in the packet payload
			 * to the file buffer data structure
			 */

         //printf("new_job->packet->payload: %s\n",new_job->packet->payload);

			file_buf_put_name(&f_buf_upload, 
				new_job->packet->payload, 
				new_job->packet->length);

			free(new_job->packet);
			free(new_job);
			break;

		case JOB_FILE_UPLOAD_RECV_END:

			/* 
			 * Download packet payload into file buffer 
			 * data structure 
			 */

			file_buf_add(&f_buf_upload, 
			new_job->packet->payload,
			new_job->packet->length);

			free(new_job->packet);
			free(new_job);

			if (dir_valid == 1) {

				n = sprintf(name, "./%s/%s", dir, oldFileName);
				name[n] = '\0';
				fp = fopen(name, "a");

				if (fp != NULL) {
					/* 
					 * Write contents in the file
					 * buffer into file
					 */

					while (f_buf_upload.occ > 0) {
						n = file_buf_remove(
							&f_buf_upload, 
							string,
							PKT_PAYLOAD_MAX);
						string[n] = '\0';
						n = fwrite(string,
							sizeof(char),
							n, 
							fp);
					}

					fclose(fp);
				}	
			}
			break;

      case JOB_FILE_DOWNLOAD_SEND: 
				/* 
				* Create packet which
				* has the file name and src
				*/

				for(i = 0; new_job->fname_download[i] != '\0'; i++);

				new_packet = (struct packet *) 
					malloc(sizeof(struct packet));
				new_packet->dst 
					= new_job->file_download_dst;
				new_packet->src = (char) host_id;
				new_packet->type 
					= PKT_FILE_DOWNLOAD_SEND;
				// for (i=0; 
				// 	new_job->fname_download[i]!= '\0'; 
				// 	i++) {
				// 	new_packet->payload[i] = 
				// 		new_job->fname_download[i];
				// }

				strcpy(new_packet->payload, new_job->fname_download);
				new_packet->length = i;

				new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
				new_job2->type = JOB_SEND_PKT_ALL_PORTS;
				new_job2->packet = new_packet;
				job_q_add(&job_q, new_job2);
			
			
			free(new_job);
			break;

      case JOB_FILE_DOWNLOAD_RECV:

			//Parse received packet

			//Extract origin ID and file name

			//Pass extracted data to upload start and upload end

			new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
			new_job2->type = JOB_FILE_UPLOAD_SEND;
			strcpy(new_job2->fname_upload, new_job->packet->payload);
			new_job2->file_upload_dst = new_job->packet->src;
			job_q_add(&job_q, new_job2);
			printf("\n\ndownload received\n\n");

			break;

      case JOB_REGISTER_NAME_SEND:
               //printf("%s\n", name);
               n = sprintf(name, new_job->domNameUp);
               
               new_packet = (struct packet *)
                  malloc(sizeof(struct packet));
               new_packet->dst
                  = new_job->file_upload_dst;
               //printf("Dest: %d\n", new_packet->dst);
               new_packet->src = (char) host_id;
               new_packet->type
                  = PKT_DNS_UPLOAD_END;
               for (i=0; new_job->domNameUp[i]!= '\0'; i++) {
                  new_packet->payload[i] =
                     new_job->domNameUp[i];
                //  printf("%c", new_job->domNameUp[i]);
               }
               //printf("\n");
               new_packet->length = i;
               
               new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
               new_job2->type = JOB_SEND_PKT_ALL_PORTS;
               new_job2->packet = new_packet;
               job_q_add(&job_q, new_job2);
           //    printf("prepping packet to SEND registration\n\n");
               free(new_job);
      break;

      case JOB_REQUEST_ID_SEND:
         printf("REQUEST ID SENT\n\n");
         n = sprintf(name, new_job->domNameUp);

         new_packet = (struct packet *)
            malloc(sizeof(struct packet));
         new_packet->dst
            = new_job->file_upload_dst;
         //printf("Dest: %d\n", new_packet->dst);
         new_packet->src = (char) host_id;
         new_packet->type
            = PKT_DNS_REQUEST_START;
         for (i=0; new_job->domNameUp[i]!= '\0'; i++) {
            new_packet->payload[i] =
               new_job->domNameUp[i];
           // printf("%c", new_job->domNameUp[i]);
         }
        // printf("\n");
         new_packet->length = i;

         new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
         new_job2->type = JOB_SEND_PKT_ALL_PORTS;
         new_job2->packet = new_packet;
         job_q_add(&job_q, new_job2);
         //printf("prepping packet to SEND request\n\n");
         free(new_job);
      break;
           case JOB_REGISTER_NAME_REC:
               /* Initialize the file buffer data structure */
               //file_buf_init(&f_buf_upload);


               //file_buf_add(&f_buf_upload,
               //new_job->packet->payload,
               //new_job->packet->length);
               //printf("%s\n\n", new_job->packet->payload);
               //printf("HIT");
               int j = new_job->packet->src;
               //free(new_job->packet);
               //free(new_job);
               char newDName[50];
               //n = sprintf(name, newDName);
               //input into table
               strcpy(newDName,new_job->packet->payload);
               AssignToTable(j,newDName);
               free(new_job->packet);
               free(new_job);
               //AssignToTable(j,newDName);
               printf("REGISTERING NAME from %d\n\n", j);
      break;

      case JOB_REQUEST_ID_REC:
               //printf("ID REQUEST FOUND! Requesting: %s\n\n", new_job->packet->payload);
               int k = -1;
               const char s[] = " ";
               //int i = strlen(new_job->packet->payload);
               char * y;
               char * z;
               y = new_job->packet->payload;
               strtok(y, s);
               z = strtok(NULL, s);
               //printf("Extra command %s, %s", y, z);
               int x = new_job->packet->src;
               k = RequestID(y);
               if(k == -1){ 
                  printf("Domain Name: %s not in system\n\n", y); 
                  free(new_job->packet);
                  free(new_job);
                  break;
               }
               else
               {
                  char newSrc[4];
                  sprintf(newSrc, "%d", x);
                  printf("\n\n Domain Name Found! Returning id: %d From: %d \n\n", k, host_id);
                  char info[4];
                  sprintf(info, "%d", k);
                 // printf("info is: %s\n", info);
                 

                  //prep new packet`
                  new_packet = (struct packet *) malloc(sizeof(struct packet));
                  new_packet->dst = (char)x;


                  //printf("\n\n Domain Name Found! Returning id: %c From: %d\n\n", new_packet->dst, host_id);

                  new_packet->src = (char) host_id;
                  new_packet->type = PKT_DNS_REQUEST_END;
                  for (i=0; info[i]!= '\0'; i++) {
                     new_packet->payload[i] = info[i];
                     //if (info[i+1] == '\0')
                        //new_packet->payload[i] = z[0];
                     //printf("%c", new_packet->payload[i]);
                  }
                  if (z!=NULL){
                     new_packet->payload[i] = ' ';
                     new_packet->payload[i+1] = z[0];
                     printf("%s", new_packet->payload);
                     new_packet->length = i+2;
                  }else{
                     printf("No extra command %s", new_packet->payload);
                     new_packet->length = i;
                  }
                  printf("\n\n");
                  new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                  new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                  new_job2->packet = new_packet;
                  job_q_add(&job_q, new_job2);
                  //printf("Sent Packets on all ports cp\n\n");
               }
               free(new_job->packet);
               free(new_job);
               //printf("cp FINAL\n\n");
      break;

      case JOB_REQUEST_ID_REC_END:
               //printf("HOST %d recieved request return packet!\n\nRequested ID Given Domain Name is: %s\n\n", host_id, new_job->packet->payload[0]);
               //printf("Domain Name Request Responce! ID: %s\n\n", new_job->packet->payload);   
               char command = ' ';
               int requestedID = atoi(new_job->packet->payload);
               if(strlen(new_job->packet->payload) > 2){
                  printf("RequestedID is %d and new command is %c\n\n", requestedID,new_job->packet->payload[2]);
                  command = new_job->packet->payload[2];
               }
               free(new_job->packet);
               free(new_job);
               
               if(command == 'p'){
                  //char str[2];
                  //sprintf(str, "%d", requestedID);
                  //printf("HIT\n\n");
                  //printf("(char) requestedId: %c\n\n", str[0]);
                  new_packet = (struct packet *)
                  malloc(sizeof(struct packet));
                  new_packet->src = (char) host_id;
                  new_packet->dst = (char) requestedID;
                  new_packet->type = (char) PKT_PING_REQ;
                  new_packet->length = 0;
                  new_job = (struct host_job *)
                        malloc(sizeof(struct host_job));
                  new_job->packet = new_packet;
                  new_job->type = JOB_SEND_PKT_ALL_PORTS;
                  job_q_add(&job_q, new_job);

                  new_job2 = (struct host_job *)
                        malloc(sizeof(struct host_job));
                  ping_reply_received = 0;
                  new_job2->type = JOB_PING_WAIT_FOR_REPLY;
                  new_job2->ping_timer = 10;
                  job_q_add(&job_q, new_job2);
               }else if (command == 'u'){
                  
                  new_job = (struct host_job *)
                  malloc(sizeof(struct host_job));
                  new_job->type = JOB_FILE_UPLOAD_SEND;
                  new_job->file_upload_dst = (char) requestedID;
                  printf("Upload requested ID is: %d. FileName is: %s\n", requestedID, dat.heldFileName);
                  for (i=0; dat.heldFileName[i] != '\0'; i++) {
                     new_job->fname_upload[i] = dat.heldFileName[i];
                  }
                  new_job->fname_upload[i] = '\0';
                  job_q_add(&job_q, new_job);
               }else if (command == 'd'){
                  new_job = (struct host_job *)
                  malloc(sizeof(struct host_job));
                  new_job->type = JOB_FILE_DOWNLOAD_SEND;
                  new_job->file_download_dst = (char) requestedID;
                  printf("Download requested ID is: %d. FileName is: %s\n", requestedID, dat.heldFileName);
                  for (i=0; dat.heldFileName[i] != '\0'; i++) {
                     new_job->fname_download[i] = dat.heldFileName[i];
                  }
                  new_job->fname_download[i] = '\0';
                  job_q_add(&job_q, new_job);
               }


               //printf("HIT\n\n");
      break;
      }
	}


	/* The host goes to sleep for 10 ms */
	usleep(TENMILLISEC);

} /* End of while loop */

}


