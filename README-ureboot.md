To build the POSIX version with microreboots:

Prerequisites:
- clone [the microreboot kernel modules](https://bitbucket.org/seicps/yolo-linux-microreboot)
- build them

Build PX4 with microreboots:
- set the UREBOOT_HOME environment variable to the path to `yolo-linux-microreboot`
- build PX4 with `make posix_sitl_ureboot`
- run PX4 with `Tools/sitl_nodll_run.sh`
