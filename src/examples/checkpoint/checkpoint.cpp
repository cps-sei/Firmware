#include <px4_config.h>
#include <px4_tasks.h>
#include <px4_posix.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>

extern "C" __EXPORT int checkpoint_main(int argc, char *argv[]);

static void do_save() {
  PX4_INFO("checkpoint: save");
}

static void do_restore() {
  PX4_INFO("checkpoint: restore");
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


