/*
 *  ircd-ratbox: A slightly useful ircd.
 *  unix.c: various unix type functions
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 2005 ircd-ratbox development team
 *  Copyright (C) 2005 Aaron Sethman <androsyn@ratbox.org>
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

#include <librb_config.h>
#include <rb_lib.h>

#ifndef _WIN32

#include <sys/wait.h>

#ifdef HAVE_DLINFO
# include <link.h>
# include <dlfcn.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <crt_externs.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(HAVE_SPAWN_H) && defined(HAVE_POSIX_SPAWN)
#include <spawn.h>

#ifndef __APPLE__
extern char **environ;
#endif
pid_t
rb_spawn_process(const char *path, const char **argv)
{
	pid_t pid;
	const void *arghack = argv;
	char **myenviron;
	int error;
	posix_spawnattr_t spattr;
	posix_spawnattr_init(&spattr);
#ifdef POSIX_SPAWN_USEVFORK
	posix_spawnattr_setflags(&spattr, POSIX_SPAWN_USEVFORK);
#endif
#ifdef __APPLE__
 	myenviron = *_NSGetEnviron(); /* apple needs to go fuck themselves for this */
#else
	myenviron = environ;
#endif
	error = posix_spawn(&pid, path, NULL, &spattr, arghack, myenviron);
	posix_spawnattr_destroy(&spattr);
	if (error != 0)
	{
		errno = error;
		pid = -1;
	}
	return pid;
}
#else
pid_t
rb_spawn_process(const char *path, const char **argv)
{
	pid_t pid;
	if(!(pid = vfork()))
	{
		execv(path, (const void *)argv);	/* make gcc shut up */
		_exit(1);	/* if we're still here, we're screwed */
	}
	return (pid);
}
#endif

#ifndef HAVE_GETTIMEOFDAY
int
rb_gettimeofday(struct timeval *tv, void *tz)
{
	if(tv == NULL)
	{
		errno = EFAULT;
		return -1;
	}
	tv->tv_usec = 0;
	if(time(&tv->tv_sec) == -1)
		return -1;
	return 0;
}
#else
int
rb_gettimeofday(struct timeval *tv, void *tz)
{
	return (gettimeofday(tv, tz));
}
#endif

void
rb_sleep(unsigned int seconds, unsigned int useconds)
{
#ifdef HAVE_NANOSLEEP
	struct timespec tv;
	tv.tv_nsec = (useconds * 1000);
	tv.tv_sec = seconds;
	nanosleep(&tv, NULL);
#else
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = useconds;
	select(0, NULL, NULL, NULL, &tv);
#endif
}

/* this is to keep some linkers from bitching about exporting a non-existant symbol..bleh */
char *
rb_strerror(int error)
{
	return strerror(error);
}

int
rb_kill(pid_t pid, int sig)
{
	return kill(pid, sig);
}

int
rb_setenv(const char *name, const char *value, int overwrite)
{
	return setenv(name, value, overwrite);
}

pid_t
rb_waitpid(pid_t pid, int *status, int options)
{
	return waitpid(pid, status, options);
}

pid_t
rb_getpid(void)
{
	return getpid();
}

const char *
rb_path_to_self(void)
{
	static char path_buf[4096];
	size_t path_len = sizeof(path_buf);
#if defined(HAVE_GETEXECNAME)
	char *s = getexecname();
	if (s == NULL)
		return NULL;
	realpath(s, path_buf);
	return path_buf;
#elif defined(__linux__) || (defined(__FreeBSD__) && !defined(KERN_PROC_PATHNAME))
	if (readlink("/proc/self/exe", path_buf, path_len) != -1)
		return path_buf;
	return NULL;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	if (sysctl(mib, 4, path_buf, &path_len, NULL, 0) == 0)
		return path_buf;
	return NULL;
#elif defined(HAVE_DLINFO)
	struct link_map *map = NULL;
	dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &map);
	if (map == NULL)
		return NULL;
	realpath(map->l_name, path_buf);
	return path_buf;
#elif defined(__APPLE__)
	char tmp_path[4096];
	uint32_t pathlen = 4096;

	if (_NSGetExecutablePath(tmp_path, &pathlen) < 0)
		return NULL;

	realpath(tmp_path, path_buf);
	return path_buf;
#else
	return NULL;
#endif
}

#endif /* !WIN32 */
