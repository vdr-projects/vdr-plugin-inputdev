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

#include "quirks.h"
#include <cstring>

using namespace std;

Quirks::Quirks() : broken_repeat(false)
{
}

bool &Quirks::find_quirk(char const *quirk) throw(UnknownQuirkError)
{
	if (strcasecmp(quirk, "broken_repeat") == 0)
		return broken_repeat;
	else
		throw UnknownQuirkError(quirk);
}

Quirks &Quirks::change(char const *quirk, bool set) throw(UnknownQuirkError)
{
	find_quirk(quirk) = set;
	return *this;
}
