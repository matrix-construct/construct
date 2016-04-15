/* ircd/batch.c - batch management
 * Copyright (c) 2016 Elizabeth Myers <elizabeth@interlinked.me>
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
#include "batch.h"
#include "client.h"
#include "s_serv.h"
#include "send.h"
#include "channel.h"
#include "hash.h"
#include "s_assert.h"
#include "rb_radixtree.h"

/* Multiple batches may be in progress for each slot. */
rb_dlink_list batches[BATCH_LAST];

static inline void
generate_batch_id(char *ptr, size_t len)
{
	size_t i;
	const char batchchars[65] =
		"\0._0123456789"		/* Zero-indexed */
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	len--; /* Room for \0 */
	for(i = 0; i < len; i++)
	{
		char r;

		do
		{
			r = batchchars[rand() & 0x7F]; /* random int between 0-64 */

			if(r == '\0' && i > 3)
				/* We have enough chars */
				goto end;
		} while(r == '\0');

		ptr[i] = r;
	}

end:
	ptr[i] = '\0';
}

struct Batch *
start_batch(batch_type batch, void *data, int parc, ...)
{
	struct Batch *batch_p = rb_malloc(sizeof(struct Batch));

	batch_p->batch = batch;
	generate_batch_id(batch_p->id, sizeof(batch_p->id));
	batch_p->data = data;
	batch_p->parc = parc;
	if(parc > 0)
	{
		/* Get the argument list */
		va_list args;

		batch_p->parv = rb_malloc(sizeof(char *) * parc);

		va_start(args, parc);
		for(size_t i = 0; i < parc; i++)
			batch_p->parv[i] = va_arg(args, char *);
		va_end(args);
	}

	/* Batch-type specific processing */
	switch(batch)
	{
	case BATCH_NETSPLIT:
	case BATCH_NETJOIN:
		{
			/* Build list of channels affected by the batch */
			rb_dlink_list *clist;
			rb_radixtree_iteration_state iter;
			struct Channel *chptr;

			batch_p->pdata = clist = rb_malloc(sizeof(rb_dlink_list));

			/* Look for channels we need to send the batch to */
			RB_RADIXTREE_FOREACH(chptr, &iter, channel_tree)
			{
				rb_dlink_node *ptr;

				if(rb_dlink_list_length(&chptr->locmembers) == 0)
					/* They're all remotes, so don't send a batch */
					continue;

				/* Hunt for members in the channel from the target server
				 * If we find one, send the channel a BATCH message */
				RB_DLINK_FOREACH(ptr, chptr->members.head)
				{
					struct Client *client_p = ptr->data;

					if(client_p->from == data)
					{
						rb_dlinkAddAlloc(rb_strdup(chptr->chname), clist);
						sendto_channel_local_with_capability(ALL_MEMBERS, CLICAP_BATCH, NOCAPS,
							chptr, ":%s BATCH +%s %s %s",
							me.name,
							batch == BATCH_NETSPLIT ? "netsplit" : "netjoin",
							batch_p->parv[0], batch_p->parv[1]);
						break;
					}
				}
			}
		}
		break;
	default:
		s_assert(0);
		break;
	}

	rb_dlinkAdd(batch_p, &batch_p->node, &batches[batch]);

	return batch_p;
}

void
finish_batch(struct Batch *batch_p)
{
	if(batch_p == NULL)
		return;

	/* Batch type-specific processing */
	switch(batch_p->batch)
	{
	case BATCH_NETSPLIT:
	case BATCH_NETJOIN:
		{
			rb_dlink_list *clist = batch_p->pdata;
			rb_dlink_node *ptr, *nptr;

			RB_DLINK_FOREACH_SAFE(ptr, nptr, clist->head)
			{
				struct Channel *chptr = find_channel(ptr->data);

				if(chptr != NULL) /* Shouldn't be but just in case... */
				{
					sendto_channel_local_with_capability(ALL_MEMBERS, CLICAP_BATCH, NOCAPS,
						chptr, ":%s BATCH -%s %s %s",
						me.name,
						batch_p->batch == BATCH_NETSPLIT ? "netsplit" : "netjoin",
						batch_p->parv[0], batch_p->parv[1]);
				}

				rb_free(ptr->data);
				rb_dlinkDestroy(ptr, clist);
			}

			rb_free(clist);
		}
		break;
	default:
		s_assert(0);
		break;
	}

	/* Free all the strings */
	for(size_t i = 0; i < (batch_p->parc - 1); i++)
		rb_free(batch_p->parv[i]);

	rb_free(batch_p->parv);

	rb_dlinkDelete(&batch_p->node, &batches[batch_p->batch]);
	rb_free(batch_p);
}

struct Batch *
find_batch(batch_type batch, void *data)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, batches[batch].head)
	{
		struct Batch *batch_p = ptr->data;

		if(batch_p->data == data)
			return batch_p;
	}

	return NULL;
}
