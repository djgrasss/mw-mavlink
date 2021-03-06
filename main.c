
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>


#include "mw.h"
#include "udp.h"
#include "mavlink.h"
#include "def.h"
#include "global.h"


uint8_t debug = 0;

uint8_t stop = 0;
uint16_t loop_counter = 0;

void check_incoming_udp(); //checks for messages on UDP port

#define HEARTBEAT_LIFE 3000/LOOP_MS
//heartbeat is used to trigger mavlink failsafe as defined in emergency in mavlink.c
//apart from this MultiWii firmware has its own failsafe (if set in config.h @ 50Hz) that is based around SET_RAW_RC
//mavlink will feed SET_RAW_RC as long as RC_TIMOUT>0 (mw.c, expressed in LOOP_MS)

uint16_t heartbeat = 0;

typedef void (*t_cb)();

struct _S_TASK {
	uint16_t freq; //has to be less than loop_counter max value
	t_cb cb_fn;
};
typedef struct _S_TASK S_TASK;

#define MAX_TASK 3

static S_TASK task[MAX_TASK] = {
	{1, check_incoming_udp}, //run every LOOP_MS (see global.h)
	{1, mw_loop},
	{1, mavlink_loop}
};

static mavlink_message_t mav_msg;


void check_incoming_udp() {
	if (heartbeat) heartbeat--;

	while (udp_recv(&mav_msg)) {
		if (debug) printf("<- MsgID: %u\n",mav_msg.msgid);
		switch (mav_msg.msgid) {
			case MAVLINK_MSG_ID_HEARTBEAT:
				heartbeat = HEARTBEAT_LIFE;
			 	break;

			case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
				msg_param_request_list(&mav_msg);
				break;
			case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
				msg_param_request_read(&mav_msg);
				break;
			case MAVLINK_MSG_ID_PARAM_SET:
				msg_param_set(&mav_msg);
				break;				
			case MAVLINK_MSG_ID_MISSION_REQUEST_LIST:
				msg_mission_request_list(&mav_msg);
				break;
			case MAVLINK_MSG_ID_COMMAND_LONG:
				msg_command_long(&mav_msg);
				break;	
			case MAVLINK_MSG_ID_MANUAL_CONTROL:
				msg_manual_control(&mav_msg);
				break;			
			default: printf("Unknown message id: %u\n",mav_msg.msgid);
		}
		//process message
	}
}



void mssleep(unsigned int ms) {
  struct timespec tim;
   tim.tv_sec = ms/1000;
   tim.tv_nsec = 1000000L * (ms % 1000);
   if(nanosleep(&tim , &tim) < 0 )
   {
      printf("Nano sleep system call failed \n");
   }
}


//runs all tasks as per defined frequency
void loop() {
	uint8_t i;
	loop_counter=0;
	while (!stop) {

		for (i=0;i<MAX_TASK;i++)
			if (loop_counter%task[i].freq==0) {
				task[i].cb_fn();
			}

		mssleep(LOOP_MS);
		loop_counter++;
		if (loop_counter==1000) loop_counter=0;
	}
}

char target_ip[64];
int target_port=14550, local_port;

void print_usage() {
    printf("Usage:\n");
	printf("-h\thelp\n");
    printf("-t TARGET\tip address of QGroundControl\n");
    printf("-p PORT\tQGroundControl port to use (default: %i)\n",target_port);
    printf("-l PORT\tlocal port to use\n");
    printf("-d for debug\n");
}

int set_defaults(int c, char **a) {
	int required = 2;
    int option;
    while ((option = getopt(c, a,"ht:p:l:d")) != -1) {
        switch (option)  {
            case 't': strcpy(target_ip,optarg); required--; break;
            case 'p': target_port = atoi(optarg); break;
            case 'l': local_port = atoi(optarg); required--; break;
            case 'd': debug = 1; break;
            default: print_usage(); return -1;
        }
    }
   	
   	if (required) {
   		print_usage();
   		return -1;
   	}

   	return 0;
}

void catch_signal(int sig)
{
        stop = 1;
}

int main(int argc, char* argv[])
{
	signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);

    dbg_init(0); //0b11111111 init the mw library debug

    if (set_defaults(argc,argv)) {
    	return -1;
    }

    printf("Initializing UDP...\n");
 	udp_init(target_ip,target_port,local_port);

 	printf("Setting up mw...\n");
 	if (mw_init()) {
 		printf("Error mw_init!\n");
 		return -1;
 	}

 	printf("Initializing PARAMS...\n");
 	params_init(); 	

  	printf("Setting up mavlink...\n");
 	if (mavlink_init()) {
  		printf("Error mavlink!\n");
 		return -1;		 
 	}	

 	printf("Started.\n");
 	loop();
 	
 	printf("Cleaning up...\n");
 	mavlink_end();

 	params_end();

 	mw_end();

 	udp_close();

 	printf("Bye.\n");
 	return 0;
}
 

