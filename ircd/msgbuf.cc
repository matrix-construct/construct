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

#include <ircd/stdinc.h>
#include <ircd/msgbuf.h>
#include <ircd/client.h>
#include <ircd/ircd.h>

namespace ircd {

/*
 * parse a message into a MsgBuf.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_parse(struct MsgBuf *msgbuf, char *line)
{
	char *ch;
	char *parv[MAXPARA];
	size_t n_para;

	/* skip any leading spaces */
	for (ch = line; *ch && *ch == ' '; ch++)
		;

	msgbuf_init(msgbuf);

	if (*ch == '@')
	{
		char *t = ch + 1;

		ch = strchr(ch, ' ');
		if (ch != NULL)
		{
			while (1)
			{
				char *next = strchr(t, ';');
				char *eq = strchr(t, '=');

				if (next != NULL)
				{
					*next = '\0';

					if (eq > next)
						eq = NULL;
				}

				if (eq != NULL)
					*eq++ = '\0';

				if (*t && *t != ' ')
					msgbuf_append_tag(msgbuf, t, eq, 0);
				else
					break;

				if (next != NULL)
					t = next + 1;
				else
					break;
			}

			*ch++ = '\0';
		}
	}

	/* skip any whitespace between tags and origin */
	for (; *ch && *ch == ' '; ch++)
		;

	if (*ch == ':')
	{
		ch++;
		msgbuf->origin = ch;

		char *end = strchr(ch, ' ');
		if (end == NULL)
			return 1;

		*end = '\0';

		for (ch = end + 1; *ch && *ch == ' '; ch++)
			;
	}

	if (*ch == '\0')
		return 1;

	n_para = rb_string_to_array(ch, parv, MAXPARA);
	if (n_para == 0)
		return 1;

	msgbuf->cmd = parv[0];
	for (size_t i = 0; i < n_para; i++)
		msgbuf_append_para(msgbuf, parv[i]);

	return 0;
}

static int
msgbuf_has_matching_tags(struct MsgBuf *msgbuf, unsigned int capmask)
{
	for (size_t i = 0; i < msgbuf->n_tags; i++)
	{
		if ((msgbuf->tags[i].capmask & capmask) != 0)
			return 1;
	}

	return 0;
}

static void
msgbuf_unparse_tags(char *buf, size_t buflen, struct MsgBuf *msgbuf, unsigned int capmask)
{
	if (!msgbuf_has_matching_tags(msgbuf, capmask))
		return;

	*buf = '@';

	for (size_t i = 0; i < msgbuf->n_tags; i++)
	{
		if ((msgbuf->tags[i].capmask & capmask) == 0)
			continue;

		if (i != 0)
			rb_strlcat(buf, ";", buflen);

		rb_strlcat(buf, msgbuf->tags[i].key, buflen);

		/* XXX properly handle escaping */
		if (msgbuf->tags[i].value)
		{
			rb_strlcat(buf, "=", buflen);
			rb_strlcat(buf, msgbuf->tags[i].value, buflen);
		}
	}

	rb_strlcat(buf, " ", buflen);
}

void
msgbuf_unparse_prefix(char *buf, size_t buflen, struct MsgBuf *msgbuf, unsigned int capmask)
{
	memset(buf, 0, buflen);

	if (msgbuf->n_tags > 0)
		msgbuf_unparse_tags(buf, buflen, msgbuf, capmask);

	rb_snprintf_append(buf, buflen, ":%s ", msgbuf->origin != NULL ? msgbuf->origin : me.name);

	if (msgbuf->cmd != NULL)
		rb_snprintf_append(buf, buflen, "%s ", msgbuf->cmd);

	if (msgbuf->target != NULL)
		rb_snprintf_append(buf, buflen, "%s ", msgbuf->target);
}

/*
 * unparse a pure MsgBuf into a buffer.
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_unparse(char *buf, size_t buflen, struct MsgBuf *msgbuf, unsigned int capmask)
{
	msgbuf_unparse_prefix(buf, buflen, msgbuf, capmask);

	for (size_t i = msgbuf->cmd != NULL ? 0 : 1; i < msgbuf->n_para; i++)
	{
		if (i == (msgbuf->n_para - 1))
		{
			if (strchr(msgbuf->para[i], ' ') != NULL)
				rb_snprintf_append(buf, buflen, ":%s", msgbuf->para[i]);
			else
				rb_strlcat(buf, msgbuf->para[i], buflen);
		}
		else
			rb_strlcat(buf, msgbuf->para[i], buflen);
	}

	return 0;
}

/*
 * unparse a MsgBuf stem + format string into a buffer
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_vunparse_fmt(char *buf, size_t buflen, struct MsgBuf *head, unsigned int capmask, const char *fmt, va_list va)
{
	char *ws;
	size_t prefixlen;

	msgbuf_unparse_prefix(buf, buflen, head, capmask);
	prefixlen = strlen(buf);

	ws = buf + prefixlen;
	vsnprintf(ws, buflen - prefixlen, fmt, va);

	return 0;
}

/*
 * unparse a MsgBuf stem + format string into a buffer (with va_list handling)
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_unparse_fmt(char *buf, size_t buflen, struct MsgBuf *head, unsigned int capmask, const char *fmt, ...)
{
	va_list va;
	int res;

	va_start(va, fmt);
	res = msgbuf_vunparse_fmt(buf, buflen, head, capmask, fmt, va);
	va_end(va);

	return res;
}


} // namespace ircd
