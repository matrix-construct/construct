/*
 *  sslproc.c: An interface to wsockd
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2007 ircd-ratbox development team
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
 */

#include <rb_lib.h>
#include "stdinc.h"


#include "s_conf.h"
#include "logger.h"
#include "listener.h"
#include "wsproc.h"
#include "s_serv.h"
#include "ircd.h"
#include "hash.h"
#include "client.h"
#include "send.h"
#include "packet.h"

static void ws_read_ctl(rb_fde_t * F, void *data);
static int wsockd_count;

static char tmpbuf[READBUF_SIZE];
static char nul = '\0';

#define MAXPASSFD 4
#define READSIZE 1024
typedef struct _ws_ctl_buf
{
	rb_dlink_node node;
	char *buf;
	size_t buflen;
	rb_fde_t *F[MAXPASSFD];
	int nfds;
} ws_ctl_buf_t;


struct ws_ctl
{
	rb_dlink_node node;
	int cli_count;
	rb_fde_t *F;
	rb_fde_t *P;
	pid_t pid;
	rb_dlink_list readq;
	rb_dlink_list writeq;
	uint8_t shutdown;
	uint8_t dead;
};

static rb_dlink_list wsock_daemons;

static inline uint32_t
buf_to_uint32(char *buf)
{
	uint32_t x;
	memcpy(&x, buf, sizeof(x));
	return x;
}

static inline void
uint32_to_buf(char *buf, uint32_t x)
{
	memcpy(buf, &x, sizeof(x));
	return;
}

static ws_ctl_t *
allocate_ws_daemon(rb_fde_t * F, rb_fde_t * P, int pid)
{
	ws_ctl_t *ctl;

	if(F == NULL || pid < 0)
		return NULL;
	ctl = rb_malloc(sizeof(ws_ctl_t));
	ctl->F = F;
	ctl->P = P;
	ctl->pid = pid;
	wsockd_count++;
	rb_dlinkAdd(ctl, &ctl->node, &wsock_daemons);
	return ctl;
}

static void
free_ws_daemon(ws_ctl_t * ctl)
{
	rb_dlink_node *ptr;
	ws_ctl_buf_t *ctl_buf;
	int x;
	if(ctl->cli_count)
		return;

	RB_DLINK_FOREACH(ptr, ctl->readq.head)
	{
		ctl_buf = ptr->data;
		for(x = 0; x < ctl_buf->nfds; x++)
			rb_close(ctl_buf->F[x]);

		rb_free(ctl_buf->buf);
		rb_free(ctl_buf);
	}

	RB_DLINK_FOREACH(ptr, ctl->writeq.head)
	{
		ctl_buf = ptr->data;
		for(x = 0; x < ctl_buf->nfds; x++)
			rb_close(ctl_buf->F[x]);

		rb_free(ctl_buf->buf);
		rb_free(ctl_buf);
	}
	rb_close(ctl->F);
	rb_close(ctl->P);
	rb_dlinkDelete(&ctl->node, &wsock_daemons);
	rb_free(ctl);
}

static char *wsockd_path;

static int wsockd_spin_count = 0;
static time_t last_spin;
static int wsockd_wait = 0;

void
restart_wsockd(void)
{
	rb_dlink_node *ptr, *next;
	ws_ctl_t *ctl;

	RB_DLINK_FOREACH_SAFE(ptr, next, wsock_daemons.head)
	{
		ctl = ptr->data;
		if(ctl->dead)
			continue;
		if(ctl->shutdown)
			continue;
		ctl->shutdown = 1;
		wsockd_count--;
		if(!ctl->cli_count)
		{
			rb_kill(ctl->pid, SIGKILL);
			free_ws_daemon(ctl);
		}
	}

	start_wsockd(ServerInfo.wsockd_count);
}

#if 0
static void
ws_killall(void)
{
	rb_dlink_node *ptr, *next;
	ws_ctl_t *ctl;
	RB_DLINK_FOREACH_SAFE(ptr, next, wsock_daemons.head)
	{
		ctl = ptr->data;
		if(ctl->dead)
			continue;
		ctl->dead = 1;
		if(!ctl->shutdown)
			wsockd_count--;
		rb_kill(ctl->pid, SIGKILL);
		if(!ctl->cli_count)
			free_ws_daemon(ctl);
	}
}
#endif

static void
ws_dead(ws_ctl_t * ctl)
{
	if(ctl->dead)
		return;

	ctl->dead = 1;
	rb_kill(ctl->pid, SIGKILL);	/* make sure the process is really gone */

	if(!ctl->shutdown)
	{
		wsockd_count--;
		ilog(L_MAIN, "wsockd helper died - attempting to restart");
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "wsockd helper died - attempting to restart");
		start_wsockd(1);
	}
}

static void
ws_do_pipe(rb_fde_t * F, void *data)
{
	int retlen;
	ws_ctl_t *ctl = data;
	retlen = rb_write(F, "0", 1);
	if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
	{
		ws_dead(ctl);
		return;
	}
	rb_setselect(F, RB_SELECT_READ, ws_do_pipe, data);
}

static void
restart_wsockd_event(void *unused)
{
	wsockd_spin_count = 0;
	last_spin = 0;
	wsockd_wait = 0;
	if(ServerInfo.wsockd_count > get_wsockd_count())
	{
		int start = ServerInfo.wsockd_count - get_wsockd_count();
		ilog(L_MAIN, "Attempting to restart wsockd processes");
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Attempting to restart wsockd processes");
		start_wsockd(start);
	}
}

int
start_wsockd(int count)
{
	rb_fde_t *F1, *F2;
	rb_fde_t *P1, *P2;
#ifdef _WIN32
	const char *suffix = ".exe";
#else
	const char *suffix = "";
#endif

	char fullpath[PATH_MAX + 1];
	char fdarg[6];
	const char *parv[2];
	char buf[128];
	char s_pid[10];
	pid_t pid;
	int started = 0, i;

	if(wsockd_wait)
		return 0;

	if(wsockd_spin_count > 20 && (rb_current_time() - last_spin < 5))
	{
		ilog(L_MAIN, "wsockd helper is spinning - will attempt to restart in 1 minute");
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "wsockd helper is spinning - will attempt to restart in 1 minute");
		rb_event_add("restart_wsockd_event", restart_wsockd_event, NULL, 60);
		wsockd_wait = 1;
		return 0;
	}

	wsockd_spin_count++;
	last_spin = rb_current_time();

	if(wsockd_path == NULL)
	{
		snprintf(fullpath, sizeof(fullpath), "%s%cwsockd%s", ircd_paths[IRCD_PATH_LIBEXEC], RB_PATH_SEPARATOR, suffix);

		if(access(fullpath, X_OK) == -1)
		{
			snprintf(fullpath, sizeof(fullpath), "%s%cbin%cwsockd%s",
				    ConfigFileEntry.dpath, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR, suffix);
			if(access(fullpath, X_OK) == -1)
			{
				ilog(L_MAIN,
				     "Unable to execute wsockd%s in %s or %s/bin",
				     suffix, ircd_paths[IRCD_PATH_LIBEXEC], ConfigFileEntry.dpath);
				return 0;
			}
		}
		wsockd_path = rb_strdup(fullpath);
	}
	rb_strlcpy(buf, "-ircd wsockd daemon", sizeof(buf));
	parv[0] = buf;
	parv[1] = NULL;

	for(i = 0; i < count; i++)
	{
		ws_ctl_t *ctl;
		if(rb_socketpair(AF_UNIX, SOCK_DGRAM, 0, &F1, &F2, "wsockd handle passing socket") == -1)
		{
			ilog(L_MAIN, "Unable to create wsockd - rb_socketpair failed: %s", strerror(errno));
			return started;
		}

		rb_set_buffers(F1, READBUF_SIZE);
		rb_set_buffers(F2, READBUF_SIZE);
		snprintf(fdarg, sizeof(fdarg), "%d", rb_get_fd(F2));
		rb_setenv("CTL_FD", fdarg, 1);
		if(rb_pipe(&P1, &P2, "wsockd pipe") == -1)
		{
			ilog(L_MAIN, "Unable to create wsockd - rb_pipe failed: %s", strerror(errno));
			return started;
		}
		snprintf(fdarg, sizeof(fdarg), "%d", rb_get_fd(P1));
		rb_setenv("CTL_PIPE", fdarg, 1);
		snprintf(s_pid, sizeof(s_pid), "%d", (int)getpid());
		rb_setenv("CTL_PPID", s_pid, 1);

#ifdef _WIN32
		SetHandleInformation((HANDLE) rb_get_fd(F2), HANDLE_FLAG_INHERIT, 1);
		SetHandleInformation((HANDLE) rb_get_fd(P1), HANDLE_FLAG_INHERIT, 1);
#endif

		pid = rb_spawn_process(wsockd_path, (const char **) parv);
		if(pid == -1)
		{
			ilog(L_MAIN, "Unable to create wsockd: %s\n", strerror(errno));
			rb_close(F1);
			rb_close(F2);
			rb_close(P1);
			rb_close(P2);
			return started;
		}
		started++;
		rb_close(F2);
		rb_close(P1);
		ctl = allocate_ws_daemon(F1, P2, pid);
		ws_read_ctl(ctl->F, ctl);
		ws_do_pipe(P2, ctl);

	}
	return started;
}

static void
ws_process_dead_fd(ws_ctl_t * ctl, ws_ctl_buf_t * ctl_buf)
{
	struct Client *client_p;
	char reason[256];
	uint32_t fd;

	if(ctl_buf->buflen < 6)
		return;		/* bogus message..drop it.. XXX should warn here */

	fd = buf_to_uint32(&ctl_buf->buf[1]);
	rb_strlcpy(reason, &ctl_buf->buf[5], sizeof(reason));
	client_p = find_cli_connid_hash(fd);
	if(client_p == NULL)
		return;
	if(IsAnyServer(client_p) || IsRegistered(client_p))
	{
		/* read any last moment ERROR, QUIT or the like -- jilles */
		if (!strcmp(reason, "Remote host closed the connection"))
			read_packet(client_p->localClient->F, client_p);
		if (IsAnyDead(client_p))
			return;
	}
	exit_client(client_p, client_p, &me, reason);
}


static void
ws_process_cmd_recv(ws_ctl_t * ctl)
{
	rb_dlink_node *ptr, *next;
	ws_ctl_buf_t *ctl_buf;
	unsigned long len;

	if(ctl->dead)
		return;

	RB_DLINK_FOREACH_SAFE(ptr, next, ctl->readq.head)
	{
		ctl_buf = ptr->data;
		switch (*ctl_buf->buf)
		{
		case 'D':
			ws_process_dead_fd(ctl, ctl_buf);
			break;
		default:
			ilog(L_MAIN, "Received invalid command from wsockd: %s", ctl_buf->buf);
			sendto_realops_snomask(SNO_GENERAL, L_ALL, "Received invalid command from wsockd");
			break;
		}
		rb_dlinkDelete(ptr, &ctl->readq);
		rb_free(ctl_buf->buf);
		rb_free(ctl_buf);
	}

}


static void
ws_read_ctl(rb_fde_t * F, void *data)
{
	ws_ctl_buf_t *ctl_buf;
	ws_ctl_t *ctl = data;
	int retlen;

	if(ctl->dead)
		return;
	do
	{
		ctl_buf = rb_malloc(sizeof(ws_ctl_buf_t));
		ctl_buf->buf = rb_malloc(READSIZE);
		retlen = rb_recv_fd_buf(ctl->F, ctl_buf->buf, READSIZE, ctl_buf->F, 4);
		ctl_buf->buflen = retlen;
		if(retlen <= 0)
		{
			rb_free(ctl_buf->buf);
			rb_free(ctl_buf);
		}
		else
			rb_dlinkAddTail(ctl_buf, &ctl_buf->node, &ctl->readq);
	}
	while(retlen > 0);

	if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
	{
		ws_dead(ctl);
		return;
	}
	ws_process_cmd_recv(ctl);
	rb_setselect(ctl->F, RB_SELECT_READ, ws_read_ctl, ctl);
}

static ws_ctl_t *
which_wsockd(void)
{
	ws_ctl_t *ctl, *lowest = NULL;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, wsock_daemons.head)
	{
		ctl = ptr->data;
		if(ctl->dead)
			continue;
		if(ctl->shutdown)
			continue;
		if(lowest == NULL)
		{
			lowest = ctl;
			continue;
		}
		if(ctl->cli_count < lowest->cli_count)
			lowest = ctl;
	}

	return (lowest);
}

static void
ws_write_ctl(rb_fde_t * F, void *data)
{
	ws_ctl_t *ctl = data;
	ws_ctl_buf_t *ctl_buf;
	rb_dlink_node *ptr, *next;
	int retlen, x;

	if(ctl->dead)
		return;

	RB_DLINK_FOREACH_SAFE(ptr, next, ctl->writeq.head)
	{
		ctl_buf = ptr->data;
		/* in theory unix sock_dgram shouldn't ever short write this.. */
		retlen = rb_send_fd_buf(ctl->F, ctl_buf->F, ctl_buf->nfds, ctl_buf->buf, ctl_buf->buflen, ctl->pid);
		if(retlen > 0)
		{
			rb_dlinkDelete(ptr, &ctl->writeq);
			for(x = 0; x < ctl_buf->nfds; x++)
				rb_close(ctl_buf->F[x]);
			rb_free(ctl_buf->buf);
			rb_free(ctl_buf);

		}
		if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
		{
			ws_dead(ctl);
			return;
		}
		else
		{
			rb_setselect(ctl->F, RB_SELECT_WRITE, ws_write_ctl, ctl);
		}
	}
}

static void
ws_cmd_write_queue(ws_ctl_t * ctl, rb_fde_t ** F, int count, const void *buf, size_t buflen)
{
	ws_ctl_buf_t *ctl_buf;
	int x;

	/* don't bother */
	if(ctl->dead)
		return;

	ctl_buf = rb_malloc(sizeof(ws_ctl_buf_t));
	ctl_buf->buf = rb_malloc(buflen);
	memcpy(ctl_buf->buf, buf, buflen);
	ctl_buf->buflen = buflen;

	for(x = 0; x < count && x < MAXPASSFD; x++)
	{
		ctl_buf->F[x] = F[x];
	}
	ctl_buf->nfds = count;
	rb_dlinkAddTail(ctl_buf, &ctl_buf->node, &ctl->writeq);
	ws_write_ctl(ctl->F, ctl);
}

ws_ctl_t *
start_wsockd_accept(rb_fde_t * sslF, rb_fde_t * plainF, uint32_t id)
{
	rb_fde_t *F[2];
	ws_ctl_t *ctl;
	char buf[5];
	F[0] = sslF;
	F[1] = plainF;

	buf[0] = 'A';
	uint32_to_buf(&buf[1], id);
	ctl = which_wsockd();
	if(!ctl)
		return NULL;
	ctl->cli_count++;
	ws_cmd_write_queue(ctl, F, 2, buf, sizeof(buf));
	return ctl;
}

void
wsockd_decrement_clicount(ws_ctl_t * ctl)
{
	if(ctl == NULL)
		return;

	ctl->cli_count--;
	if(ctl->shutdown && !ctl->cli_count)
	{
		ctl->dead = 1;
		rb_kill(ctl->pid, SIGKILL);
	}
	if(ctl->dead && !ctl->cli_count)
	{
		free_ws_daemon(ctl);
	}
}

static void
cleanup_dead_ws(void *unused)
{
	rb_dlink_node *ptr, *next;
	ws_ctl_t *ctl;
	RB_DLINK_FOREACH_SAFE(ptr, next, wsock_daemons.head)
	{
		ctl = ptr->data;
		if(ctl->dead && !ctl->cli_count)
		{
			free_ws_daemon(ctl);
		}
	}
}

int
get_wsockd_count(void)
{
	return wsockd_count;
}

void
wsockd_foreach_info(void (*func)(void *data, pid_t pid, int cli_count, enum wsockd_status status), void *data)
{
	rb_dlink_node *ptr, *next;
	ws_ctl_t *ctl;
	RB_DLINK_FOREACH_SAFE(ptr, next, wsock_daemons.head)
	{
		ctl = ptr->data;
		func(data, ctl->pid, ctl->cli_count,
			ctl->dead ? WSOCKD_DEAD :
				(ctl->shutdown ? WSOCKD_SHUTDOWN : WSOCKD_ACTIVE));
	}
}

void
init_wsockd(void)
{
	rb_event_addish("cleanup_dead_ws", cleanup_dead_ws, NULL, 60);
}
