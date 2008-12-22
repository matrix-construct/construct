/*
 *  ircd-ratbox: A slightly useful ircd.
 *  win32.c: select() compatible network routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2001 Adrian Chadd <adrian@creative.net.au>
 *  Copyright (C) 2005-2006 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2002-2006 ircd-ratbox development team
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
 *  $Id: win32.c 26092 2008-09-19 15:13:52Z androsyn $
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>

#ifdef _WIN32

static HWND hwnd;

#define WM_SOCKET WM_USER
/*
 * having gettimeofday is nice...
 */

typedef union
{
	unsigned __int64 ft_i64;
	FILETIME ft_val;
} FT_t;

#ifdef __GNUC__
#define Const64(x) x##LL
#else
#define Const64(x) x##i64
#endif
/* Number of 100 nanosecond units from 1/1/1601 to 1/1/1970 */
#define EPOCH_BIAS  Const64(116444736000000000)

pid_t
rb_getpid()
{
	return GetCurrentProcessId();
}


int
rb_gettimeofday(struct timeval *tp, void *not_used)
{
	FT_t ft;

	/* this returns time in 100-nanosecond units  (i.e. tens of usecs) */
	GetSystemTimeAsFileTime(&ft.ft_val);

	/* seconds since epoch */
	tp->tv_sec = (long)((ft.ft_i64 - EPOCH_BIAS) / Const64(10000000));

	/* microseconds remaining */
	tp->tv_usec = (long)((ft.ft_i64 / Const64(10)) % Const64(1000000));

	return 0;
}


pid_t
rb_spawn_process(const char *path, const char **argv)
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	char cmd[MAX_PATH];
	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	rb_strlcpy(cmd, path, sizeof(cmd));
	if(CreateProcess(cmd, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi) == FALSE)
		return -1;

	return (pi.dwProcessId);
}

pid_t
rb_waitpid(int pid, int *status, int flags)
{
	DWORD timeout = (flags & WNOHANG) ? 0 : INFINITE;
	HANDLE hProcess;
	DWORD waitcode;

	hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
	if(hProcess)
	{
		waitcode = WaitForSingleObject(hProcess, timeout);
		if(waitcode == WAIT_TIMEOUT)
		{
			CloseHandle(hProcess);
			return 0;
		}
		else if(waitcode == WAIT_OBJECT_0)
		{
			if(GetExitCodeProcess(hProcess, &waitcode))
			{
				*status = (int)((waitcode & 0xff) << 8);
				CloseHandle(hProcess);
				return pid;
			}
		}
		CloseHandle(hProcess);
	}
	else
		errno = ECHILD;

	return -1;
}

int
rb_setenv(const char *name, const char *value, int overwrite)
{
	char *buf;
	int len;
	if(!overwrite)
	{
		if((buf = getenv(name)) != NULL)
		{
			if(strlen(buf) > 0)
			{
				return 0;
			}
		}
	}
	if(name == NULL || value == NULL)
		return -1;
	len = strlen(name) + strlen(value) + 5;
	buf = rb_malloc(len);
	rb_snprintf(buf, len, "%s=%s", name, value);
	len = putenv(buf);
	rb_free(buf);
	return (len);
}

int
rb_kill(int pid, int sig)
{
	HANDLE hProcess;
	int ret = -1;
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
	
	if(hProcess)
	{
		switch (sig)
		{
		case 0:
			ret = 0;
			break;

		default:
			if(TerminateProcess(hProcess, sig))
				ret = 0;
			break;
		}
		CloseHandle(hProcess);
	}
	else
		errno = EINVAL;

	return ret;


}

/*
 * packet format is
 uint32_t magic
 uint8_t protocol count
 WSAPROTOCOL_INFO * count
 size_t datasize
 data
 
 */

static int
make_wsaprotocol_info(pid_t process, rb_fde_t *F, WSAPROTOCOL_INFO * inf)
{
	WSAPROTOCOL_INFO info;
	if(!WSADuplicateSocket((SOCKET) rb_get_fd(F), process, &info))
	{
		memcpy(inf, &info, sizeof(WSAPROTOCOL_INFO));
		return 1;
	}
	return 0;
}

static rb_fde_t *
make_fde_from_wsaprotocol_info(void *data)
{
	WSAPROTOCOL_INFO *info = data;
	SOCKET t;
	t = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, info, 0, 0);
	if(t == INVALID_SOCKET)
	{
		rb_get_errno();
		return NULL;
	}
	return rb_open(t, RB_FD_SOCKET, "remote_socket");
}

static uint8_t fd_buf[16384];
#define MAGIC_CONTROL 0xFF0ACAFE

int
rb_send_fd_buf(rb_fde_t *xF, rb_fde_t **F, int count, void *data, size_t datasize, pid_t pid)
{
	size_t bufsize =
		sizeof(uint32_t) + sizeof(uint8_t) + (sizeof(WSAPROTOCOL_INFO) * (size_t)count) +
		sizeof(size_t) + datasize;
	int i;
	uint32_t magic = MAGIC_CONTROL;
	void *ptr;
	if(count > 4)
	{
		errno = EINVAL;
		return -1;
	}
	if(bufsize > sizeof(fd_buf))
	{
		errno = EINVAL;
		return -1;
	}
	memset(fd_buf, 0, sizeof(fd_buf));

	ptr = fd_buf;
	memcpy(ptr, &magic, sizeof(magic));
	ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(magic));
	*((uint8_t *)ptr) = count;
	ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(uint8_t));

	for(i = 0; i < count; i++)
	{
		make_wsaprotocol_info(pid, F[i], (WSAPROTOCOL_INFO *) ptr);
		ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(WSAPROTOCOL_INFO));
	}
	memcpy(ptr, &datasize, sizeof(size_t));
	ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(size_t));
	memcpy(ptr, data, datasize);
	return rb_write(xF, fd_buf, bufsize);
}

#ifdef MYMIN
#undef MYMIN
#endif
#define MYMIN(a, b)  ((a) < (b) ? (a) : (b))

int
rb_recv_fd_buf(rb_fde_t *F, void *data, size_t datasize, rb_fde_t **xF, int nfds)
{
	size_t minsize = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(size_t);
	size_t datalen;
	ssize_t retlen;
	uint32_t magic;
	uint8_t count;
	unsigned int i;
	void *ptr;
	ssize_t ret;
	memset(fd_buf, 0, sizeof(fd_buf));	/* some paranoia here... */
	ret = rb_read(F, fd_buf, sizeof(fd_buf));
	if(ret <= 0)
	{
		return ret;
	}
	if(ret < (ssize_t) minsize)
	{
		errno = EINVAL;
		return -1;
	}
	ptr = fd_buf;
	memcpy(&magic, ptr, sizeof(uint32_t));
	if(magic != MAGIC_CONTROL)
	{
		errno = EAGAIN;
		return -1;
	}
	ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(uint32_t));
	memcpy(&count, ptr, sizeof(uint8_t));
	ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(uint8_t));
	for(i = 0; i < count && i < (unsigned int)nfds; i++)
	{
		rb_fde_t *tF = make_fde_from_wsaprotocol_info(ptr);
		if(tF == NULL)
			return -1;
		xF[i] = tF;
		ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(WSAPROTOCOL_INFO));
	}
	memcpy(&datalen, ptr, sizeof(size_t));
	ptr = (void *)((uintptr_t)ptr + (uintptr_t)sizeof(size_t));
	retlen = MYMIN(datalen, datasize);
	memcpy(data, ptr, datalen);
	return retlen;
}

static LRESULT CALLBACK
rb_process_events(HWND nhwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	rb_fde_t *F;
	PF *hdl;
	void *data;
	switch (umsg)
	{
	case WM_SOCKET:
		{
			F = rb_find_fd(wparam);

			if(F != NULL && IsFDOpen(F))
			{
				switch (WSAGETSELECTEVENT(lparam))
				{
				case FD_ACCEPT:
				case FD_CLOSE:
				case FD_READ:
					{
						if((hdl = F->read_handler) != NULL)
						{
							F->read_handler = NULL;
							data = F->read_data;
							F->read_data = NULL;
							hdl(F, data);
						}
						break;
					}

				case FD_CONNECT:
				case FD_WRITE:
					{
						if((hdl = F->write_handler) != NULL)
						{
							F->write_handler = NULL;
							data = F->write_data;
							F->write_data = NULL;
							hdl(F, data);
						}
					}
				}

			}
			return 0;
		}
	case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}

	default:
		return DefWindowProc(nhwnd, umsg, wparam, lparam);
	}
	return 0;
}

int
rb_init_netio_win32(void)
{
	/* this muchly sucks, but i'm too lazy to do overlapped i/o, maybe someday... -androsyn */
	WNDCLASS wc;
	static const char *classname = "ircd-ratbox-class";

	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC) rb_process_events;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = classname;
	wc.hInstance = GetModuleHandle(NULL);

	if(!RegisterClass(&wc))
		rb_lib_die("cannot register window class");

	hwnd = CreateWindow(classname, classname, WS_POPUP, CW_USEDEFAULT,
			    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			    (HWND) NULL, (HMENU) NULL, wc.hInstance, NULL);
	if(!hwnd)
		rb_lib_die("could not create window");
	return 0;
}

void
rb_sleep(unsigned int seconds, unsigned int useconds)
{
	DWORD msec = seconds * 1000;;
	Sleep(msec);
}

int
rb_setup_fd_win32(rb_fde_t *F)
{
	if(F == NULL)
		return 0;

	SetHandleInformation((HANDLE) F->fd, HANDLE_FLAG_INHERIT, 0);
	switch (F->type)
	{
	case RB_FD_SOCKET:
		{
			u_long nonb = 1;
			if(ioctlsocket((SOCKET) F->fd, FIONBIO, &nonb) == -1)
			{
				rb_get_errno();
				return 0;
			}
			return 1;
		}
	default:
		return 1;

	}
}

void
rb_setselect_win32(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	int old_flags = F->pflags;

	lrb_assert(IsFDOpen(F));

	/* Update the list, even though we're not using it .. */
	if(type & RB_SELECT_READ)
	{
		if(handler != NULL)
			F->pflags |= FD_CLOSE | FD_READ | FD_ACCEPT;
		else
			F->pflags &= ~(FD_CLOSE | FD_READ | FD_ACCEPT);
		F->read_handler = handler;
		F->read_data = client_data;
	}

	if(type & RB_SELECT_WRITE)
	{
		if(handler != NULL)
			F->pflags |= FD_WRITE | FD_CONNECT;
		else
			F->pflags &= ~(FD_WRITE | FD_CONNECT);
		F->write_handler = handler;
		F->write_data = client_data;
	}

	if(old_flags == 0 && F->pflags == 0)
		return;

	if(F->pflags != old_flags)
	{
		WSAAsyncSelect(F->fd, hwnd, WM_SOCKET, F->pflags);
	}

}


static int has_set_timer = 0;

int
rb_select_win32(long delay)
{
	MSG msg;
	if(has_set_timer == 0)
	{
		/* XXX should probably have this handle all the events
		 * instead of busy looping 
		 */
		SetTimer(hwnd, 0, delay, NULL);
		has_set_timer = 1;
	}

	if(GetMessage(&msg, NULL, 0, 0) == FALSE)
	{
		rb_lib_die("GetMessage failed..byebye");
	}
	rb_set_time();

	DispatchMessage(&msg);
	return RB_OK;
}

#ifdef strerror
#undef strerror
#endif

static const char *
_rb_strerror(int error)
{
	switch (error)
	{
	case 0:
		return "Success";
	case WSAEINTR:
		return "Interrupted system call";
	case WSAEBADF:
		return "Bad file number";
	case WSAEACCES:
		return "Permission denied";
	case WSAEFAULT:
		return "Bad address";
	case WSAEINVAL:
		return "Invalid argument";
	case WSAEMFILE:
		return "Too many open sockets";
	case WSAEWOULDBLOCK:
		return "Operation would block";
	case WSAEINPROGRESS:
		return "Operation now in progress";
	case WSAEALREADY:
		return "Operation already in progress";
	case WSAENOTSOCK:
		return "Socket operation on non-socket";
	case WSAEDESTADDRREQ:
		return "Destination address required";
	case WSAEMSGSIZE:
		return "Message too long";
	case WSAEPROTOTYPE:
		return "Protocol wrong type for socket";
	case WSAENOPROTOOPT:
		return "Bad protocol option";
	case WSAEPROTONOSUPPORT:
		return "Protocol not supported";
	case WSAESOCKTNOSUPPORT:
		return "Socket type not supported";
	case WSAEOPNOTSUPP:
		return "Operation not supported on socket";
	case WSAEPFNOSUPPORT:
		return "Protocol family not supported";
	case WSAEAFNOSUPPORT:
		return "Address family not supported";
	case WSAEADDRINUSE:
		return "Address already in use";
	case WSAEADDRNOTAVAIL:
		return "Can't assign requested address";
	case WSAENETDOWN:
		return "Network is down";
	case WSAENETUNREACH:
		return "Network is unreachable";
	case WSAENETRESET:
		return "Net connection reset";
	case WSAECONNABORTED:
		return "Software caused connection abort";
	case WSAECONNRESET:
		return "Connection reset by peer";
	case WSAENOBUFS:
		return "No buffer space available";
	case WSAEISCONN:
		return "Socket is already connected";
	case WSAENOTCONN:
		return "Socket is not connected";
	case WSAESHUTDOWN:
		return "Can't send after socket shutdown";
	case WSAETOOMANYREFS:
		return "Too many references, can't splice";
	case WSAETIMEDOUT:
		return "Connection timed out";
	case WSAECONNREFUSED:
		return "Connection refused";
	case WSAELOOP:
		return "Too many levels of symbolic links";
	case WSAENAMETOOLONG:
		return "File name too long";
	case WSAEHOSTDOWN:
		return "Host is down";
	case WSAEHOSTUNREACH:
		return "No route to host";
	case WSAENOTEMPTY:
		return "Directory not empty";
	case WSAEPROCLIM:
		return "Too many processes";
	case WSAEUSERS:
		return "Too many users";
	case WSAEDQUOT:
		return "Disc quota exceeded";
	case WSAESTALE:
		return "Stale NFS file handle";
	case WSAEREMOTE:
		return "Too many levels of remote in path";
	case WSASYSNOTREADY:
		return "Network system is unavailable";
	case WSAVERNOTSUPPORTED:
		return "Winsock version out of range";
	case WSANOTINITIALISED:
		return "WSAStartup not yet called";
	case WSAEDISCON:
		return "Graceful shutdown in progress";
	case WSAHOST_NOT_FOUND:
		return "Host not found";
	case WSANO_DATA:
		return "No host data of that type was found";
	default:
		return strerror(error);
	}
};

char *
rb_strerror(int error)
{
	static char buf[128];
	rb_strlcpy(buf, _rb_strerror(error), sizeof(buf));
	return buf;
}
#else /* win32 not supported */
int
rb_init_netio_win32(void)
{
	errno = ENOSYS;
	return -1;
}

void
rb_setselect_win32(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	errno = ENOSYS;
	return;
}

int
rb_select_win32(long delay)
{
	errno = ENOSYS;
	return -1;
}

int
rb_setup_fd_win32(rb_fde_t *F)
{
	errno = ENOSYS;
	return -1;
}
#endif /* _WIN32 */
