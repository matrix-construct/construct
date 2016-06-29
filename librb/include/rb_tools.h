/*
 *  ircd-ratbox: A slightly useful ircd.
 *  tools.h: Header for the various tool functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#ifndef RB_LIB_H
# error "Do not use tools.h directly"
#endif

#ifndef __TOOLS_H__
#define __TOOLS_H__

int rb_strcasecmp(const char *s1, const char *s2);
int rb_strncasecmp(const char *s1, const char *s2, size_t n);
char *rb_strcasestr(const char *s, const char *find);
size_t rb_strlcpy(char *dst, const char *src, size_t siz);
size_t rb_strlcat(char *dst, const char *src, size_t siz);
size_t rb_strnlen(const char *s, size_t count);

#ifdef __GNUC__
int rb_snprintf_append(char *str, size_t len, const char *format, ...)
	__attribute__ ((format(printf, 3, 4)));
#else
int rb_snprintf_append(char *str, const size_t size, const char *, ...);
#endif

char *rb_basename(const char *);
char *rb_dirname(const char *);

int rb_string_to_array(char *string, char **parv, int maxpara);
size_t rb_array_to_string(int parc, const char **parv, char *buf, size_t max);

typedef struct _rb_zstring
{
	uint16_t len; /* big enough */
	uint16_t alloclen;
	uint8_t *data;
} rb_zstring_t;

size_t rb_zstring_serialized(rb_zstring_t *zs, void **buf, size_t *buflen);
size_t rb_zstring_deserialize(rb_zstring_t *zs, void *buf);
void rb_zstring_free(rb_zstring_t *zs);
rb_zstring_t *rb_zstring_alloc(void);
rb_zstring_t *rb_zstring_from_c_len(const char *buf, size_t len);
rb_zstring_t *rb_zstring_from_c(const char *buf);
size_t rb_zstring_len(rb_zstring_t *zs);
void rb_zstring_append_from_zstring(rb_zstring_t *dst_zs, rb_zstring_t *src_zs);
void rb_zstring_append_from_c(rb_zstring_t *zs, const char *buf, size_t len);
char *rb_zstring_to_c(rb_zstring_t *zs, char *buf, size_t len);
char *rb_zstring_to_c_alloc(rb_zstring_t *zs);
size_t rb_zstring_to_ptr(rb_zstring_t *zs, void **ptr);
const char *rb_path_to_self(void);

#endif /* __TOOLS_H__ */
