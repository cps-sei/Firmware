#include <px4_config.h>
#include <px4_tasks.h>
#include <px4_posix.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>

extern "C"{
#include <checkpointapi.h>
}


extern "C" __EXPORT int checkpoint_main(int argc, char *argv[]);

int fid=-1;
int ckpid;

static void do_save() {

  if ((fid = open("/dev/checkpoint0",O_RDWR)) <0){
    printf("error trying to open /dev/checkpoint0\n");
    return;
  }
  
  PX4_INFO("checkpoint: saving");
  
  if ((ckpid = ckp_create_checkpoint(fid,getpid()))<0){
    printf("could not create the checkpoint\n");
    return;
  }
    
}

static void do_restore() {

  if (fid <0 ){
    printf("checkpoint module not open\n");
    return;
  }

  PX4_INFO("checkpoint: restoring");

  if (ckp_rollback(fid, ckpid)<0){
    printf("Could not rollback\n");
    return;
  }
  
}

static int usage()
{
  PX4_WARN("usage: checkpoint {save|restore}");
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
  } else {
    return usage();
  }
  
  return OK;
}


