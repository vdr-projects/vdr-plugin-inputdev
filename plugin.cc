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

#include <getopt.h>
#include <unistd.h>

#include <vdr/plugin.h>

#include "inputdev.h"

static char const *DEFAULT_SOCKET_PATH = SOCKET_PATH;
static const char *VERSION        = PACKAGE_VERSION;
static const char *DESCRIPTION    = "Linux input device  plugin";

class cInputDevicePlugin : public cPlugin {
private:
	class cInputDeviceController	*controller_;

	enum {
		enSOCKET,
		enSYSTEMD
	}				socket_type_;

	union {
		char const		*path;
		int			idx;
	}				socket_;

	cString				coldplug_dir;

private:
	cInputDevicePlugin(cInputDevicePlugin const &);
	operator = (cInputDevicePlugin const &);

public:
	cInputDevicePlugin(void);
	virtual ~cInputDevicePlugin();

	virtual const char *Version(void) { return VERSION; }
	virtual const char *Description(void) { return DESCRIPTION; }

	virtual bool	ProcessArgs(int argc, char *argv[]);
	virtual bool	Initialize(void);
	virtual bool	Start(void);
	virtual void	Stop(void);
};

cInputDevicePlugin::cInputDevicePlugin() :
	controller_(NULL), coldplug_dir("/dev/vdr/input")
{
}

cInputDevicePlugin::~cInputDevicePlugin(void)
{
	delete controller_;
}

bool cInputDevicePlugin::ProcessArgs(int argc, char *argv[])
{
	static struct option const	CMDLINE_OPTIONS[] = {
		{ "systemd", required_argument, NULL, 'S' },
		{ "socket",  required_argument, NULL, 's' },
		{ }
	};

	char const	*systemd_idx = NULL;
	char const	*socket_path = NULL;
	bool		is_ok = true;

	for (;;) {
		int		c;

		c = getopt_long(argc, argv, "S:s:", CMDLINE_OPTIONS, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'S':
#ifdef VDR_USE_SYSTEMD
			systemd_idx = optarg;
			break;
#else
			esyslog("%s: systemd support has not been compiled in\n",
				Name());
			return false;
#endif
		case 's':  socket_path = optarg; break;
		default:
			esyslog("%s: invalid option\n", Name());
			return false;
		}
	}

	if (systemd_idx != NULL && socket_path != NULL) {
		esyslog("%s: both systemd idx and socket path given\n",
			Name());
		return false;
	}

	if (systemd_idx == NULL && socket_path == NULL)
		socket_path = DEFAULT_SOCKET_PATH;

	if (systemd_idx != NULL) {
		socket_type_ = enSYSTEMD;
		socket_.idx  = atoi(systemd_idx);
	} else if (socket_path != NULL) {
		socket_type_ = enSOCKET;
		socket_.path = socket_path;
	} else
		is_ok = false;

	return is_ok;
}

bool cInputDevicePlugin::Initialize(void)
{
	bool		is_ok;

	controller_ = new cInputDeviceController(*this);

	switch (socket_type_) {
#ifdef VDR_USE_SYSTEMD
	case enSYSTEMD:
		is_ok = controller_->open_udev_socket(socket_.idx);
		break;
#endif

	case enSOCKET:
		is_ok = controller_->open_udev_socket(socket_.path);
		break;

	default:
		esyslog("%s: bad internal socket type %d\n", Name(),
			socket_type_);
		is_ok = false;
		break;
	}

	if (is_ok)
		is_ok = controller_->initialize(coldplug_dir);

	if (!is_ok) {
		delete controller_;
		controller_ = NULL;
	}

	return is_ok;
}

bool cInputDevicePlugin::Start(void)
{
	return controller_->start();
}

void cInputDevicePlugin::Stop(void)
{
	controller_->stop();
	delete controller_;
	controller_ = NULL;
}

VDRPLUGINCREATOR(cInputDevicePlugin); // Don't touch this!
