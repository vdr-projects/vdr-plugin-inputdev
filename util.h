/*	--*- c -*--
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

#ifndef HH_ENSC_VDR_INPUTDEV_UTIL_HH
#define HH_ENSC_VDR_INPUTDEV_UTIL_HH

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

inline static void clear_bit(unsigned int bit, unsigned long mask[])
{
	unsigned int	i = bit % (sizeof mask[0] * 8);

	mask[bit / (sizeof mask[0] * 8)] &= ~(1u << i);
}

inline static void change_bit(unsigned int bit, unsigned long mask[])
{
	if (test_bit(bit, mask))
		clear_bit(bit, mask);
	else
		set_bit(bit, mask);
}

#endif	/* HH_ENSC_VDR_INPUTDEV_UTIL_HH */
