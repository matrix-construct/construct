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

#include "stdinc.h"
#include "ircd_defs.h"
#include "msgbuf.h"

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
	int i;

	/* skip any leading spaces */
	for (ch = line; *ch && *ch == ' '; ch++)
		;

	msgbuf_init(msgbuf);

	if (*ch == '@')
	{
		char *t = ch++;

		ch = strchr(ch, ' ');
		if (ch != NULL)
		{
			while (t < ch)
			{
				char *next = strchr(t, ';');
				char *eq = strchr(t, '=');

				if (next != NULL)
					*next = '\0';

				if (eq > next)
					eq = NULL;

				if (eq != NULL)
					*eq = '\0';

				msgbuf_append_tag(msgbuf, t, eq);
				t = next + 1;
			}
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
	msgbuf->n_para = n_para - 1;

	for (i = 1; i < n_para; i++)
		msgbuf_append_para(msgbuf, parv[i]);

	return 0;
}
