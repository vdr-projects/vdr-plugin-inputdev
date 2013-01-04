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

#ifndef H_ENSC_VDR_INPUTDEV_INPUTDEV_H
#define H_ENSC_VDR_INPUTDEV_INPUTDEV_H

#include <vdr/remote.h>
#include <vdr/thread.h>

class cEpollHandler {
public:
	virtual void	handle_hup() = 0;
	virtual void	handle_pollin() = 0;
};

class cPlugin;
class cInputDevice;
class cInputDeviceController : protected cRemote, protected cThread,
			       protected cEpollHandler
{
private:
	cPlugin			&plugin_;
	int			fd_udev_;
	int			fd_epoll_;
	int			fd_alive_[2];
	cList<cInputDevice>	devices_;
	cList<cInputDevice>	gc_devices_;

	cMutex			dev_mutex_;

	cInputDeviceController(cInputDeviceController const &);

	bool		open_generic(int fd_udev);
	void		cleanup_devices(void);

	bool		coldplug_devices(char const *);

	void		dump_active_devices();
	void		dump_gc_devices();

protected:
	virtual void	Action(void);

	virtual void	handle_hup();
	virtual void	handle_pollin();

public:
	explicit cInputDeviceController(cPlugin &p);
	virtual ~cInputDeviceController();

	bool		initialize(char const *coldplug_dir);
	bool		start(void);
	void		stop(void);

	char const	*plugin_name(void) const;

	bool		open_udev_socket(char const *sock_path);
	bool		open_udev_socket(unsigned int systemd_idx);

	bool		add_device(char const *dev);
	void		remove_device(char const *dev);
	void		remove_device(class cInputDevice *dev);

	static void	close(int &fd);

	bool	Put(uint64_t Code, bool Repeat, bool Release) {
		return cRemote::Put(Code, Repeat, Release);
	}
};

#endif	/* H_ENSC_VDR_INPUTDEV_INPUTDEV_H */
