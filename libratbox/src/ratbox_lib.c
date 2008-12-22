/*
 *  ircd-ratbox: A slightly useful ircd.
 *  ratbox_lib.c: libircd initialization functions at the like
 *
 *  Copyright (C) 2005,2006 ircd-ratbox development team
 *  Copyright (C) 2005,2006 Aaron Sethman <androsyn@ratbox.org>
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
 *  $Id: ratbox_lib.c 26282 2008-12-10 20:33:21Z androsyn $
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <commio-ssl.h>

static log_cb *rb_log;
static restart_cb *rb_restart;
static die_cb *rb_die;

static struct timeval rb_time;
static char errbuf[512];

/* this doesn't do locales...oh well i guess */

static const char *months[] = {
	"January", "February", "March", "April",
	"May", "June", "July", "August",
	"September", "October", "November", "December"
};

static const char *weekdays[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"
};

static const char *s_month[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	"Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *s_weekdays[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

char *
rb_ctime(const time_t t, char *buf, size_t len)
{
	char *p;
	struct tm *tp;
	static char timex[128];
	size_t tlen;
#if defined(HAVE_GMTIME_R)
	struct tm tmr;
	tp = gmtime_r(&t, &tmr);
#else
	tp = gmtime(&t);
#endif
	if(rb_unlikely(tp == NULL))
	{
		strcpy(buf, "");
		return (buf);
	}

	if(buf == NULL)
	{
		p = timex;
		tlen = sizeof(timex);
	}
	else
	{
		p = buf;
		tlen = len;
	}

	rb_snprintf(p, tlen, "%s %s %d %02u:%02u:%02u %d",
		    s_weekdays[tp->tm_wday], s_month[tp->tm_mon],
		    tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec, tp->tm_year + 1900);
	return (p);
}


/* I hate this..but its sort of ircd standard now.. */
char *
rb_date(const time_t t, char *buf, size_t len)
{
	struct tm *gm;
#if defined(HAVE_GMTIME_R)
	struct tm gmbuf;
	gm = gmtime_r(&t, &gmbuf);
#else
	gm = gmtime(&t);
#endif

	if(rb_unlikely(gm == NULL))
	{
		rb_strlcpy(buf, "", len);
		return (buf);
	}

	rb_snprintf(buf, len, "%s %s %d %d -- %02u:%02u:%02u +00:00",
		    weekdays[gm->tm_wday], months[gm->tm_mon], gm->tm_mday,
		    gm->tm_year + 1900, gm->tm_hour, gm->tm_min, gm->tm_sec);
	return (buf);
}

time_t
rb_current_time(void)
{
	return rb_time.tv_sec;
}

const struct timeval *
rb_current_time_tv(void)
{
	return &rb_time;
}

void
rb_lib_log(const char *format, ...)
{
	va_list args;
	if(rb_log == NULL)
		return;
	va_start(args, format);
	rb_vsnprintf(errbuf, sizeof(errbuf), format, args);
	va_end(args);
	rb_log(errbuf);
}

void
rb_lib_die(const char *format, ...)
{
	va_list args;
	if(rb_die == NULL)
		abort();
	va_start(args, format);
	rb_vsnprintf(errbuf, sizeof(errbuf), format, args);
	va_end(args);
	rb_die(errbuf);
}

void
rb_lib_restart(const char *format, ...)
{
	va_list args;
	if(rb_restart == NULL)
		abort();
	va_start(args, format);
	rb_vsnprintf(errbuf, sizeof(errbuf), format, args);
	va_end(args);
	rb_restart(errbuf);
}

void
rb_set_time(void)
{
	struct timeval newtime;

	if(rb_unlikely(rb_gettimeofday(&newtime, NULL) == -1))
	{
		rb_lib_log("Clock Failure (%s)", strerror(errno));
		rb_lib_restart("Clock Failure");
	}

	if(newtime.tv_sec < rb_time.tv_sec)
		rb_set_back_events(rb_time.tv_sec - newtime.tv_sec);

	memcpy(&rb_time, &newtime, sizeof(struct timeval));
}

extern const char *libratbox_serno;

const char *
rb_lib_version(void)
{
	static char version_info[512];
	char ssl_info[512];
	rb_get_ssl_info(ssl_info, sizeof(ssl_info));
	rb_snprintf(version_info, sizeof(version_info), "libratbox version: %s - %s", libratbox_serno, ssl_info);
	return version_info;
}

void
rb_lib_init(log_cb * ilog, restart_cb * irestart, die_cb * idie, int closeall, int maxcon,
	    size_t dh_size, size_t fd_heap_size)
{
	rb_set_time();
	rb_log = ilog;
	rb_restart = irestart;
	rb_die = idie;
	rb_event_init();
	rb_init_bh();
	rb_fdlist_init(closeall, maxcon, fd_heap_size);
	rb_init_netio();
	rb_init_rb_dlink_nodes(dh_size);
	if(rb_io_supports_event())
	{
		rb_io_init_event();
	}
}

void
rb_lib_loop(long delay)
{
	time_t next;
	rb_set_time();

	if(rb_io_supports_event())
	{
		if(delay == 0)
			delay = -1;
		while(1)
			rb_select(-1);
	}


	while(1)
	{
		if(delay == 0)
		{
			if((next = rb_event_next()) > 0)
			{
				next -= rb_current_time();
				if(next <= 0)
					next = 1000;
				else
					next *= 1000;
			}
			else
				next = -1;
			rb_select(next);
		}
		else
			rb_select(delay);
		rb_event_run();
	}
}

#ifndef HAVE_STRTOK_R
char *
rb_strtok_r(char *s, const char *delim, char **save)
{
	char *token;

	if(s == NULL)
		s = *save;

	/* Scan leading delimiters.  */
	s += strspn(s, delim);

	if(*s == '\0')
	{
		*save = s;
		return NULL;
	}

	token = s;
	s = strpbrk(token, delim);

	if(s == NULL)
		*save = (token + strlen(token));
	else
	{
		*s = '\0';
		*save = s + 1;
	}
	return token;
}
#else
char *
rb_strtok_r(char *s, const char *delim, char **save)
{
	return strtok_r(s, delim, save);
}
#endif


static const char base64_table[] =
	{ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'
};

static const char base64_pad = '=';

static const short base64_reverse_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

unsigned char *
rb_base64_encode(const unsigned char *str, int length)
{
	const unsigned char *current = str;
	unsigned char *p;
	unsigned char *result;

	if((length + 2) < 0 || ((length + 2) / 3) >= (1 << (sizeof(int) * 8 - 2)))
	{
		return NULL;
	}

	result = rb_malloc(((length + 2) / 3) * 5);
	p = result;

	while(length > 2)
	{
		*p++ = base64_table[current[0] >> 2];
		*p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
		*p++ = base64_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
		*p++ = base64_table[current[2] & 0x3f];

		current += 3;
		length -= 3;
	}

	if(length != 0)
	{
		*p++ = base64_table[current[0] >> 2];
		if(length > 1)
		{
			*p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
			*p++ = base64_table[(current[1] & 0x0f) << 2];
			*p++ = base64_pad;
		}
		else
		{
			*p++ = base64_table[(current[0] & 0x03) << 4];
			*p++ = base64_pad;
			*p++ = base64_pad;
		}
	}
	*p = '\0';
	return result;
}

unsigned char *
rb_base64_decode(const unsigned char *str, int length, int *ret)
{
	const unsigned char *current = str;
	int ch, i = 0, j = 0, k;
	unsigned char *result;

	result = rb_malloc(length + 1);

	while((ch = *current++) != '\0' && length-- > 0)
	{
		if(ch == base64_pad)
			break;

		ch = base64_reverse_table[ch];
		if(ch < 0)
			continue;

		switch (i % 4)
		{
		case 0:
			result[j] = ch << 2;
			break;
		case 1:
			result[j++] |= ch >> 4;
			result[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			result[j++] |= ch >> 2;
			result[j] = (ch & 0x03) << 6;
			break;
		case 3:
			result[j++] |= ch;
			break;
		}
		i++;
	}

	k = j;

	if(ch == base64_pad)
	{
		switch (i % 4)
		{
		case 1:
			free(result);
			return NULL;
		case 2:
			k++;
		case 3:
			result[k++] = 0;
		}
	}
	result[j] = '\0';
	*ret = j;
	return result;
}
