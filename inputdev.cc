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
#include <cassert>
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

inline static void set_bit(unsigned int bit, unsigned long mask[])
{
	unsigned int	i = bit % (sizeof mask[0] * 8);

	mask[bit / (sizeof mask[0] * 8)] |= (1u << i);
}

class MagicState {
private:
	static bool const	IS_SUPPORTED_;

	unsigned int		state_;
	struct timespec		next_;

	static int compare(struct timespec const &a,
			   struct timespec const &b);

	static void add(struct timespec &res,
			struct timespec const &a,
			struct timespec const &b);

public:
	static struct timespec const	TIMEOUT;

	MagicState() : state_(0) {}
	bool	process(struct input_event const &ev);

};

static bool check_clock_gettime(void)
{
	struct timespec		tmp;

	if (clock_gettime(CLOCK_MONOTONIC, &tmp) < 0) {
		fprintf(stderr,
			"clock_gettime() not available; magic keysequences will not be available: %s\n",
			strerror(errno));
		return false;
	}

	return true;
}

bool const		MagicState::IS_SUPPORTED_ = check_clock_gettime();
struct timespec const	MagicState::TIMEOUT = { 2, 500000000 };

int MagicState::compare(struct timespec const &a, struct timespec const &b)
{
	if (a.tv_sec < b.tv_sec)
		return -1;
	else if (a.tv_sec > b.tv_sec)
		return + 1;
	else if (a.tv_nsec < b.tv_nsec)
		return -1;
	else if (a.tv_nsec > b.tv_nsec)
		return +1;
	else
		return 0;
}

void MagicState::add(struct timespec &res,
		     struct timespec const &a, struct timespec const &b)
{
	assert(a.tv_nsec < 1000000000);
	assert(b.tv_nsec < 1000000000);

	res.tv_sec  = a.tv_sec + b.tv_sec;
	res.tv_nsec = a.tv_nsec + b.tv_nsec;

	if (res.tv_nsec >= 1000000000) {
		res.tv_nsec -= 1000000000;
		res.tv_sec  += 1;
	}
}

bool MagicState::process(struct input_event const &ev)
{
	static unsigned int const	SEQUENCE[] = {
		KEY_LEFTSHIFT,
		KEY_LEFTSHIFT,
		KEY_ESC,
		KEY_LEFTSHIFT,
	};

	unsigned int		code;
	struct timespec		now;

	assert(state_ < ARRAY_SIZE(SEQUENCE));

	if (!IS_SUPPORTED_)
		return false;

	if (ev.type != EV_KEY || ev.value != 1)
		// ignore non-key events and skip release- and repeat events
		return false;

	// translate some keys
	switch (ev.code) {
	case KEY_RIGHTSHIFT:	code = KEY_LEFTSHIFT; break;
	default:		code = ev.code;
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	if (state_ > 0 && compare(next_, now) < 0)
		// reset state due to timeout
		state_ = 0;

	if (SEQUENCE[state_] != code) {
		state_ = 0;
	} else {
		++state_;
		add(next_, now, TIMEOUT);
	}

	if (state_ == ARRAY_SIZE(SEQUENCE)) {
		state_ = 0;
		return true;
	}

	return false;
}

class cInputDevice : public cListObject, public cEpollHandler {
public:
	enum modifier {
		modSHIFT,
		modCONTROL,
		modALT,
		modMETA,
		modNUMLOCK,
	};

private:
	cInputDeviceController	&controller_;
	cString			dev_path_;
	cString			description_;
	int			fd_;
	dev_t			dev_t_;
	class MagicState	magic_state_;

	unsigned long		modifiers_;

	cInputDevice(cInputDevice const &);
	cInputDevice & operator	= (cInputDevice const &);

public:
	// the vdr list implementation requires knowledge about the containing
	// list when unlinking a object :(
	cList<cInputDevice>	*container;

	cInputDevice(cInputDeviceController &controller,
		     cString const &dev_path);
	virtual ~cInputDevice();

	virtual int Compare(cInputDevice const &b) const {
		return this->dev_t_ - b.dev_t_;
	}

	virtual int Compare(dev_t b) const {
		return this->dev_t_ - b;
	}

	virtual void	handle_hup();
	virtual void	handle_pollin();

	bool		open(void);
	bool		start(int efd);
	void		stop(int efd);
	int		get_fd(void) const { return fd_; }
	char const	*get_description(void) const { return description_; }
	char const	*get_dev_path(void) const { return dev_path_; }

	void		dump(void) const;

	static uint64_t	generate_code(uint16_t type, uint16_t code,
				      uint32_t value);
	static void	install_keymap(char const *remote);

};

cInputDevice::cInputDevice(cInputDeviceController &controller,
			   cString const &dev_path) :
	controller_(controller), dev_path_(dev_path), fd_(-1), dev_t_(0),
	container(NULL)
{
}

cInputDevice::~cInputDevice()
{
	controller_.close(fd_);
}

void cInputDevice::dump(void) const
{
	dsyslog("%s:   %lx %s (%s), fd=%d\n", controller_.plugin_name(),
		static_cast<unsigned long>(dev_t_),
		get_dev_path(), get_description(), get_fd());
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
		{ kBack,	KEY_EXIT },	// \todo
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
		{ kFastFwd,	KEY_FORWARD },
		{ kFastRew,	KEY_REWIND },
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
		{ kCommands,	KEY_OPTION }, // \todo
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
	static unsigned int const	ONE = 1;
	int			rc;
	struct epoll_event	ev = { };
	char const		*dev_path = dev_path_;

	ev.events   = EPOLLIN;
	ev.data.ptr = static_cast<cEpollHandler *>(this);

	rc = ioctl(fd_, EVIOCGRAB, &ONE);
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

void cInputDevice::handle_hup(void)
{
	isyslog("%s: device '%s' (%s) hung up\n", controller_.plugin_name(),
		get_dev_path(), get_description());
	controller_.remove_device(this);
}

void cInputDevice::handle_pollin(void)
{
	struct input_event	ev;
	ssize_t			rc;

	uint64_t		code;
	bool			is_released = false;
	bool			is_repeated = false;
	bool			is_valid;
	bool			is_internal = false;

	rc = read(fd_, &ev, sizeof ev);
	if (rc < 0 && errno == EINTR)
		return;

	if (rc < 0 && errno == ENODEV) {
		isyslog("%s: device '%s' removed\n",
			controller_.plugin_name(), get_dev_path());
		controller_.remove_device(this);
		return;
	}

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

	// \todo: do something useful with the other events...
	if (ev.type != EV_KEY)
		// ignore events which are no valid key events
		return;

	if (0)
		dsyslog("%s: event{%s}=[%lu.%06u, %02x, %04x, %d]\n",
			controller_.plugin_name(), get_dev_path(),
			(unsigned long)(ev.time.tv_sec),
			(unsigned int)(ev.time.tv_usec),
			ev.type, ev.code, ev.value);

	if (magic_state_.process(ev)) {
		isyslog("%s: magic keysequence from %s; detaching device\n",
			controller_.plugin_name(), get_dev_path());
		controller_.remove_device(this);
		return;
	}

	switch (ev.type) {
	case EV_KEY: {
		unsigned long	mask = 0;
		
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

		switch (ev.code) {
		case KEY_LEFTSHIFT:
		case KEY_RIGHTSHIFT:
			set_bit(modSHIFT, &mask);
			break;

		case KEY_LEFTCTRL:
		case KEY_RIGHTCTRL:
			set_bit(modCONTROL, &mask);
			break;

		case KEY_LEFTALT:
		case KEY_RIGHTALT:
			set_bit(modALT, &mask);
			break;

		case KEY_LEFTMETA:
		case KEY_RIGHTMETA:
			set_bit(modMETA, &mask);
			break;

		case KEY_NUMLOCK:
			set_bit(modNUMLOCK, &mask);
			break;

		default:
			break;
		}

		if (mask == 0) {
			code = generate_code(0, ev.type, ev.code);
		} else if (is_released) {
			this->modifiers_ &= ~mask;
			is_internal = true;
		} else if (is_valid) {
 			this->modifiers_ |=  mask;
			is_internal = true;
		} else {
			// repeated events
			is_internal = true;
		}

		break;
	}

	default:
		is_valid = false;
		break;
	}

	if (is_internal)
		return;

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
	fd_alive_[0] = -1;
	fd_alive_[1] = -1;

	SetDescription("inputdev handler");
}

cInputDeviceController::~cInputDeviceController(void)
{
	this->close(fd_alive_[0]);
	this->close(fd_alive_[1]);
	this->close(fd_udev_);
	this->close(fd_epoll_);
}


char const *cInputDeviceController::plugin_name(void) const
{
	return plugin_.Name();
}

void cInputDeviceController::close(int &fd)
{
	if (fd != -1) {
		::close(fd);
		fd = -1;
	}
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
	ev.data.ptr = static_cast<cEpollHandler *>(this);

	rc = epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_udev, &ev);
	if (rc < 0) {
		esyslog("%s: epoll_ctl(ADD, <udev>) failed: %s\n",
			plugin_.Name(), strerror(errno));
		goto err;
	}

	rc = pipe2(fd_alive_, O_CLOEXEC);
	if (rc < 0) {
		esyslog("%s: pipe2(): %s\n", plugin_.Name(), strerror(errno));
		goto err;
	}

	ev.data.ptr = NULL;
	rc = epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_alive_[0], &ev);
	if (rc < 0) {
		esyslog("%s: epoll_ctl(ADD, <alive#%d>) failed: %s\n",
			plugin_.Name(), fd_alive_[0], strerror(errno));
		goto err;
	}

	this->fd_udev_  = fd_udev;
	this->fd_epoll_ = fd_epoll;

	return true;

err:
	this->close(fd_alive_[0]);
	this->close(fd_alive_[1]);
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

#ifdef VDR_USE_SYSTEMD

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

#endif

void cInputDeviceController::cleanup_devices(void)
{
	dev_mutex_.Lock();
	while (gc_devices_.Count() > 0) {
		class cInputDevice *dev = gc_devices_.First();

		assert(dev->container == &gc_devices_);
		gc_devices_.Del(dev, false);

		dev_mutex_.Unlock();
		delete dev;
		dev_mutex_.Lock();
	}
	dev_mutex_.Unlock();
}

void cInputDeviceController::handle_hup(void)
{
	esyslog("%s: uevent socket hung up; stopping plugin\n",
		plugin_name());
	this->Cancel(-1);
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
			class cEpollHandler	*dev =
				static_cast<class cEpollHandler *>(events[i].data.ptr);

			if (!dev)
				esyslog("%s: internal error; got event from keep-alive pipe\n",
					plugin_.Name());
			else if ((ev & (EPOLLHUP|EPOLLIN)) == EPOLLHUP)
				dev->handle_hup();
			else if (ev & EPOLLIN)
				dev->handle_pollin();
			else
				esyslog("%s: unexpected event %04x@%p\n",
					plugin_.Name(), ev, dev);
		}

		cleanup_devices();
	}
}

void cInputDeviceController::remove_device(char const *dev_path)
{
	class cInputDevice	*dev = NULL;
	struct stat		st;

	if (stat(dev_path, &st) < 0) {
		esyslog("%s: fstat(%s) failed: %s\n", plugin_name(),
			dev_path, strerror(errno));
	} else {
		cMutexLock	lock(&dev_mutex_);

		for (cInputDevice *i = devices_.First();
		     i != NULL && dev == NULL;
		     i = devices_.Next(i)) {
			if (i->Compare(st.st_rdev) == 0)
				dev = i;
		}

		if (dev != NULL) {
			dev->stop(fd_epoll_);

			assert(dev->container == &devices_);
			devices_.Del(dev, false);

			gc_devices_.Add(dev);
			dev->container = &gc_devices_;
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
	if (dev->container)
		dev->container->Del(dev, false);

	gc_devices_.Add(dev);
	dev->container = &gc_devices_;

	dev_mutex_.Unlock();
}

bool cInputDeviceController::add_device(char const *dev_name)
{
	class cInputDevice	*dev =
		new cInputDevice(*this,
				 cString::sprintf("/dev/input/%s", dev_name));
	char const		*desc;
	bool			res;

	if (!dev->open()) {
		delete dev;
		return false;
	}

	desc = dev->get_description();

	{
		cMutexLock	lock(&dev_mutex_);

		for (cInputDevice *i = devices_.First(); i;
		     i = devices_.Next(i)) {
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
			dev->container = &devices_;
		}
	}

	if (dev != NULL && !dev->start(fd_epoll_)) {
		res = false;
		remove_device(dev);
	} else {
		res = dev != NULL;
	}

	return res;
}

void cInputDeviceController::dump_active_devices(void)
{
	cMutexLock	lock(&dev_mutex_);

	dsyslog("%s: active devices:\n", plugin_name());
	for (cInputDevice *i = devices_.First(); i; i = devices_.Next(i))
		i->dump();
}

void cInputDeviceController::dump_gc_devices(void)
{
	cMutexLock	lock(&dev_mutex_);

	dsyslog("%s: gc devices:\n", plugin_name());
	for (cInputDevice *i = gc_devices_.First(); i; i = gc_devices_.Next(i))
		i->dump();
}

void cInputDeviceController::handle_pollin(void)
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
	} else if (strcasecmp(cmd, "dump") == 0) {
		bool	is_all = strcasecmp(dev, "all") == 0;
		if (is_all || strcasecmp(dev, "active") == 0)
			dump_active_devices();

		if (is_all || strcasecmp(dev, "gc") == 0)
			dump_gc_devices();
	} else {
		esyslog("%s: invalid command '%s' for '%s'\n", plugin_name(),
			cmd, dev);
		return;
	}
}

bool cInputDeviceController::coldplug_devices(char const *path)
{
	cReadDir		cdir(path);
	bool			res = true;

	for (;;) {
		struct dirent const	*ent = cdir.Next();

		if (!ent)
			break;

		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;

		if (!add_device(ent->d_name)) {
			esyslog("%s: failed to coldplug '%s'\n",
				plugin_name(), ent->d_name);
			res = false;
		} else {
			isyslog("%s: coldplugged '%s'\n",
				plugin_name(), ent->d_name);
		}
	}

	return res;
}

bool cInputDeviceController::initialize(char const *coldplug_dir)
{
	cInputDevice::install_keymap(Name());

	coldplug_devices(coldplug_dir);

	return true;
}

bool cInputDeviceController::start(void)
{
	cThread::Start();
	return true;
}

void cInputDeviceController::stop(void)
{
	Cancel(-1);

	this->close(fd_epoll_);
	this->close(fd_alive_[1]);

	Cancel(5);
}
