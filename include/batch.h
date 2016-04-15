/* ircd/batch.h - batch management
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
#include "client.h"

typedef enum
{
	BATCH_NETSPLIT,
	BATCH_NETJOIN,
	BATCH_LAST,
} batch_type;

/* Used for netsplits/netjoins */
struct Batch
{
	batch_type batch;	/* Type of batch */
	char id[8];		/* Id of batch */
	void *data;		/* Batch-specific data */
	void *pdata;		/* Private data */
	int parc;		/* Batch parameter count */
	char **parv;		/* Batch parameters */

	rb_dlink_node node;
};

struct Batch *start_batch(batch_type batch, void *data, int parc, ...);
void finish_batch(struct Batch *batch_p);

struct Batch *find_batch(batch_type batch, void *data);
