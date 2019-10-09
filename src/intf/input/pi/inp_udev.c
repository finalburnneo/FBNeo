/**
** Copyright (C) 2015 Akop Karapetyan
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**/

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "inp_udev.h"

#define MAX_DEVICES 10

// #define DUMP_DEVICES

struct USBDevice {
	char *path;
	char *vendorId;
	char *productId;
	char *mfr;
	char *product;
};

static pthread_t monthread;
static int stopMonitor = 0;
static int initted = 0;
static struct udev *udev = NULL;
static struct udev_monitor *mon;

static void* monitor(void *arg);

static struct USBDevice* create_usb_device(struct udev_device *dev);
static void destroy_usb_device(struct USBDevice *usbDev);

static const char* device_id(int index,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex);
static const char* device_product(int index,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex);
static void add_device(struct udev_device *dev,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex);
static void remove_device(struct udev_device *dev,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex);
static void clear_devices(struct USBDevice *devices[], int *deviceCount,
	pthread_mutex_t *mutex);
static void clear_all_devices();

static void scan_devices();
static void dump_all_devices();
static void dump_devices(struct USBDevice *devices[], const char *desc,
	int *deviceCount, pthread_mutex_t *mutex);

static int joystickCount = 0;
static int mouseCount = 0;

static struct USBDevice *joysticks[MAX_DEVICES];
static struct USBDevice *mice[MAX_DEVICES];

static pthread_mutex_t joystickLock;
static pthread_mutex_t mouseLock;

// Based on
// http://www.signal11.us/oss/udev/udev_example.c

int phl_udev_init()
{
	if (!initted) {
		if (!(udev = udev_new())) {
			return 0;
		}

		if (pthread_mutex_init(&joystickLock, NULL) != 0) {
			udev_unref(udev);
			return 0;
		}
		if (pthread_mutex_init(&mouseLock, NULL) != 0) {
			pthread_mutex_destroy(&joystickLock);
			udev_unref(udev);
			return 0;
		}

		mon = udev_monitor_new_from_netlink(udev, "udev");
		udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
		udev_monitor_enable_receiving(mon);

		scan_devices();

		if (pthread_create(&monthread, NULL, monitor, NULL) != 0) {
			pthread_mutex_destroy(&mouseLock);
			pthread_mutex_destroy(&joystickLock);
			udev_unref(udev);
			return 0;
		}

		fprintf(stderr, "udev initialized\n");
		initted = 1;
	}
		
	return 1;
}

int phl_udev_shutdown()
{
	if (initted) {
		stopMonitor = 1;
		pthread_join(monthread, NULL);
	
		clear_all_devices();
		fprintf(stderr, "udev shut down\n");
	
		udev_unref(udev);

		pthread_mutex_destroy(&joystickLock);
		pthread_mutex_destroy(&mouseLock);
		
		initted = 0;
	}

	return 1;
}

int phl_udev_joy_count()
{
	return joystickCount;
}

int phl_udev_mouse_count()
{
	return mouseCount;
}

const char* phl_udev_joy_id(int index)
{
	return device_id(index, joysticks, &joystickCount, &joystickLock);
}

const char* phl_udev_joy_name(int index)
{
	return device_product(index, joysticks, &joystickCount, &joystickLock);
}

static void scan_devices()
{
	fprintf(stderr, "Scanning devices...\n");

	clear_all_devices();

	struct udev_enumerate *uenum;
	struct udev_list_entry *devices, *devEntry;
	
	uenum = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(uenum, "input");
	udev_enumerate_scan_devices(uenum);
	devices = udev_enumerate_get_list_entry(uenum);
	udev_list_entry_foreach(devEntry, devices) {
		const char *path = udev_list_entry_get_name(devEntry);
		struct udev_device *dev = udev_device_new_from_syspath(udev, path);
		
		const char *sysname = udev_device_get_sysname(dev);
		if (strncmp(sysname, "mouse", 5) == 0) {
			add_device(dev, mice, &mouseCount, &mouseLock);
		} else if (strncmp(sysname, "js", 2) == 0) {
			add_device(dev, joysticks, &joystickCount, &joystickLock);
		}
		
		udev_device_unref(dev);
	};
	
	udev_enumerate_unref(uenum);
}

static void* monitor(void *arg)
{
	fprintf(stderr, "udev monitor starting\n");
	
	int monfd = udev_monitor_get_fd(mon);
	struct udev_device *dev;
	fd_set fds;
	struct timeval tv;
	int ret;
	
	while (!stopMonitor) {
		FD_ZERO(&fds);
		FD_SET(monfd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		
		ret = select(monfd + 1, &fds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(monfd, &fds)) {
			// Make the call to receive the device.
			//   select() ensured that this will not block.
			dev = udev_monitor_receive_device(mon);
			if (dev) {
				const char *sysname = udev_device_get_sysname(dev);
				const char *action = udev_device_get_action(dev);

				if (strncmp(sysname, "mouse", 5) == 0) {
					if (strncmp(action, "add", 3) == 0) {
						fprintf(stderr, "+ mouse\n");
						add_device(dev, mice, &mouseCount, &mouseLock);
					} else if (strncmp(action, "remove", 6) == 0) {
						fprintf(stderr, "- mouse\n");
						remove_device(dev, mice, &mouseCount, &mouseLock);
					}
#ifdef DUMP_DEVICES
					dump_all_devices();
#endif
				} else if (strncmp(sysname, "js", 2) == 0) {
					if (strncmp(action, "add", 3) == 0) {
						fprintf(stderr, "+ joystick\n");
						add_device(dev, joysticks, &joystickCount, &joystickLock);
					} else if (strncmp(action, "remove", 6) == 0) {
						fprintf(stderr, "- joystick\n");
						remove_device(dev, joysticks, &joystickCount, &joystickLock);
					}
#ifdef DUMP_DEVICES
					dump_all_devices();
#endif
				}

				udev_device_unref(dev);
			}
		}
		
		usleep(250*1000);
	}
	
	fprintf(stderr, "udev monitor exiting\n");
	return NULL;
}

#ifdef TEST_MAIN
int main(int argc, char* argv[])
{
	udevInit();
	fprintf(stderr, "Starting...\n");
	char *my_string;
	int nbytes = 100;
	getline(&my_string, &nbytes, stdin);
	dump_all_devices();
	fprintf(stderr, "Ending\n");
	udevShutdown();

	return 0;
}
#endif

static struct USBDevice* create_usb_device(struct udev_device *dev)
{
	const char *path = udev_device_get_syspath(dev);
	dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");

	struct USBDevice *usbDev = NULL;
	if (dev != NULL) {
		usbDev = (struct USBDevice *)calloc(1, sizeof(struct USBDevice));	
		if (usbDev != NULL) {
			usbDev->path = strdup(path);
			const char *p;
			if ((p = udev_device_get_sysattr_value(dev, "idVendor")) != NULL) {
				usbDev->vendorId = strdup(p);
			}
			if ((p = udev_device_get_sysattr_value(dev, "idProduct")) != NULL) {
				usbDev->productId = strdup(p);
			}
			if ((p = udev_device_get_sysattr_value(dev, "manufacturer")) != NULL) {
				usbDev->mfr = strdup(p);
			}
			if ((p = udev_device_get_sysattr_value(dev, "product")) != NULL) {
				usbDev->product = strdup(p);
			}
		}
	}
	
	return usbDev;
}

static void destroy_usb_device(struct USBDevice *usbDev)
{
	free(usbDev->vendorId);
	free(usbDev->productId);
	free(usbDev->mfr);
	free(usbDev->product);
	free(usbDev);
}

static const char* device_id(int index,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex)
{
	char *result = NULL;
	static char temp[100];
	
	pthread_mutex_lock(mutex);
	if (index < *deviceCount) {
		snprintf(temp, 99, "%s:%s",
			devices[index]->vendorId, devices[index]->productId);
		result = temp;
	}
	pthread_mutex_unlock(mutex);
	
	return result;
}

static const char* device_product(int index,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex)
{
	char *result = NULL;
	static char temp[100];
	
	pthread_mutex_lock(mutex);
	if (index < *deviceCount) {
		strncpy(temp, devices[index]->product, 99);
		result = temp;
	}
	pthread_mutex_unlock(mutex);
	
	return result;
}

static void add_device(struct udev_device *dev,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex)
{
	pthread_mutex_lock(mutex);
	if (*deviceCount + 1 < MAX_DEVICES) {
		struct USBDevice *usbDev = create_usb_device(dev);
		if (usbDev != NULL) {
			devices[(*deviceCount)++] = usbDev;
		}
	}
	pthread_mutex_unlock(mutex);
}

static void remove_device(struct udev_device *dev,
	struct USBDevice *devices[], int *deviceCount, pthread_mutex_t *mutex)
{
	const char *path = udev_device_get_syspath(dev);
	int i, j;

	pthread_mutex_lock(mutex);
	for (i = 0; i < *deviceCount; i++) {
		if (strcmp(path, devices[i]->path) == 0) {
			// Destroy the removed item
			destroy_usb_device(devices[i]);
			// Move the items back one slot
			for (j = i + 1; j < *deviceCount; j++) {
				devices[j - 1] = devices[j];
			}
			(*deviceCount)--;
			break;
		}
	}
	pthread_mutex_unlock(mutex);
}

static void clear_devices(struct USBDevice *devices[], int *deviceCount,
	pthread_mutex_t *mutex)
{
	int i;
    pthread_mutex_lock(mutex);
	for (i = 0; i < *deviceCount; i++) {
		destroy_usb_device(devices[i]);
	}
	*deviceCount = 0;
    pthread_mutex_unlock(mutex);
}

static void clear_all_devices()
{
	clear_devices(joysticks, &joystickCount, &joystickLock);
}

static void dump_devices(struct USBDevice *devices[], const char *desc,
	int *deviceCount, pthread_mutex_t *mutex)
{
	int i;
    pthread_mutex_lock(mutex);
	for (i = 0; i < *deviceCount; i++) {
		fprintf(stderr, "Joy %d: %s:%s %s (%s)\n", i,
			devices[i]->vendorId, devices[i]->productId,
			devices[i]->product,
			devices[i]->path);
	}
    pthread_mutex_unlock(mutex);

    fprintf(stderr, "%d %s\n", *deviceCount, desc);
}

static void dump_all_devices()
{
	dump_devices(joysticks, "joystick(s)", &joystickCount, &joystickLock);
	dump_devices(mice, "m(ous|ic)e", &mouseCount, &mouseLock);
}

