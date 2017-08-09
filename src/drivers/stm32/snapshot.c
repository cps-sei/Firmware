#include <stdbool.h>
#include <sys/types.h>
#include <syslog.h>
#include <nuttx/progmem.h>

#include <systemlib/param/param.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/rc_channels.h>
#include <uORB/topics/manual_control_setpoint.h>

#include <drivers/drv_hrt.h>

#ifndef CONFIG_ARCH_BOARD_PX4IO_V2
volatile bool snp_in_reboot = false;
volatile bool snp_do_save = false;
volatile bool snp_do_restore = false;

#define SRAM_IMAGE_START	(0x08120000)
#define SRAM_IMAGE_SIZE		(0x30000)
#define SRAM_IMAGE_END		(SRAM_IMAGE_START + SRAM_IMAGE_SIZE)
#define CCRAM_IMAGE_START	(0x08160000)
#define CCRAM_IMAGE_SIZE	(0x8000)
#define CCRAM_IMAGE_END		(CCRAM_IMAGE_START + CCRAM_IMAGE_SIZE)

void prepare_for_reboot(void);
void resync_with_px4io(void);

int stm32_do_save(void);
int stm32_do_save(void)
{
	ssize_t s;

	syslog(LOG_INFO, "storing snapshot\n");

	//SAVE SNAPSHOT
	if (*(uint32_t *)SRAM_IMAGE_START != 0xffffffff) {
	  syslog(LOG_ERR, "section already used\n");
		return -1;
	}

	snp_in_reboot = true;	/* turned off once we have 'sync'-ed back */
	asm volatile("dsb" ::: "memory");

	s = 0;
	s += up_progmem_write(SRAM_IMAGE_START,
			      (const void *)0x20000000,
			      SRAM_IMAGE_SIZE);
	s += up_progmem_write(CCRAM_IMAGE_START,
			      (const void *)0x10000000,
			      CCRAM_IMAGE_SIZE);
	snp_in_reboot = false;

	syslog(LOG_INFO, "done %x\n", s);

	return 0;
}

extern void record_dwt(void);

void stm32_do_restore(void);
void stm32_do_restore(void)
{
	uint32_t s, d;

	//RESTORE SNAPSHOT
	for (s = SRAM_IMAGE_START, d = 0x20000000;
	     s < SRAM_IMAGE_END;
	     s += 4, d += 4) {
		*(uint32_t *)d = *(uint32_t *)s;
	}

	for (s = CCRAM_IMAGE_START, d = 0x10000000;
	     s < CCRAM_IMAGE_END;
	     s += 4, d += 4) {
		*(uint32_t *)d = *(uint32_t *)s;
	}

	asm volatile("dsb" ::: "memory");
}
#endif

bool px4io_done;

bool snapshot_safe(void);
bool snapshot_safe(void)
{
	return px4io_done;
}

static struct vehicle_command_s rearm_cmd;
static orb_advert_t com_pub;

void prepare_for_reboot(void)
{
	param_t sys_id_param = param_find("MAV_SYS_ID");
	param_t comp_id_param = param_find("MAV_COMP_ID");

	memset(&rearm_cmd, 0, sizeof (rearm_cmd));
	com_pub = orb_advertise_queue(ORB_ID(vehicle_command),
				      &rearm_cmd,
				      ORB_QUEUE_LENGTH);

	param_get(sys_id_param, &rearm_cmd.target_system);
	param_get(comp_id_param, &rearm_cmd.target_component);
	rearm_cmd.source_system = rearm_cmd.target_system;
	rearm_cmd.source_component = rearm_cmd.target_component;
	rearm_cmd.param1 = 1.0f;
	rearm_cmd.param2 = 0;
	rearm_cmd.param3 = 0;
	rearm_cmd.param4 = 0;
	rearm_cmd.param5 = 0;
	rearm_cmd.param6 = 0;
	rearm_cmd.param7 = 0;
	rearm_cmd.command = VEHICLE_CMD_COMPONENT_ARM_DISARM;
	rearm_cmd.confirmation = 1;
}

void resync_with_px4io(void)
{
	if (snp_in_reboot) {
		//this may lead to a context switch?
		orb_publish(ORB_ID(vehicle_command), com_pub, &rearm_cmd);
		snp_in_reboot = false;
	}
}

static hrt_abstime
cycletime(void)
{
	static uint64_t basetime __attribute__((section(".bss.across_reboot")));
	static uint32_t lasttime __attribute__((section(".bss.across_reboot")));
	uint32_t cycles;

	cycles = *(unsigned long *)0xe0001004;

	if (cycles < lasttime) {
		basetime += 0x100000000ULL;
	}

	lasttime = cycles;

	return (basetime + cycles) / 168;	/* XXX magic number */
}

static hrt_abstime t __attribute__((section(".data.across_reboot"))) = 0;

extern bool mavlink_boot_complete(void);

static bool snapshot_request = false;
static bool reboot_request = false;

static void poll_rc_channels(void) {
	static int sub = 0;

	bool updated = false;
	struct rc_channels_s rc;
	int ch;
	float value;

	if (sub == 0) {
		sub = orb_subscribe(ORB_ID(rc_channels));
	}

	orb_check(sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(rc_channels), sub, &rc);

		ch = rc.function[RC_CHANNELS_FUNCTION_AUX_1];
		value = 0.5f * rc.channels[ch] + 0.5f;
		reboot_request = (value > 0.5f);

		ch = rc.function[RC_CHANNELS_FUNCTION_AUX_2];
		value = 0.5f * rc.channels[ch] + 0.5f;
		snapshot_request = (value > 0.5f);
	}
}

static void poll_manual_control_setpoint(void) {
	static int sub = 0;

	bool updated = false;
	struct manual_control_setpoint_s manual;

	if (sub == 0) {
		sub = orb_subscribe(ORB_ID(manual_control_setpoint));
	}

	orb_check(sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(manual_control_setpoint), sub, &manual);

		reboot_request = (manual.aux1 > 0.5f);
		snapshot_request = (manual.aux2 > 0.5f);
	}
}

void poll_reboot(void);
void poll_reboot(void)
{
	hrt_abstime t2;
	static int32_t period = -1;
	static hrt_abstime last_param_get;

	if (!mavlink_boot_complete()) {
		return;
	}
	
	t2 = cycletime();

	// check for param update every 2 seconds
	if (period == -1 || t2 - last_param_get > 2000000) {
		param_get(param_find("YL_REBOOT_PERIOD"), &period);
		last_param_get = cycletime();
		if (period < 1000000) {
			period = 1000000;
		}
	}

	if (t2 - t > period) {
		poll_rc_channels();
		poll_manual_control_setpoint();
		if (reboot_request) {
			t = cycletime();
			snp_do_restore = true;
			syslog(LOG_INFO, "restore detected, %lld\n", t);
		}
	}
}

bool snapshot_taken __attribute__((section(".data.across_reboot"))) = false;

void poll_snapshot(void);
void poll_snapshot(void)
{
	if (!snapshot_taken) {
		poll_rc_channels();
		poll_manual_control_setpoint();
		if (snapshot_request) {
			snp_do_save = true;
			snapshot_taken = true;
			syslog(LOG_INFO, "save detected, %lld\n", t);
		}
	}
}

#if 0
#include <stdint.h>
#include <stdlib.h>
#include <px4_log.h>

#if UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xe2dee396
#else
#define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif

uintptr_t __stack_chk_guard = STACK_CHK_GUARD;
__attribute__((noreturn))
	void __stack_chk_fail(void);

__attribute__((noreturn))
	void __stack_chk_fail(void)
{
#if __STDC_HOSTED__
	abort();
#else
	PX4_PANIC();
#endif
}
#endif
