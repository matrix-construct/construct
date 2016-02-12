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
	const char *key;		/* the key of the tag (must be set) */
	const char *value;		/* the value of the tag or NULL */
	unsigned int capmask;		/* the capability mask this tag belongs to (used only when sending) */
};

struct MsgBuf {
	size_t n_tags;			/* the number of tags in the MsgBuf */
	struct MsgTag tags[MAXPARA];	/* the tags themselves, upto MAXPARA tags available */

	const char *origin;		/* the origin of the message (or NULL) */
	const char *cmd;		/* the cmd/verb of the message (also para[0]) */

	size_t parselen;		/* the length of the message */
	size_t n_para;			/* the number of parameters (always at least 1) */
	const char *para[MAXPARA];	/* parameters vector (starting with cmd as para[0]) */
};

/*
 * parse a message into a MsgBuf.
 * returns 0 on success, 1 on error.
 */
int msgbuf_parse(struct MsgBuf *msgbuf, char *line);

/*
 * unparse a pure MsgBuf into a buffer.
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int msgbuf_unparse(char *buf, struct MsgBuf *msgbuf, unsigned int capmask);

/*
 * unparse a MsgBuf header plus payload into a buffer.
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int msgbuf_unparse_fmt(char *buf, struct MsgBuf *head, unsigned int capmask, const char *fmt, ...) AFP(3, 4);
int msgbuf_vunparse_fmt(char *buf, struct MsgBuf *head, unsigned int capmask, const char *fmt, va_list va);

static inline void
msgbuf_init(struct MsgBuf *msgbuf)
{
	memset(msgbuf, 0, sizeof(*msgbuf));
}

static inline void
msgbuf_append_tag(struct MsgBuf *msgbuf, const char *key, const char *value)
{
	s_assert(msgbuf->n_tags < MAXPARA);

	msgbuf->tags[msgbuf->n_tags].key = key;
	msgbuf->tags[msgbuf->n_tags].value = value;
	msgbuf->n_tags++;
}

static inline void
msgbuf_append_para(struct MsgBuf *msgbuf, const char *para)
{
	s_assert(msgbuf->n_para < MAXPARA);

	msgbuf->para[msgbuf->n_para] = para;
	msgbuf->n_para++;
}

#endif
