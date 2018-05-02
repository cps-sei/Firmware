/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
 *   Author: @author Lorenz Meier <lm@inf.ethz.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file reboot.c
 * Tool similar to UNIX reboot command
 */

#include <px4_config.h>
#include <px4_getopt.h>
#include <px4_log.h>
#include <px4_module.h>
#include <px4_shutdown.h>
#include <systemlib/systemlib.h>
#include <string.h>
#include <drivers/drv_hrt.h>

#include <stdbool.h>
#include <syslog.h>
#include <sched.h>
#include <unistd.h>
#include "systemlib/cpuload.h"

extern volatile bool snp_in_reboot;
extern volatile bool snp_do_save;
extern volatile bool snp_do_restore;

__EXPORT int reboot_main(int argc, char *argv[]);

static void print_usage(void)
{
	PRINT_MODULE_DESCRIPTION("Reboot the system");

	PRINT_MODULE_USAGE_NAME_SIMPLE("reboot", "command");
	PRINT_MODULE_USAGE_PARAM_FLAG('b', "Reboot into bootloader", true);
#ifdef __PX4_NUTTX
	PRINT_MODULE_USAGE_PARAM_FLAG('d', "Done (stop logging debug info)", true);
	PRINT_MODULE_USAGE_PARAM_FLAG('s', "Take snapshot (can only be done once)", true);
	PRINT_MODULE_USAGE_PARAM_FLAG('r', "Restore the snapshot state", true);
	PRINT_MODULE_USAGE_PARAM_FLAG('w', "Warm reboot", true);
#endif
	PRINT_MODULE_USAGE_ARG("lock|unlock", "Take/release the shutdown lock (for testing)", true);
}

#ifdef __PX4_NUTTX
static const char *OPTIONS = "bdsrw";
#else
static const char *OPTIONS = "b";
#endif

int reboot_main(int argc, char *argv[])
{
	int ch;
	bool to_bootloader = false;

	int myoptind = 1;
	const char *myoptarg = NULL;

	while ((ch = px4_getopt(argc, argv, OPTIONS, &myoptind, &myoptarg)) != -1) {
		switch (ch) {
		case 'b':
			to_bootloader = true;
			break;
#ifdef __PX4_NUTTX

		case 'd': /* done (stop logging debug info) */
			syslog(LOG_INFO, "snp_in_reboot was: %d\n", snp_in_reboot);
			snp_in_reboot = false;
			return 0;

		case 's': /* save */
			snp_do_save = true;
			sleep(2); // delay nsh>
			return 0;

		case 'r': /* restore */
			*(volatile uint32_t *)0x40002854 = 0x11111111;
			snp_do_restore = true;
			sleep(2); // delay nsh>
			return 0;

		case 'w':
			asm volatile("cpsid i");
			syslog(LOG_INFO, "warm reboot\n");
			{
				uint32_t addr;
#define NVIC_ICER_S	(0xE000E180)
#define NVIC_ICER_E	(0xE000E1C0)

				for (addr = NVIC_ICER_S;
				     addr < NVIC_ICER_E;
				     addr += 4) {
					*(volatile uint32_t *)addr = 0xFFFFFFFF;
				}

#define SYST_CSR	(0xE000E010)
				addr = SYST_CSR;
				*(volatile uint32_t *)addr = 0x0;
			}
			{
				uint32_t primask, faultmask, basepri;
				asm volatile("mrs %0, PRIMASK" : "=r"(primask));
				asm volatile("mrs %0, FAULTMASK" : "=r"(faultmask));
				asm volatile("mrs %0, BASEPRI" : "=r"(basepri));
				syslog(LOG_INFO, "primask: %x, faultmask: %x, basepri: %x\n",
				       primask, faultmask, basepri);
			}
			asm volatile("cpsie i");

			stm32_pwr_enablebkp(true);
			*(volatile uint32_t *)0x40002854 = 0x22222222;
			asm volatile("dsb");

			asm volatile("msr msp, %0\n"
				     "bx %1\n"
				     :
				     : "r"(*(uint32_t *)0x08004000),
				     "r"(*(uint32_t *)0x08004004));

			for (;;);

			break;
#endif

		default:
			printf("hrt=%"PRIu64"\n", hrt_absolute_time());
			print_usage();
			return 1;

		}
	}

	if (myoptind >= 0 && myoptind < argc) {
		int ret = -1;

		if (strcmp(argv[myoptind], "lock") == 0) {
			ret = px4_shutdown_lock();

			if (ret != 0) {
				PX4_ERR("lock failed (%i)", ret);
			}
		}

		if (strcmp(argv[myoptind], "unlock") == 0) {
			ret = px4_shutdown_unlock();

			if (ret != 0) {
				PX4_ERR("unlock failed (%i)", ret);
			}
		}

		return ret;
	}

	int ret = px4_shutdown_request(true, to_bootloader);

	if (ret < 0) {
		PX4_ERR("reboot failed (%i)", ret);
		return -1;
	}

	while (1) { usleep(1); } // this command should not return on success

	return 0;
}
