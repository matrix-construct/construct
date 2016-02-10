/*
 * charybdis - an advanced ircd.
 * Copyright (c) 2016 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CHARYBDIS__MSGBUF_H
#define CHARYBDIS__MSGBUF_H

#define MAXPARA		(15)

/* a key-value structure for each message tag. */
struct MsgTag {
	const char *key;
	const char *value;
};

struct MsgBuf {
	size_t n_tags;
	struct MsgTag tags[MAXPARA];

	const char *origin;
	const char *cmd;

	size_t parselen;
	size_t n_para;
	const char *para[MAXPARA];
};

/*
 * parse a message into a MsgBuf.
 * returns 0 on success, 1 on error.
 */
int msgbuf_parse(struct MsgBuf *msgbuf, const char *line);

/*
 * unparse a MsgBuf header plus payload into a buffer.
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int msgbuf_unparse(char *buf, struct MsgBuf *head, const char *fmt, ...) AFP(3, 4);
int msgbuf_vunparse(char *buf, struct MsgBuf *head, const char *fmt, va_list va);

#endif
