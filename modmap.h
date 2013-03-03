/*	--*- c++ -*--
 * Copyright (C) 2013 Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
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

#ifndef HH_ENSC_VDR_INPUTDEV_MODMAP_HH
#define HH_ENSC_VDR_INPUTDEV_MODMAP_HH

class ModifierMap {
public:
	enum modifier {
		modSHIFT,
		modCONTROL,
		modALT,
		modMETA,
		modNUMLOCK,
		modCAPSLOCK,
		modMODE,

		_modMAX,
	};

	ModifierMap();
	~ModifierMap();

	bool	read_modmap(char const *fname);
	bool	translate(wchar_t &chr, unsigned int code,
			  unsigned long mask) const;

private:
	enum {
		ktNORMAL,
		ktSHIFT,
		ktCONTROL,
		ktMODE,
		ktMODE_SHIFT,
		ktUNUSED0,
		ktUNUSED1,
		ktUNUSED2,

		_ktMAX
	};

	typedef wchar_t		*keytable_t;

	// same semantics like 'keycode' in xmodmap(1)
	keytable_t		keytables_[_ktMAX];

	void	set_default_tables(void);
};

#endif	/* HH_ENSC_VDR_INPUTDEV_MODMAP_HH */
