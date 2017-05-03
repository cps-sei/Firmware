/****************************************************************************
 *
 *   Copyright (c) 2015-2016 PX4 Development Team. All rights reserved.
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

#include <stdint.h>

#include <px4_tasks.h>
#include <px4_getopt.h>
#include <px4_posix.h>
#include <errno.h>
#include <cmath>	// NAN

#include <uORB/uORB.h>
#include <uORB/topics/actuator_outputs.h>
#include <uORB/topics/actuator_armed.h>

#include <drivers/drv_hrt.h>

#include <systemlib/param/param.h>
#include <systemlib/pwm_limit/pwm_limit.h>
#include <iostream>

namespace navio_sysfs_pwm_hil
{
static px4_task_t _task_handle = -1;
volatile bool _task_should_exit = false;
static bool _is_running = false;

static const int NUM_PWM = 4;
static char _device[64] = "/sys/class/pwm/pwmchip0";
static int  _pwm_fd[NUM_PWM];

static const int FREQUENCY_PWM = 400;

// subscriptions
int     _armed_sub;

// publications
orb_advert_t    _outputs_pub = nullptr;
orb_advert_t    _rc_pub = nullptr;

// topic structures
actuator_outputs_s  _outputs;
actuator_armed_s    _armed;

// polling
uint8_t _poll_fds_num = 0;
px4_pollfd_struct_t _poll_fds[NUM_PWM];

// control groups related
uint32_t	_groups_required = 0;
uint32_t	_groups_subscribed = 0;

pwm_limit_t     _pwm_limit;

// esc parameters
int32_t _pwm_disarmed;
int32_t _pwm_min;
int32_t _pwm_max;

void usage();

void start();

void stop();

int pwm_write_sysfs(char *path, int value);

int pwm_poll_initialize(const char *device);

void pwm_poll_deinitialize();

uint16_t pwm_read(int index);

void send_outputs_pwm(const uint16_t *pwm);

void task_main_trampoline(int argc, char *argv[]);

void subscribe();

void task_main(int argc, char *argv[]);

int pwm_write_sysfs(char *path, int value)
{
	int fd = ::open(path, O_WRONLY | O_CLOEXEC);
	int n;
	char data[16];

	if (fd == -1) {
		return -errno;
	}

	n = ::snprintf(data, sizeof(data), "%u", value);

	if (n > 0) {
		n = ::write(fd, data, n);	// This n is not used, but to avoid a compiler error.
	}

	::close(fd);

	return 0;
}

int pwm_poll_initialize(const char *device)
{
	int i;
	char path[128];

	for (i = 0; i < NUM_PWM; ++i) {
		::snprintf(path, sizeof(path), "%s/pwm%u/duty_cycle", device, i);
		_pwm_fd[i] = ::open(path, O_RDONLY | O_CLOEXEC);

		if (_pwm_fd[i] == -1) {
			PX4_ERR("PWM: Failed to open duty_cycle.");
			return -errno;
		}

		_poll_fds[_poll_fds_num].fd = _pwm_fd[i];
		_poll_fds[_poll_fds_num].events = POLLIN;
		_poll_fds_num++;
	}

	return 0;
}

void pwm_poll_deinitialize()
{
	for (int i = 0; i < NUM_PWM; ++i) {
		if (_pwm_fd[i] != -1) {
			::close(_pwm_fd[i]);
		}
	}
}


uint16_t pwm_read(int index)
{
	int n;
	char data[17];

	n = ::pread(_pwm_fd[index], data, 16, 0);

	if (n > 0) {
		data[n] = 0;
		unsigned long r = strtoul(data, NULL, 10);
		uint16_t duty_cycle = r / 1000;
		return duty_cycle;

	} else if (n == 0) {
		PX4_ERR("duty cycle %u no pwm to read", index);

	} else {
		PX4_ERR("error reading pwm duty cycle %u", index);
	}

	return 0;
}

void send_outputs_pwm(const uint16_t *pwm)
{
	int n;
	char data[16];

	//convert this to duty_cycle in ns
	for (unsigned i = 0; i < NUM_PWM; ++i) {
		n = ::snprintf(data, sizeof(data), "%u", pwm[i] * 1000);
		n = ::write(_pwm_fd[i], data, n);	// This n is not used, but to avoid a compiler error.
	}
}

void subscribe()
{
	_armed_sub = orb_subscribe(ORB_ID(actuator_armed));
}

void task_main(int argc, char *argv[])
{
	_is_running = true;

	if (pwm_poll_initialize(_device) < 0) {
		PX4_ERR("Failed to initialize PWM.");
		return;
	}

	// subscribe and set up polling
	subscribe();

	// Start disarmed
	_armed.armed = false;
	_armed.prearmed = false;

	pwm_limit_init(&_pwm_limit);


	unsigned int sleep_usec = 1000000 / 50; // 50 Hz (approx)

	// Main loop
	while (!_task_should_exit) {

		/* get duty cycles */
		uint16_t pwm[NUM_PWM];

		for (int i = 0; i < NUM_PWM; i++) {
			uint16_t duty_cycle = pwm_read(i);
			pwm[i] = duty_cycle;
		}

		/* compute original outputs from pwm */
		_outputs.noutputs = NUM_PWM;
		_outputs.timestamp = hrt_absolute_time();

		for (int i = 0; i < sizeof(_outputs.output) / sizeof(_outputs.output[0]); i++) {
			if (i >= _outputs.noutputs) {
				_outputs.output[i] = NAN;

			} else {
				_outputs.output[i] = pwm[i];
			}
		}

		if (_outputs_pub != nullptr) {
			orb_publish(ORB_ID(actuator_outputs), _outputs_pub, &_outputs);

		} else {
			_outputs_pub = orb_advertise(ORB_ID(actuator_outputs), &_outputs);
		}


		bool updated;
		orb_check(_armed_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(actuator_armed), _armed_sub, &_armed);
		}

		usleep(sleep_usec);
	}

	pwm_poll_deinitialize();

	orb_unsubscribe(_armed_sub);

	_is_running = false;

}

void task_main_trampoline(int argc, char *argv[])
{
	task_main(argc, argv);
}

void start()
{
	ASSERT(_task_handle == -1);

	_task_should_exit = false;

	/* start the task */
	_task_handle = px4_task_spawn_cmd("pwm_out_hil",
					  SCHED_DEFAULT,
					  SCHED_PRIORITY_MAX,
					  1500,
					  (px4_main_t)&task_main_trampoline,
					  nullptr);

	if (_task_handle < 0) {
		warn("task start failed");
		return;
	}

}

void stop()
{
	_task_should_exit = true;

	while (_is_running) {
		usleep(200000);
		PX4_INFO(".");
	}

	_task_handle = -1;
}

void usage()
{
	PX4_INFO("usage: pwm_out start [-d pwmdevice] -m mixerfile");
	PX4_INFO("       -d pwmdevice : sysfs device for pwm generation");
	PX4_INFO("                       (default /sys/class/pwm/pwmchip0)");
	PX4_INFO("       pwm_out stop");
	PX4_INFO("       pwm_out status");
}

} // namespace navio_sysfs_pwm_hil

/* driver 'main' command */
extern "C" __EXPORT int navio_sysfs_pwm_hil_main(int argc, char *argv[]);

int navio_sysfs_pwm_hil_main(int argc, char *argv[])
{
	int ch;
	int myoptind = 1;
	const char *myoptarg = nullptr;

	char *verb = nullptr;

	if (argc >= 2) {
		verb = argv[1];

	} else {
		return 1;
	}

	while ((ch = px4_getopt(argc, argv, "d:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'd':
			strncpy(navio_sysfs_pwm_hil::_device, myoptarg, sizeof(navio_sysfs_pwm_hil::_device));
			break;
		}
	}

	// gets the parameters for the esc's pwm
	param_get(param_find("PWM_DISARMED"), &navio_sysfs_pwm_hil::_pwm_disarmed);
	param_get(param_find("PWM_MIN"), &navio_sysfs_pwm_hil::_pwm_min);
	param_get(param_find("PWM_MAX"), &navio_sysfs_pwm_hil::_pwm_max);

	/*
	 * Start/load the driver.
	 */
	if (!strcmp(verb, "start")) {
		if (navio_sysfs_pwm_hil::_is_running) {
			PX4_WARN("pwm_out already running");
			return 1;
		}

		navio_sysfs_pwm_hil::start();
	}

	else if (!strcmp(verb, "stop")) {
		if (!navio_sysfs_pwm_hil::_is_running) {
			PX4_WARN("pwm_out is not running");
			return 1;
		}

		navio_sysfs_pwm_hil::stop();
	}

	else if (!strcmp(verb, "status")) {
		PX4_WARN("pwm_out is %s", navio_sysfs_pwm_hil::_is_running ? "running" : "not running");
		return 0;

	} else {
		navio_sysfs_pwm_hil::usage();
		return 1;
	}

	return 0;
}
