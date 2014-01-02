/*	--*- c++ -*--
 * Copyright (C) 2014 Enrico Scholz <ensc@ensc.de>
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

#ifndef H_ENSC_VDR_INPUTDEV_QUIRKS_H
#define H_ENSC_VDR_INPUTDEV_QUIRKS_H

#include <stdexcept>

class Quirks {
public:	
	class UnknownQuirkError : public std::runtime_error {
	public:
		UnknownQuirkError(std::string const &quirk) :
			std::runtime_error("unknown quirk '" + quirk + "'") {
		}
	};

	bool	broken_repeat;

	Quirks();

	Quirks	&change(char const *quirk, bool set) throw(UnknownQuirkError);

	Quirks	&set(char const *quirk) throw(UnknownQuirkError) {
		return change(quirk, true);
	}

	Quirks	&clear(char const *quirk) throw(UnknownQuirkError) {
		return change(quirk, false);
	}

private:
	bool	&find_quirk(char const *quirk) throw(UnknownQuirkError);
};

#endif	/* H_ENSC_VDR_INPUTDEV_QUIRKS_H */
