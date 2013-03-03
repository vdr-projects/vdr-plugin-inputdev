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

#include "modmap.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/input.h>
#include <vdr/tools.h>

#include "util.h"
#include "gen-keymap.h"

#define ARRAY_SIZE(_a)	(sizeof(_a) / sizeof(_a)[0])

ModifierMap::ModifierMap()
{
	for (size_t i = 0; i < ARRAY_SIZE(keytables_); ++i) {
		keytables_[i] = new wchar_t[KEY_MAX];
		memset(keytables_[i], 0, sizeof keytables_[i][0] * KEY_MAX);
	}

	set_default_tables();
}

static struct {
	unsigned int	key;
	char		code[2];
} const		DEFAULT_MAP[] = {
	{ KEY_A, { 'a', 'A' } },
	{ KEY_B, { 'b', 'B' } },
	{ KEY_C, { 'c', 'C' } },
	{ KEY_D, { 'd', 'D' } },
	{ KEY_E, { 'e', 'E' } },
	{ KEY_F, { 'f', 'F' } },
	{ KEY_G, { 'g', 'G' } },
	{ KEY_H, { 'h', 'H' } },
	{ KEY_I, { 'i', 'I' } },
	{ KEY_J, { 'j', 'J' } },
	{ KEY_K, { 'k', 'K' } },
	{ KEY_L, { 'l', 'L' } },
	{ KEY_M, { 'm', 'M' } },
	{ KEY_N, { 'n', 'N' } },
	{ KEY_O, { 'o', 'O' } },
	{ KEY_P, { 'p', 'P' } },
	{ KEY_Q, { 'q', 'Q' } },
	{ KEY_R, { 'r', 'R' } },
	{ KEY_S, { 's', 'S' } },
	{ KEY_T, { 't', 'T' } },
	{ KEY_U, { 'u', 'U' } },
	{ KEY_V, { 'v', 'V' } },
	{ KEY_W, { 'w', 'W' } },
	{ KEY_X, { 'x', 'X' } },
	{ KEY_Y, { 'y', 'Y' } },
	{ KEY_Z, { 'z', 'Z' } },
	{ KEY_0, { '0', ')' } },
	{ KEY_1, { '1', '!' } },
	{ KEY_2, { '2', '@' } },
	{ KEY_3, { '3', '#' } },
	{ KEY_4, { '4', '$' } },
	{ KEY_5, { '5', '%' } },
	{ KEY_6, { '6', '^' } },
	{ KEY_7, { '7', '&' } },
	{ KEY_8, { '8', '*' } },
	{ KEY_9, { '9', '(' } },
};

void ModifierMap::set_default_tables()
{
#define assign(_kt, _idx) \
	keytables_[_kt][DEFAULT_MAP[i].key] = DEFAULT_MAP[i].code[_idx]

	for (size_t i = 0; i < ARRAY_SIZE(DEFAULT_MAP); ++i) {
		assign(ktNORMAL, 0);
		assign(ktSHIFT,  1);
		if (i < 27)
			keytables_[ktCONTROL][DEFAULT_MAP[i].key] = i + 1;
	}
#undef assign
}

ModifierMap::~ModifierMap()
{
	for (size_t i = ARRAY_SIZE(keytables_); i > 0; --i)
		delete [] keytables_[i-1];
}

static wchar_t utf8_to_wchar(char const *str)
{
	wchar_t			r = 0;
	size_t			cnt;
	unsigned char		c = str[0];

	if ((c & 0x80) == 0) {
		cnt = 1;
	} else if ((c & 0xe0) == 0xc0) {
		cnt = 2;
		c  &= 0x1f;
	} else if ((c & 0xf0) == 0xe0) { 
		cnt = 3;
		c  &= 0x0f;
	} else if ((c & 0xf8) == 0xf0) {
		cnt = 4;
		c  &= 0x07;
	} else {
		return L'\0';
	}

	if (str[cnt] != '\0')
		return L'\0';

	r = c;

	for (size_t i = 1; i < cnt; ++i) {
		c   = str[i];
		if ((c & 0xc0) != 0x80)
			return L'\0';

		r <<= 6;
		r  |= c & 0x3f;
	}

	return r;
}

bool ModifierMap::read_modmap(char const *fname)
{
	FILE		*f = fopen(fname, "r");
	cReadLine	r;

	if (!f) {
		esyslog("failed to open keymap file '%s': %s",
			fname, strerror(errno));
		return false;
	}

	for (size_t line_num = 1;; ++line_num) {
		static char const	DELIMS[] = " \t";
		char			*buf = r.Read(f);
		char			*buf_next;
		char const		*keycode;
		struct keymap_def const	*keydef;

		if (!buf)
			break;

		keycode = strtok_r(buf, DELIMS, &buf_next);
		if (!keycode || strchr(keycode, '#') != NULL) 
			continue;

		keydef = Perfect_Hash::in_word_set(keycode, strlen(keycode));
		if (!keydef) {
			esyslog("%s:%zu invalid keycode '%s'", fname, line_num,
				keycode);
			continue;
		}

		for (size_t v_idx = 0; v_idx < ARRAY_SIZE(keytables_); ++v_idx) {
			char const	*s;

			s = strtok_r(NULL, DELIMS, &buf_next);
			if (!s)
				break;

			if (strncmp(s, "\\x", 2) == 0) {
				keytables_[v_idx][keydef->num] = strtoul(s+2, NULL, 16);
				// \todo: error checks
			} else if (strncmp(s, "\\", 2) == 0) {
				keytables_[v_idx][keydef->num] = strtoul(s+1, NULL, 8);
				// \todo: error checks
			} else if (strcmp(s, "DEF") == 0) {
				// noop
			} else {
				wchar_t	v = utf8_to_wchar(s);

				if (!v)
					esyslog("%s:%zu invalid utf-8 char '%s'",
						fname, line_num, s);
				else
					keytables_[v_idx][keydef->num] = v;
			}
		}

		if (strtok_r(NULL, DELIMS, &buf_next))
			esyslog("%s:%zu superflous data", fname, line_num);
	}

	return true;
}

bool ModifierMap::translate(wchar_t &chr, unsigned int code,
			    unsigned long mask) const
{
	ssize_t		idx = -1;

	if (code >= KEY_MAX)
		return false;

	if (test_bit(modCAPSLOCK, &mask))
		change_bit(modSHIFT, &mask);

	clear_bit(modCAPSLOCK, &mask);
	clear_bit(modNUMLOCK,  &mask);	// \todo: implement me!

	if (test_bit(modCONTROL, &mask))
		idx = ktCONTROL;
	else if (test_bit(modSHIFT, &mask) && test_bit(modMODE, &mask))
		idx = ktMODE_SHIFT;
	else if (test_bit(modSHIFT, &mask))
		idx = ktSHIFT;
	else if (test_bit(modMODE, &mask))
		idx = ktMODE;
	else
		idx = ktNORMAL;

	if (idx != -1)
		chr = keytables_[idx][code];

	return idx != -1 && chr != L'\0';
}
