#include <px4_config.h>
#include <px4_tasks.h>
#include <px4_posix.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <sys/time.h>
#include <sys/mman.h>


extern "C"{
#include <checkpointapi.h>
}

#include "checkpoint_keep.h"

extern "C" __EXPORT int checkpoint_main(int argc, char *argv[]);

int fid=-1;
int ckpid;

static int get_timestamp(struct timespec *tp){
	struct timeval now;
	int rv = gettimeofday(&now, NULL);
	if (rv){
	  return rv;
	}
	tp->tv_sec = now.tv_sec;
	tp->tv_nsec = now.tv_usec * 1000;
	return 0;
}

static void do_save() {
  
  if ((fid = open("/dev/checkpoint0",O_RDWR)) <0){
    printf("error trying to open /dev/checkpoint0\n");
    return;
  }

  if (ckp_checkpoint_timestamp == NULL){
    // Dio: Allocate memory for the checkpoint timestamp
    if ((ckp_checkpoint_timestamp = (struct timespec *)malloc(sizeof(struct timespec)))== NULL){
      perror("trying to allocate memory for checkpoint timestamp");
      return;
    }
    // Initizalize
    ckp_checkpoint_timestamp->tv_sec = 0;
    ckp_checkpoint_timestamp->tv_nsec = 0;
  
    // Dio: allocate a page for persistent storage across rollbacks
    // At this point only the rollback timestamp will be stored
    ckp_rollback_timestamp = (struct timespec *) mmap(NULL,sysconf(_SC_PAGE_SIZE),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_LOCKED,-1,0);
    if (ckp_rollback_timestamp == NULL){
      perror("trying to allocate persistent storage");
      free(ckp_checkpoint_timestamp);
      ckp_checkpoint_timestamp = NULL;
      return;
    }
    ckp_rollback_timestamp->tv_sec = 0;
    ckp_rollback_timestamp->tv_nsec = 0;
  }

  if (mlockall(MCL_CURRENT | MCL_FUTURE)<0){
    perror("trying mlockall");
    return;
  }
  

  PX4_INFO("checkpoint: saving");

  if (get_timestamp(ckp_checkpoint_timestamp)){
    printf("error getting timestamp\n");
  }
  
  if ((ckpid = ckp_create_checkpoint(fid,getpid(),ckp_rollback_timestamp,sizeof(struct timespec)))<0){
    printf("could not create the checkpoint\n");
    //return;
  }

  if (ckp_rollback_timestamp->tv_sec != 0 || ckp_rollback_timestamp->tv_nsec != 0){
    printf("Returning from rollback with rollback timestamp: %d s: %d ns\n",
	   ckp_rollback_timestamp->tv_sec,ckp_rollback_timestamp->tv_nsec);
    printf("\t and checkpoint timestamp: %d s:%d ns\n",
	   ckp_checkpoint_timestamp->tv_sec,ckp_checkpoint_timestamp->tv_nsec);
    // reset the clock delay if any -- attempt 1
    // hrt_start_delay();
  } else {
    printf("Just created checkpoint or rollback timestamp not restored\n");
  }
}

static void do_restore() {

  if (fid <0 ){
    printf("checkpoint module not open\n");
    return;
  }

  PX4_INFO("checkpoint: restoring");

  if (get_timestamp(ckp_rollback_timestamp)){
    printf("error getting timestamp\n");
  }
  printf("rollback timestamp: %d s, %d ns\n",ckp_rollback_timestamp->tv_sec, ckp_rollback_timestamp->tv_nsec);
  printf("checkpoint timestamp: %d s, %d ns\n",ckp_checkpoint_timestamp->tv_sec, ckp_checkpoint_timestamp->tv_nsec);
  
  // Do a rollback but sending the rollback timestamp so that the syscall will
  // preserve its value after the rest of the memory is rollbacked.
  // This way we can use the timestamp from the checkpoint and the rollback to
  // calculate the timestamp reset
  if (ckp_rollback(fid, ckpid, ckp_rollback_timestamp, sizeof(struct timespec))<0){
    printf("Could not rollback\n");
    return;
  }
}

static void do_timer(){
  if (fid <0){
    printf("checkpoint module not open\n");
    return;
  }

  PX4_INFO("checkpint: setting timer");

  if (ckp_set_rollback_timer(fid, ckpid, 2,0)<0){
    printf("could not set timer\n");
    return;
  }
}

static int usage()
{
  PX4_WARN("usage: checkpoint {save|restore|timer}");
  return 1;
}


int checkpoint_main(int argc, char *argv[])
{
  if (argc != 2) {
    return usage();
  }

  if (!strcmp(argv[1], "save")) {
    do_save();
  } else if (!strcmp(argv[1], "restore")) {
    do_restore();
  } else if (!strcmp(argv[1],"timer")) {
    do_timer();
  } else {
    return usage();
  }
  
  return OK;
}


