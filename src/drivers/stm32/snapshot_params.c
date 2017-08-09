#include <px4_config.h>
#include <systemlib/param/param.h>

/**
 * Period of reboots in microseconds
 *
 * @group YOLO
 * @min 0
 */
PARAM_DEFINE_INT32(YL_REBOOT_PERIOD, 1000000);

