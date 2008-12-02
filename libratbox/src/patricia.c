/*
 * Yanked out of Net::Patricia by Aaron Sethman <androsyn@ratbox.org>
 * 
 * This was then yanked out of the ratbox/devel/src tree and stuffed into
 * libratbox and had function names changed, but otherwise not really altered.
 *
 * $Id: patricia.c 24244 2007-08-22 19:04:55Z androsyn $
 * Dave Plonka <plonka@doit.wisc.edu>
 *
 * This product includes software developed by the University of Michigan,
 * Merit Network, Inc., and their contributors. 
 *
 * This file had been called "radix.c" in the MRT sources.
 *
 * I renamed it to "patricia.c" since it's not an implementation of a general
 * radix trie.  Also I pulled in various requirements from "prefix.c" and
 * "demo.c" so that it could be used as a standalone API.
 *
 * This product includes software developed by the University of Michigan, Merit
 * Network, Inc., and their contributors.
 *
 */
#include <libratbox_config.h>
#include <ratbox_lib.h>

/* Enable both of these to debug patricia.c
 * #define NOTYET 1
 * #define PATRICIA_DEBUG 1
 */

#define PREFIX_HEAP_COUNT	1024
#define NODE_HEAP_COUNT		1024
#define PATRICIA_HEAP_COUNT	128

void
rb_init_patricia(void)
{
	return;
}

/* prefix_tochar
 * convert prefix information to bytes
 */
static uint8_t *
prefix_tochar(rb_prefix_t *prefix)
{
	if(prefix == NULL)
		return (NULL);

	return ((uint8_t *)&prefix->add.sin);
}

static int
comp_with_mask(void *addr, void *dest, unsigned int mask)
{

	if( /* mask/8 == 0 || */ memcmp(addr, dest, mask / 8) == 0)
	{
		int n = mask / 8;
		int m = ((-1) << (8 - (mask % 8)));

		if(mask % 8 == 0 || (((uint8_t *)addr)[n] & m) == (((uint8_t *)dest)[n] & m))
			return (1);
	}
	return (0);
}

#ifdef NOTYET
static char *
prefix_toa2x(rb_prefix_t *prefix, char *buf, int buf_len, int with_len)
{
	static char tmp[6];
	if(prefix == NULL)
	{
		strcpy(buf, "(NULL)");
		return (buf);
	}
	inet_ntop(prefix->family, &prefix->add.sin, buf, buf_len);
	if(with_len)
	{
		rb_snprintf(tmp, sizeof(tmp), "/%d", prefix->bitlen);
		strcat(buf, tmp);
	}
	return (buf);
}

/* prefix_toa2
 * convert prefix information to ascii string
 */

static char *
prefix_toa2(rb_prefix_t *prefix, char *buff, int buf_len)
{
	return (prefix_toa2x(prefix, buff, buf_len, 0));
}

static char *
prefix_toa(rb_prefix_t *prefix)
{
#ifdef RB_IPV6
	static char buf[INET6_ADDRSTRLEN + 6];
#else
	static char buf[16 + 6];
#endif
	return (prefix_toa2(prefix, buf, sizeof(buf)));
}
#endif
static rb_prefix_t *
New_Prefix2(int family, void *dest, int bitlen, rb_prefix_t *prefix)
{
	int dynamic_allocated = 0;
#ifdef RB_IPV6
	int default_bitlen = 128;
#else
	int default_bitlen = 32;
#endif

#ifdef RB_IPV6
	if(family == AF_INET6)
	{
		default_bitlen = 128;
		if(prefix == NULL)
		{
			prefix = rb_malloc(sizeof(rb_prefix_t));
			dynamic_allocated++;
		}
		memcpy(&prefix->add.sin6, dest, 16);
	}
	else
#endif /* RB_IPV6 */
	if(family == AF_INET)
	{
		if(prefix == NULL)
		{
			prefix = rb_malloc(sizeof(rb_prefix_t));
			dynamic_allocated++;
		}
		memcpy(&prefix->add.sin, dest, 4);
	}
	else
	{
		return (NULL);
	}

	prefix->bitlen = (bitlen >= 0) ? bitlen : default_bitlen;
	prefix->family = family;
	prefix->ref_count = 0;
	if(dynamic_allocated)
	{
		prefix->ref_count++;
	}
	return (prefix);
}

static rb_prefix_t *
New_Prefix(int family, void *dest, int bitlen)
{
	return (New_Prefix2(family, dest, bitlen, NULL));
}

/* ascii2prefix
 */
static rb_prefix_t *
ascii2prefix(int family, const char *string)
{
	long bitlen, maxbitlen = 0;
	char *cp;
	struct in_addr sinaddr;
#ifdef RB_IPV6
	struct in6_addr sinaddr6;
#endif /* RB_IPV6 */
	int result;
	char save[MAXLINE];

	if(string == NULL)
		return (NULL);

	/* easy way to handle both families */
	if(family == 0)
	{
		family = AF_INET;
#ifdef RB_IPV6
		if(strchr(string, ':'))
			family = AF_INET6;
#endif /* RB_IPV6 */
	}
	if(family == AF_INET)
	{
		maxbitlen = 32;
	}
#ifdef RB_IPV6
	else if(family == AF_INET6)
	{
		maxbitlen = 128;
	}
#endif /* RB_IPV6 */

	if((cp = strchr(string, '/')) != NULL)
	{
		bitlen = atol(cp + 1);
		/* *cp = '\0'; */
		/* copy the string to save. Avoid destroying the string */
		assert(cp - string < MAXLINE);
		memcpy(save, string, cp - string);
		save[cp - string] = '\0';
		string = save;
		if(bitlen <= 0 || bitlen > maxbitlen)
			bitlen = maxbitlen;
	}
	else
	{
		bitlen = maxbitlen;
	}

	if(family == AF_INET)
	{
		if((result = rb_inet_pton(AF_INET, string, &sinaddr)) <= 0)
			return (NULL);
		return (New_Prefix(AF_INET, &sinaddr, bitlen));
	}
#ifdef RB_IPV6
	else if(family == AF_INET6)
	{
		if((result = rb_inet_pton(AF_INET6, string, &sinaddr6)) <= 0)
			return (NULL);
		return (New_Prefix(AF_INET6, &sinaddr6, bitlen));
	}
#endif /* RB_IPV6 */
	else
		return (NULL);
}

static rb_prefix_t *
Ref_Prefix(rb_prefix_t *prefix)
{
	if(prefix == NULL)
		return (NULL);
	if(prefix->ref_count == 0)
	{
		/* make a copy in case of a static prefix */
		return (New_Prefix2(prefix->family, &prefix->add, prefix->bitlen, NULL));
	}
	prefix->ref_count++;
	return (prefix);
}

static void
Deref_Prefix(rb_prefix_t *prefix)
{
	if(prefix == NULL)
		return;
	/* for secure programming, raise an assert. no static prefix can call this */
	assert(prefix->ref_count > 0);

	prefix->ref_count--;
	assert(prefix->ref_count >= 0);
	if(prefix->ref_count <= 0)
	{
		rb_free(prefix);
		return;
	}
}

/* } */

// #define PATRICIA_DEBUG 1 

static int num_active_patricia = 0;

/* these routines support continuous mask only */

rb_patricia_tree_t *
rb_new_patricia(int maxbits)
{
	rb_patricia_tree_t *patricia = rb_malloc(sizeof(rb_patricia_tree_t));

	patricia->maxbits = maxbits;
	patricia->head = NULL;
	patricia->num_active_node = 0;
	assert(maxbits <= RB_PATRICIA_MAXBITS);	/* XXX */
	num_active_patricia++;
	return (patricia);
}


/*
 * if func is supplied, it will be called as func(node->data)
 * before deleting the node
 */

void
rb_clear_patricia(rb_patricia_tree_t *patricia, void (*func) (void *))
{
	assert(patricia);
	if(patricia->head)
	{

		rb_patricia_node_t *Xstack[RB_PATRICIA_MAXBITS + 1];
		rb_patricia_node_t **Xsp = Xstack;
		rb_patricia_node_t *Xrn = patricia->head;

		while(Xrn)
		{
			rb_patricia_node_t *l = Xrn->l;
			rb_patricia_node_t *r = Xrn->r;

			if(Xrn->prefix)
			{
				Deref_Prefix(Xrn->prefix);
				if(Xrn->data && func)
					func(Xrn->data);
			}
			else
			{
				assert(Xrn->data == NULL);
			}
			rb_free(Xrn);
			patricia->num_active_node--;

			if(l)
			{
				if(r)
				{
					*Xsp++ = r;
				}
				Xrn = l;
			}
			else if(r)
			{
				Xrn = r;
			}
			else if(Xsp != Xstack)
			{
				Xrn = *(--Xsp);
			}
			else
			{
				Xrn = (rb_patricia_node_t *)0;
			}
		}
	}
	assert(patricia->num_active_node == 0);
	rb_free(patricia);
}


void
rb_destroy_patricia(rb_patricia_tree_t *patricia, void (*func) (void *))
{
	rb_clear_patricia(patricia, func);
	num_active_patricia--;
}


/*
 * if func is supplied, it will be called as func(node->prefix, node->data)
 */

void
rb_patricia_process(rb_patricia_tree_t *patricia, void (*func) (rb_prefix_t *, void *))
{
	rb_patricia_node_t *node;
	assert(func);

	RB_PATRICIA_WALK(patricia->head, node)
	{
		func(node->prefix, node->data);
	}
	RB_PATRICIA_WALK_END;
}

rb_patricia_node_t *
rb_patricia_search_exact(rb_patricia_tree_t *patricia, rb_prefix_t *prefix)
{
	rb_patricia_node_t *node;
	uint8_t *addr;
	unsigned int bitlen;

	assert(patricia);
	assert(prefix);
	assert(prefix->bitlen <= patricia->maxbits);

	if(patricia->head == NULL)
		return (NULL);

	node = patricia->head;
	addr = rb_prefix_touchar(prefix);
	bitlen = prefix->bitlen;

	while(node->bit < bitlen)
	{

		if(BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07)))
		{
#ifdef PATRICIA_DEBUG
			if(node->prefix)
				fprintf(stderr,
					"patricia_search_exact: take right %s/%d\n",
					prefix_toa(node->prefix), node->prefix->bitlen);
			else
				fprintf(stderr,
					"patricia_search_exact: take right at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->r;
		}
		else
		{
#ifdef PATRICIA_DEBUG
			if(node->prefix)
				fprintf(stderr,
					"patricia_search_exact: take left %s/%d\n",
					prefix_toa(node->prefix), node->prefix->bitlen);
			else
				fprintf(stderr,
					"patricia_search_exact: take left at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->l;
		}

		if(node == NULL)
			return (NULL);
	}

#ifdef PATRICIA_DEBUG
	if(node->prefix)
		fprintf(stderr, "patricia_search_exact: stop at %s/%d %d\n",
			prefix_toa(node->prefix), node->prefix->bitlen, node->bit);
	else
		fprintf(stderr, "patricia_search_exact: stop at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
	if(node->bit > bitlen || node->prefix == NULL)
		return (NULL);
	assert(node->bit == bitlen);
	assert(node->bit == node->prefix->bitlen);
	if(comp_with_mask(prefix_tochar(node->prefix), prefix_tochar(prefix), bitlen))
	{
#ifdef PATRICIA_DEBUG
		fprintf(stderr, "patricia_search_exact: found %s/%d\n",
			prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		return (node);
	}
	return (NULL);
}

/* if inclusive != 0, "best" may be the given prefix itself */
rb_patricia_node_t *
rb_patricia_search_best2(rb_patricia_tree_t *patricia, rb_prefix_t *prefix, int inclusive)
{
	rb_patricia_node_t *node;
	rb_patricia_node_t *stack[RB_PATRICIA_MAXBITS + 1];
	uint8_t *addr;
	unsigned int bitlen;
	int cnt = 0;

	assert(patricia);
	assert(prefix);
	assert(prefix->bitlen <= patricia->maxbits);

	if(patricia->head == NULL)
		return (NULL);

	node = patricia->head;
	addr = rb_prefix_touchar(prefix);
	bitlen = prefix->bitlen;

	while(node->bit < bitlen)
	{

		if(node->prefix)
		{
#ifdef PATRICIA_DEBUG
			fprintf(stderr,
				"patricia_search_best: push %s/%d\n",
				prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
			stack[cnt++] = node;
		}

		if(BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07)))
		{
#ifdef PATRICIA_DEBUG
			if(node->prefix)
				fprintf(stderr,
					"patricia_search_best: take right %s/%d\n",
					prefix_toa(node->prefix), node->prefix->bitlen);
			else
				fprintf(stderr,
					"patricia_search_best: take right at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->r;
		}
		else
		{
#ifdef PATRICIA_DEBUG
			if(node->prefix)
				fprintf(stderr,
					"patricia_search_best: take left %s/%d\n",
					prefix_toa(node->prefix), node->prefix->bitlen);
			else
				fprintf(stderr,
					"patricia_search_best: take left at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->l;
		}

		if(node == NULL)
			break;
	}

	if(inclusive && node && node->prefix)
		stack[cnt++] = node;

#ifdef PATRICIA_DEBUG
	if(node == NULL)
		fprintf(stderr, "patricia_search_best: stop at null\n");
	else if(node->prefix)
		fprintf(stderr, "patricia_search_best: stop at %s/%d\n",
			prefix_toa(node->prefix), node->prefix->bitlen);
	else
		fprintf(stderr, "patricia_search_best: stop at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */

	if(cnt <= 0)
		return (NULL);

	while(--cnt >= 0)
	{
		node = stack[cnt];
#ifdef PATRICIA_DEBUG
		fprintf(stderr, "patricia_search_best: pop %s/%d\n",
			prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		if(comp_with_mask(prefix_tochar(node->prefix),
				  prefix_tochar(prefix), node->prefix->bitlen))
		{
#ifdef PATRICIA_DEBUG
			fprintf(stderr,
				"patricia_search_best: found %s/%d\n",
				prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
			return (node);
		}
	}
	return (NULL);
}


rb_patricia_node_t *
rb_patricia_search_best(rb_patricia_tree_t *patricia, rb_prefix_t *prefix)
{
	return (rb_patricia_search_best2(patricia, prefix, 1));
}


rb_patricia_node_t *
rb_patricia_lookup(rb_patricia_tree_t *patricia, rb_prefix_t *prefix)
{
	rb_patricia_node_t *node, *new_node, *parent, *glue;
	uint8_t *addr, *test_addr;
	unsigned int bitlen, check_bit, differ_bit;
	unsigned int i, j, r;

	assert(patricia);
	assert(prefix);
	assert(prefix->bitlen <= patricia->maxbits);

	if(patricia->head == NULL)
	{
		node = rb_malloc(sizeof(rb_patricia_node_t));
		node->bit = prefix->bitlen;
		node->prefix = Ref_Prefix(prefix);
		node->parent = NULL;
		node->l = node->r = NULL;
		node->data = NULL;
		patricia->head = node;
#ifdef PATRICIA_DEBUG
		fprintf(stderr,
			"patricia_lookup: new_node #0 %s/%d (head)\n",
			prefix_toa(prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		patricia->num_active_node++;
		return (node);
	}

	addr = rb_prefix_touchar(prefix);
	bitlen = prefix->bitlen;
	node = patricia->head;

	while(node->bit < bitlen || node->prefix == NULL)
	{

		if(node->bit < patricia->maxbits &&
		   BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07)))
		{
			if(node->r == NULL)
				break;
#ifdef PATRICIA_DEBUG
			if(node->prefix)
				fprintf(stderr,
					"patricia_lookup: take right %s/%d\n",
					prefix_toa(node->prefix), node->prefix->bitlen);
			else
				fprintf(stderr, "patricia_lookup: take right at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->r;
		}
		else
		{
			if(node->l == NULL)
				break;
#ifdef PATRICIA_DEBUG
			if(node->prefix)
				fprintf(stderr,
					"patricia_lookup: take left %s/%d\n",
					prefix_toa(node->prefix), node->prefix->bitlen);
			else
				fprintf(stderr, "patricia_lookup: take left at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->l;
		}

		assert(node);
	}

	assert(node->prefix);
#ifdef PATRICIA_DEBUG
	fprintf(stderr, "patricia_lookup: stop at %s/%d\n",
		prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */

	test_addr = rb_prefix_touchar(node->prefix);
	/* find the first bit different */
	check_bit = (node->bit < bitlen) ? node->bit : bitlen;
	differ_bit = 0;
	for(i = 0; i * 8 < check_bit; i++)
	{
		if((r = (addr[i] ^ test_addr[i])) == 0)
		{
			differ_bit = (i + 1) * 8;
			continue;
		}
		/* I know the better way, but for now */
		for(j = 0; j < 8; j++)
		{
			if(BIT_TEST(r, (0x80 >> j)))
				break;
		}
		/* must be found */
		assert(j < 8);
		differ_bit = i * 8 + j;
		break;
	}
	if(differ_bit > check_bit)
		differ_bit = check_bit;
#ifdef PATRICIA_DEBUG
	fprintf(stderr, "patricia_lookup: differ_bit %d\n", differ_bit);
#endif /* PATRICIA_DEBUG */

	parent = node->parent;
	while(parent && parent->bit >= differ_bit)
	{
		node = parent;
		parent = node->parent;
#ifdef PATRICIA_DEBUG
		if(node->prefix)
			fprintf(stderr, "patricia_lookup: up to %s/%d\n",
				prefix_toa(node->prefix), node->prefix->bitlen);
		else
			fprintf(stderr, "patricia_lookup: up to %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
	}

	if(differ_bit == bitlen && node->bit == bitlen)
	{
		if(node->prefix)
		{
#ifdef PATRICIA_DEBUG
			fprintf(stderr, "patricia_lookup: found %s/%d\n",
				prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
			return (node);
		}
		node->prefix = Ref_Prefix(prefix);
#ifdef PATRICIA_DEBUG
		fprintf(stderr,
			"patricia_lookup: new node #1 %s/%d (glue mod)\n",
			prefix_toa(prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		assert(node->data == NULL);
		return (node);
	}

	new_node = rb_malloc(sizeof(rb_patricia_node_t));
	new_node->bit = prefix->bitlen;
	new_node->prefix = Ref_Prefix(prefix);
	new_node->parent = NULL;
	new_node->l = new_node->r = NULL;
	new_node->data = NULL;
	patricia->num_active_node++;

	if(node->bit == differ_bit)
	{
		new_node->parent = node;
		if(node->bit < patricia->maxbits &&
		   BIT_TEST(addr[node->bit >> 3], 0x80 >> (node->bit & 0x07)))
		{
			assert(node->r == NULL);
			node->r = new_node;
		}
		else
		{
			assert(node->l == NULL);
			node->l = new_node;
		}
#ifdef PATRICIA_DEBUG
		fprintf(stderr,
			"patricia_lookup: new_node #2 %s/%d (child)\n",
			prefix_toa(prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		return (new_node);
	}

	if(bitlen == differ_bit)
	{
		if(bitlen < patricia->maxbits &&
		   BIT_TEST(test_addr[bitlen >> 3], 0x80 >> (bitlen & 0x07)))
		{
			new_node->r = node;
		}
		else
		{
			new_node->l = node;
		}
		new_node->parent = node->parent;
		if(node->parent == NULL)
		{
			assert(patricia->head == node);
			patricia->head = new_node;
		}
		else if(node->parent->r == node)
		{
			node->parent->r = new_node;
		}
		else
		{
			node->parent->l = new_node;
		}
		node->parent = new_node;
#ifdef PATRICIA_DEBUG
		fprintf(stderr,
			"patricia_lookup: new_node #3 %s/%d (parent)\n",
			prefix_toa(prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
	}
	else
	{
		glue = rb_malloc(sizeof(rb_patricia_node_t));
		glue->bit = differ_bit;
		glue->prefix = NULL;
		glue->parent = node->parent;
		glue->data = NULL;
		patricia->num_active_node++;
		if(differ_bit < patricia->maxbits &&
		   BIT_TEST(addr[differ_bit >> 3], 0x80 >> (differ_bit & 0x07)))
		{
			glue->r = new_node;
			glue->l = node;
		}
		else
		{
			glue->r = node;
			glue->l = new_node;
		}
		new_node->parent = glue;

		if(node->parent == NULL)
		{
			assert(patricia->head == node);
			patricia->head = glue;
		}
		else if(node->parent->r == node)
		{
			node->parent->r = glue;
		}
		else
		{
			node->parent->l = glue;
		}
		node->parent = glue;
#ifdef PATRICIA_DEBUG
		fprintf(stderr,
			"patricia_lookup: new_node #4 %s/%d (glue+node)\n",
			prefix_toa(prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
	}
	return (new_node);
}


void
rb_patricia_remove(rb_patricia_tree_t *patricia, rb_patricia_node_t *node)
{
	rb_patricia_node_t *parent, *child;

	assert(patricia);
	assert(node);

	if(node->r && node->l)
	{
#ifdef PATRICIA_DEBUG
		fprintf(stderr, "patricia_remove: #0 %s/%d (r & l)\n",
			prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */

		/* this might be a placeholder node -- have to check and make sure
		 * there is a prefix aossciated with it ! */
		if(node->prefix != NULL)
			Deref_Prefix(node->prefix);
		node->prefix = NULL;
		/* Also I needed to clear data pointer -- masaki */
		node->data = NULL;
		return;
	}

	if(node->r == NULL && node->l == NULL)
	{
#ifdef PATRICIA_DEBUG
		fprintf(stderr, "patricia_remove: #1 %s/%d (!r & !l)\n",
			prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		parent = node->parent;
		Deref_Prefix(node->prefix);
		rb_free(node);
		patricia->num_active_node--;

		if(parent == NULL)
		{
			assert(patricia->head == node);
			patricia->head = NULL;
			return;
		}

		if(parent->r == node)
		{
			parent->r = NULL;
			child = parent->l;
		}
		else
		{
			assert(parent->l == node);
			parent->l = NULL;
			child = parent->r;
		}

		if(parent->prefix)
			return;

		/* we need to remove parent too */

		if(parent->parent == NULL)
		{
			assert(patricia->head == parent);
			patricia->head = child;
		}
		else if(parent->parent->r == parent)
		{
			parent->parent->r = child;
		}
		else
		{
			assert(parent->parent->l == parent);
			parent->parent->l = child;
		}
		child->parent = parent->parent;
		rb_free(parent);
		patricia->num_active_node--;
		return;
	}
#ifdef PATRICIA_DEBUG
	fprintf(stderr, "patricia_remove: #2 %s/%d (r ^ l)\n",
		prefix_toa(node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
	if(node->r)
	{
		child = node->r;
	}
	else
	{
		assert(node->l);
		child = node->l;
	}
	parent = node->parent;
	child->parent = parent;

	Deref_Prefix(node->prefix);
	rb_free(node);
	patricia->num_active_node--;

	if(parent == NULL)
	{
		assert(patricia->head == node);
		patricia->head = child;
		return;
	}

	if(parent->r == node)
	{
		parent->r = child;
	}
	else
	{
		assert(parent->l == node);
		parent->l = child;
	}
}

rb_patricia_node_t *
make_and_lookup_ip(rb_patricia_tree_t *tree, struct sockaddr *in, int bitlen)
{
	rb_prefix_t *prefix;
	rb_patricia_node_t *node;
	void *ipptr = NULL;
#ifdef RB_IPV6
	if(in->sa_family == AF_INET6)
		ipptr = &((struct sockaddr_in6 *)in)->sin6_addr;
	else
#endif
		ipptr = &((struct sockaddr_in *)in)->sin_addr;

	prefix = New_Prefix(in->sa_family, ipptr, bitlen);

	if(prefix == NULL)
		return NULL;

	node = rb_patricia_lookup(tree, prefix);



	Deref_Prefix(prefix);
	return (node);
}


rb_patricia_node_t *
make_and_lookup(rb_patricia_tree_t *tree, const char *string)
{
	rb_prefix_t *prefix;
	rb_patricia_node_t *node;

	if((prefix = ascii2prefix(AF_INET, string)) != NULL)
	{
		node = rb_patricia_lookup(tree, prefix);
	}
	else
#ifdef RB_IPV6
	if((prefix = ascii2prefix(AF_INET6, string)) != NULL)
	{
		node = rb_patricia_lookup(tree, prefix);
	}
	else
#endif
		return NULL;
#ifdef PATRICIA_DEBUG
	printf("make_and_lookup: %s/%d\n", prefix_toa(prefix), prefix->bitlen);
#endif
	Deref_Prefix(prefix);
	return (node);
}

#ifdef NOTYET
static rb_patricia_node_t *
try_search_exact(rb_patricia_tree_t *tree, char *string)
{
	rb_prefix_t *prefix;
	rb_patricia_node_t *node;
	if((prefix = ascii2prefix(AF_INET, string)) != NULL)
	{
		node = rb_patricia_search_exact(tree, prefix);
		Deref_Prefix(prefix);
		return (node);
	}
#ifdef RB_IPV6
	else if((prefix = ascii2prefix(AF_INET6, string)) != NULL)
	{
		node = rb_patricia_search_exact(tree, prefix);
		Deref_Prefix(prefix);
		return (node);
	}
#endif
	else
		return NULL;
}

void
lookup_then_remove(rb_patricia_tree_t *tree, char *string)
{
	rb_patricia_node_t *node;

	if((node = try_search_exact(tree, string)))
		patricia_remove(tree, node);
}
#endif

rb_patricia_node_t *
rb_match_ip(rb_patricia_tree_t *tree, struct sockaddr *ip)
{
	rb_prefix_t *prefix;
	rb_patricia_node_t *node;
	void *ipptr;
	unsigned int len;
	int family;
#ifndef RB_IPV6
	len = 32;
	family = AF_INET;
	ipptr = &((struct sockaddr_in *)ip)->sin_addr;
#else
	if(ip->sa_family == AF_INET6)
	{
		len = 128;
		family = AF_INET6;
		ipptr = &((struct sockaddr_in6 *)ip)->sin6_addr;
	}
	else
	{
		len = 32;
		family = AF_INET;
		ipptr = &((struct sockaddr_in *)ip)->sin_addr;
	}
#endif

	if((prefix = New_Prefix(family, ipptr, len)) != NULL)
	{
		node = rb_patricia_search_best(tree, prefix);
		Deref_Prefix(prefix);
		return (node);
	}
	return NULL;
}

rb_patricia_node_t *
rb_match_ip_exact(rb_patricia_tree_t *tree, struct sockaddr *ip, unsigned int len)
{
	rb_prefix_t *prefix;
	rb_patricia_node_t *node;
	void *ipptr;
	int family;
#ifndef RB_IPV6
	if(len > 128)
		len = 128;

	family = AF_INET;
	ipptr = &((struct sockaddr_in *)ip)->sin_addr;
#else
	if(ip->sa_family == AF_INET6)
	{
		if(len > 128)
			len = 128;
		family = AF_INET6;
		ipptr = &((struct sockaddr_in6 *)ip)->sin6_addr;
	}
	else
	{
		if(len > 32)
			len = 32;
		family = AF_INET;
		ipptr = &((struct sockaddr_in *)ip)->sin_addr;
	}
#endif

	if((prefix = New_Prefix(family, ipptr, len)) != NULL)
	{
		node = rb_patricia_search_exact(tree, prefix);
		Deref_Prefix(prefix);
		return (node);
	}
	return NULL;
}



rb_patricia_node_t *
rb_match_string(rb_patricia_tree_t *tree, const char *string)
{
	rb_patricia_node_t *node;
	rb_prefix_t *prefix;

	if((prefix = ascii2prefix(AF_INET, string)) != NULL)
	{
		node = rb_patricia_search_best(tree, prefix);
		Deref_Prefix(prefix);
	}
	else
#ifdef RB_IPV6
	if((prefix = ascii2prefix(AF_INET6, string)) != NULL)
	{
		node = rb_patricia_search_best(tree, prefix);
		Deref_Prefix(prefix);
	}
	else
#endif
		return NULL;
	return node;
}

rb_patricia_node_t *
rb_match_exact_string(rb_patricia_tree_t *tree, const char *string)
{
	rb_prefix_t *prefix;
	rb_patricia_node_t *node;
	if((prefix = ascii2prefix(AF_INET, string)) != NULL)
	{
		node = rb_patricia_search_exact(tree, prefix);
		Deref_Prefix(prefix);
	}
	else
#ifdef RB_IPV6
	if((prefix = ascii2prefix(AF_INET6, string)) != NULL)
	{
		node = rb_patricia_search_exact(tree, prefix);
		Deref_Prefix(prefix);
	}
	else
#endif
		return NULL;
	return node;
}
