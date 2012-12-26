/*	--*- c -*--
 * Copyright (C) 2012 Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <alloca.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char *argv[])
{
	struct sockaddr_un	addr = {
		.sun_family	=  AF_UNIX,
		.sun_path	=  SOCKET_PATH,
	};

	int			fd;
	char			*cmd;
	char const		*dev;
	char const		*action = getenv("ACTION");
	ssize_t			l;
	bool			is_add;

	if (argc < 2) {
		fprintf(stderr, "missing device name\n");
		return EX_USAGE;
	}

	if (argc > 2) {
		strncpy(addr.sun_path, argv[2], sizeof addr.sun_path - 1);
		/* string is terminated by initial assignment */
	}

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket()");
		return EX_OSERR;
	}

	dev = argv[1];
	if (strncmp(dev, "input/", 6) == 0)
		dev += 6;

	if (action == NULL)
		action = "add";

	cmd = alloca(strlen(action) + sizeof(" \n") + strlen(dev));
	strcpy(cmd, action);
	strcat(cmd, " ");
	strcat(cmd, dev);
	strcat(cmd, "\n");

	l = sendto(fd, cmd, strlen(cmd), 0, (void *)&addr, sizeof addr);
	close(fd);

	if (l < 0) {
		perror("sendto()");
		return EX_OSERR;
	}

	if (l != strlen(cmd)) {
		fprintf(stderr, "not all data sent\n");
		return EX_OSERR;
	}

	return EX_OK;
}
