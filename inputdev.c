/*	--*- c++ -*--
 * Copyright (C) 2012 Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 and/or 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inputdev.h"

#include <algorithm>
#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <linux/input.h>

#include <vdr/plugin.h>


#define ARRAY_SIZE(_a)	(sizeof(_a) / sizeof(_a)[0])
inline static bool test_bit(unsigned int bit, unsigned long const mask[])
{
	unsigned long	m = mask[bit / (sizeof mask[0] * 8)];
	unsigned int	i = bit % (sizeof mask[0] * 8);

	return (m & (1u << i)) != 0u;
}

class cInputDevice : public cListObject {
private:
	cInputDeviceController	&controller_;
	cString			dev_path_;
	cString			description_;
	int			fd_;
	dev_t			dev_t_;

public:
	cInputDevice(cInputDeviceController &controller,
		     cString const &dev_path);
	~cInputDevice();

	virtual int Compare(cInputDevice const &b) const {
		isyslog("Compare(%s, %s) -> %04lx - %04lx\n",
			get_dev_path(), b.get_dev_path(),
			dev_t_, b.dev_t_);
		return this->dev_t_ - b.dev_t_;
	}

	void		handle(void);
	bool		open(void);
	bool		start(int efd);
	void		stop(int efd);
	int		get_fd(void) const { return fd_; }
	char const	*get_description(void) const { return description_; }
	char const	*get_dev_path(void) const { return dev_path_; }

	static uint64_t	generate_code(uint16_t type, uint16_t code,
				      uint32_t value);
	static void	install_keymap(char const *remote);

};

cInputDevice::cInputDevice(cInputDeviceController &controller,
			   cString const &dev_path) :
	controller_(controller), dev_path_(dev_path), fd_(-1)
{
}

cInputDevice::~cInputDevice()
{
	controller_.close(fd_);
}

uint64_t cInputDevice::generate_code(uint16_t type, uint16_t code,
				     uint32_t value)
{
	uint64_t	res = type;

	res <<= 16;
	res  |= code;
	res <<= 32;
	res  |= value;

	return res;
}

void cInputDevice::install_keymap(char const *remote)
{
	static struct {
		enum eKeys		vdr_key;
		unsigned int		code;
	} const			MAPPING[] = {
		{ kUp,		KEY_UP },
		{ kDown,	KEY_DOWN },
		{ kMenu,	KEY_MENU },
		{ kOk,		KEY_OK },
		{ kBack,	KEY_BACKSPACE },	// \todo
		{ kLeft,	KEY_LEFT },
		{ kRight,	KEY_RIGHT },
		{ kRed,		KEY_RED },
		{ kGreen,	KEY_GREEN },
		{ kYellow,	KEY_YELLOW },
		{ kBlue,	KEY_BLUE },
		{ k0,		KEY_KP0 },
		{ k1,		KEY_KP1 },
		{ k2,		KEY_KP2 },
		{ k3,		KEY_KP3 },
		{ k4,		KEY_KP4 },
		{ k5,		KEY_KP5 },
		{ k6,		KEY_KP6 },
		{ k7,		KEY_KP7 },
		{ k8,		KEY_KP8 },
		{ k9,		KEY_KP9 },
		{ kInfo,	KEY_INFO },
		{ kPlayPause,	KEY_PLAYPAUSE },
		{ kPlay,	KEY_PLAY },
		{ kPause,	KEY_PAUSE },
		{ kStop,	KEY_STOP },
		{ kRecord,	KEY_RECORD },
		{ kFastFwd,	KEY_FORWARD }, // \todo
		{ kFastRew,	KEY_BACK },	// \todo
		{ kNext,	KEY_NEXTSONG },
		{ kPrev,	KEY_PREVIOUSSONG },
		{ kPower,	KEY_POWER },
		{ kChanUp,	KEY_CHANNELUP },
		{ kChanDn,	KEY_CHANNELDOWN },
		{ kChanPrev,	KEY_PREVIOUS }, // \todo
		{ kVolUp,	KEY_VOLUMEUP },
		{ kVolDn,	KEY_VOLUMEDOWN },
		{ kMute,	KEY_MUTE },
		{ kAudio,	KEY_AUDIO },
		{ kSubtitles,	KEY_SUBTITLE },
		{ kSchedule,	KEY_EPG }, // \todo
		{ kChannels,	KEY_CHANNEL },
		{ kTimers,	KEY_PROGRAM }, // \todo
		{ kRecordings,	KEY_ARCHIVE }, // \todo
		{ kSetup,	KEY_SETUP },
		{ kCommands,	KEY_VENDOR }, // \todo
		{ kUser0,	KEY_FN_F10 },
		{ kUser1,	KEY_FN_F1 },
		{ kUser2,	KEY_FN_F2 },
		{ kUser3,	KEY_FN_F3 },
		{ kUser4,	KEY_FN_F4 },
		{ kUser5,	KEY_FN_F5 },
		{ kUser6,	KEY_FN_F6 },
		{ kUser7,	KEY_FN_F7 },
		{ kUser8,	KEY_FN_F8 },
		{ kUser9,	KEY_FN_F9 },
	};

	size_t		i;

	for (i = 0; i < ARRAY_SIZE(MAPPING); ++i) {
		uint64_t	code = generate_code(0, EV_KEY, MAPPING[i].code);
		char		buf[17];

		snprintf(buf, sizeof buf, "%016"PRIX64, code);
		Keys.Add(new cKey(remote, buf, MAPPING[i].vdr_key));
	}
}

bool cInputDevice::open(void)
{
	char const	*path = dev_path_;
	char		description[256];
	int		fd;
	int		rc;
	struct stat	st;
	unsigned long	events_mask[(std::max(EV_CNT,KEY_MAX) +
				     sizeof(unsigned long) * 8 - 1)/
				    (sizeof(unsigned long) * 8)];
	
	fd = ::open(path, O_RDWR);
	if (fd < 0) {
		esyslog("%s: open(%s) failed: %s\n", controller_.plugin_name(),
			path, strerror(errno));
		goto err;
	}

	rc = fstat(fd, &st);
	if (rc < 0) {
		esyslog("%s: fstat(%s) failed: %s\n", controller_.plugin_name(),
			path, strerror(errno));
		goto err;
	}

	rc = ioctl(fd, EVIOCGNAME(sizeof description - 1), description);
	if (rc < 0) {
		esyslog("%s: ioctl(%s, EVIOCGNAME) failed: %s\n",
			controller_.plugin_name(), path, strerror(errno));
		goto err;
	}

	rc = ioctl(fd, EVIOCGBIT(0, sizeof events_mask), events_mask);
	if (rc < 0) {
		esyslog("%s: ioctl(%s, EVIOCGBIT) failed: %s\n",
			controller_.plugin_name(), path, strerror(errno));
		goto err;
	}

	if (!test_bit(EV_KEY, events_mask)) {
		isyslog("%s: skipping %s; no key events\n",
			controller_.plugin_name(), path);
		goto err;
	}
		

	description[sizeof description - 1] = '\0';

	this->dev_t_ = st.st_rdev;
	this->fd_ = fd;
	this->description_ = description;

	return true;

err:
	controller_.close(fd);
	return false;
}

bool cInputDevice::start(int efd)
{
	int			rc;
	struct epoll_event	ev = { };
	char const		*dev_path = dev_path_;

	ev.events   = EPOLLIN;
	ev.data.ptr = this;

	rc = ioctl(fd_, EVIOCGRAB, 1);
	if (rc < 0) {
		esyslog("%s: ioctl(GRAB, <%s>) failed: %s\n",
			controller_.plugin_name(), dev_path, strerror(errno));
		goto err;
	}

	rc = epoll_ctl(efd, EPOLL_CTL_ADD, fd_, &ev);
	if (rc < 0) {
		esyslog("%s: epoll_ctl(ADD, <%s>) failed: %s\n",
			controller_.plugin_name(), dev_path, strerror(errno));
		goto err;
	}

	return true;

err:
	return false;
}

void cInputDevice::stop(int efd)
{
	ioctl(fd_, EVIOCGRAB, 0);
	epoll_ctl(efd, EPOLL_CTL_DEL, fd_, NULL);
}

void cInputDevice::handle(void)
{
	struct input_event	ev;
	ssize_t			rc;

	uint64_t		code;
	bool			is_released = false;
	bool			is_repeated = false;
	bool			is_valid;

	rc = read(fd_, &ev, sizeof ev);
	if (rc < 0 && errno == EINTR)
		return;
	
	if (rc < 0) {
		esyslog("%s: failed to read from %s: %s\n",
			controller_.plugin_name(), get_dev_path(),
			strerror(errno));
		return;
	}

	if ((size_t)rc != sizeof ev) {
		esyslog("%s: read unexpected amount %zd of data\n",
			controller_.plugin_name(), rc);
		return;
	}

	// \todo: do something useful with EV_SYN...
	if (ev.type == EV_SYN || ev.type >= EV_MAX)
		// ignore events which are no valid key events
		return;

	switch (ev.type) {
	case EV_KEY:
		is_valid = true;

		switch (ev.value) {
		case 0:
			is_released = true;
			break;
		case 1:
			break;
		case 2:
			is_repeated = true;
			break;
		default:
			is_valid = false;
			break;
		}

		code = generate_code(0, ev.type, ev.code);
		break;

	default:
		is_valid = false;
		break;
	}

	if (!is_valid) {
		esyslog("%s: unexpected key events [%02x,%04x,%u]\n",
			controller_.plugin_name(), ev.type, ev.code, ev.value);
		return;
	}

	if (!controller_.Put(code, is_repeated, is_released)) {
		esyslog("%s: failed to put [%02x,%04x,%u] event\n",
			controller_.plugin_name(), ev.type, ev.code, ev.value);
		return;
	}
}

// ===========================

cInputDeviceController::cInputDeviceController(cPlugin &p)
	: cRemote("inputdev"), plugin_(p), fd_udev_(-1), fd_epoll_(-1)
{
	SetDescription("inpudev handler");
}

cInputDeviceController::~cInputDeviceController(void)
{
	this->close(fd_udev_);
	this->close(fd_epoll_);
}


char const *cInputDeviceController::plugin_name(void) const
{
	return plugin_.Name();
}

void cInputDeviceController::close(int fd)
{
	if (fd != -1)
		::close(fd);
}

bool cInputDeviceController::open_generic(int fd_udev)
{
	struct epoll_event	ev = { };
	int			rc;
	int			fd_epoll = -1;

	if (this->fd_epoll_ != -1) {
		esyslog("%s: internal error; epoll fd already open\n", 
			plugin_.Name());
		goto err;
	}

	if (this->fd_udev_ != -1) {
		esyslog("%s: internal error; udev fd already open\n", 
			plugin_.Name());
		goto err;
	}
	
	// requires linux >= 2.6.27
	fd_epoll = epoll_create1(EPOLL_CLOEXEC);
	if (fd_epoll < 0) {
		esyslog("%s: epoll_create1() failed: %s\n", plugin_.Name(),
			strerror(errno));
		goto err;
	}

	ev.events = EPOLLIN;
	ev.data.ptr = NULL;

	rc = epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_udev, &ev);
	if (rc < 0) {
		esyslog("%s: epoll_ctl(ADD, <udev>) failed: %s\n",
			plugin_.Name(), strerror(errno));
		goto err;
	}

	this->fd_udev_  = fd_udev;
	this->fd_epoll_ = fd_epoll;

	return true;

err:
	this->close(fd_epoll);
	return false;
}

bool cInputDeviceController::open_udev_socket(char const *sock_path)
{
	struct sockaddr_un	addr = { AF_UNIX };
	int			fd = -1;
	int			rc;
	mode_t			old_umask;

	strncpy(addr.sun_path, sock_path, sizeof addr.sun_path);
	
	/* requires linux >= 2.6.27 */
	rc = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (rc < 0) {
		esyslog("%s: socket() failed: %s\n", 
			plugin_.Name(), strerror(errno));
		goto err;
	}

	fd = rc;

	unlink(sock_path);		// ignore errors
	old_umask = umask(0077);
	rc = bind(fd, reinterpret_cast<sockaddr const *>(&addr), sizeof addr);
	umask(old_umask);
	if (rc < 0) {
		esyslog("%s: bind() failed: %s\n",
			plugin_.Name(), strerror(errno));
		goto err;
	}

	if (!open_generic(fd))
		goto err;

	return true;

err:
	this->close(fd);
	return false;
}

#include <systemd/sd-daemon.h>
bool cInputDeviceController::open_udev_socket(unsigned int systemd_idx)
{
	int	rc;
	int	fd = SD_LISTEN_FDS_START + systemd_idx;
	bool	is_valid = false;

	rc = sd_is_socket_unix(fd, SOCK_DGRAM, 1, NULL, 0);

	if (rc < 0)
		esyslog("%s: failed to check systemd socket: %s\n",
			plugin_.Name(), strerror(rc));
	else if (rc == 0)
		esyslog("%s: invalid systemd socket\n", plugin_.Name());
	else
		is_valid = open_generic(fd);

	return is_valid;
}

void cInputDeviceController::cleanup_devices(void)
{
	dev_mutex_.Lock();
	while (gc_devices_.Count() > 0) {
		class cInputDevice *dev = gc_devices_.First();
		gc_devices_.Del(dev, false);

		dev_mutex_.Unlock();
		delete dev;
		dev_mutex_.Lock();
	}
	dev_mutex_.Unlock();
}

void cInputDeviceController::Action(void)
{
	while (Running()) {
		struct epoll_event	events[10];
		int			rc;
		size_t			i;

		rc = epoll_wait(fd_epoll_, events, 
				sizeof events/sizeof events[0], -1);

		if (!Running())
			break;

		if (rc < 0 && errno == EINTR)
			continue;
		else if (rc < 0) {
			esyslog("%s: epoll_wait() failed: %s\n", plugin_.Name(),
				strerror(errno));
			break;
		}

		for (i = 0; i < (size_t)(rc); ++i) {
			unsigned int		ev = events[i].events;
			class cInputDevice	*dev =
				static_cast<class cInputDevice *>(events[i].data.ptr);

			if ((ev & (EPOLLHUP|EPOLLIN)) == EPOLLHUP) {
				if (dev == NULL) {
					esyslog("%s: uevent socket hung up; stopping plugin\n",
						plugin_name());
					this->Cancel(-1);
				} else {
					isyslog("%s: device '%s' (%s) hung up\n", 
						plugin_name(), 
						dev->get_dev_path(),
						dev->get_description());
					remove_device(dev);
				}
			} else if (dev == NULL) {
				handle_uevent();
			} else {
				dev->handle();
			}
		}

		cleanup_devices();
	}
}

void cInputDeviceController::remove_device(char const *dev_path)
{
	class cInputDevice	*dev = NULL;

	{
		cMutexLock	lock(&dev_mutex_);
		cInputDevice	*i;

		for (i = devices_.First(); i;
		     i = static_cast<class cInputDevice *>(i->Next())) {
			if (i->get_dev_path() == dev_path) {
				dev = i;
				break;
			}
		}

		if (dev != NULL) {
			dev->stop(fd_epoll_);

			dev->Unlink();
			gc_devices_.Add(dev);
		}
	}

	if (dev == NULL) {
		esyslog("%s: device '%s' not found\n",
			plugin_name(), dev_path);
		return;
	}
}

void cInputDeviceController::remove_device(class cInputDevice *dev)
{
	dev->stop(fd_epoll_);

	dev_mutex_.Lock();
	dev->Unlink();
	gc_devices_.Add(dev);
	dev_mutex_.Unlock();
}

void cInputDeviceController::add_device(char const *dev_name)
{
	class cInputDevice	*dev =
		new cInputDevice(*this, 
				 cString::sprintf("/dev/input/%s", dev_name));
	char const		*desc;

	if (!dev->open()) {
		delete dev;
		return;
	}

	desc = dev->get_description();

	{
		cMutexLock	lock(&dev_mutex_);
		cInputDevice	*i;

		for (i = devices_.First(); i;
		     i = static_cast<class cInputDevice *>(i->Next())) {
			if (dev->Compare(*i) == 0) {
				dsyslog("%s: device '%s' (%s) already registered\n",
					plugin_name(), dev_name, desc);
				delete dev;
				dev = NULL;
				break;
			}
		}

		if (dev != NULL) {
			isyslog("%s: added input device '%s' (%s)\n",
				plugin_name(), dev_name, desc);
			devices_.Add(dev);
		}
	}

	if (dev != NULL && !dev->start(fd_epoll_))
		remove_device(dev);
}

void cInputDeviceController::handle_uevent(void)
{
	char		buf[128];
	char		cmd[sizeof buf];
	char		dev[sizeof buf];

	ssize_t		rc;

	rc = read(fd_udev_, buf, sizeof buf - 1u);
	if (rc < 0 && errno == EINTR)
		return;

	if (rc < 0) {
		esyslog("%s: read(<udev>) failed: %s\n", plugin_.Name(),
			strerror(errno));
		return;
	}

	if (rc == sizeof buf - 1u) {
		esyslog("%s: read(<udev>) received too much data\n", 
			plugin_.Name());
		return;
	}

	buf[rc] = '\0';
	rc = sscanf(buf, "%s %s", cmd, dev);
	if (rc != 2) {
		esyslog("%s: invalid uevent '%s'\n", plugin_.Name(), buf);
		return;
	}

	if (strcasecmp(cmd, "add") == 0 ||
	    strcasecmp(cmd, "change") == 0) {
		add_device(dev);
	} else if (strcasecmp(cmd, "remove") == 0) {
		remove_device(dev);
	} else {
		esyslog("%s: invalid command '%s' for '%s'\n", plugin_name(), 
			cmd, dev);
		return;
	}
}

bool cInputDeviceController::start(void)
{
	cThread::Start();
	return true;
}

bool cInputDeviceController::initialize(void)
{
	cInputDevice::install_keymap(Name());
	return true;
}

void cInputDeviceController::stop(void)
{
	Cancel(-1);
	this->close(fd_epoll_);
	fd_epoll_ = -1;
}
