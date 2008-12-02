/*
 * $Id: patricia.h 23020 2006-09-01 18:20:19Z androsyn $
 * Dave Plonka <plonka@doit.wisc.edu>
 *
 * This product includes software developed by the University of Michigan,
 * Merit Network, Inc., and their contributors. 
 *
 * This file had been called "radix.h" in the MRT sources.
 *
 * I renamed it to "patricia.h" since it's not an implementation of a general
 * radix trie.  Also, pulled in various requirements from "mrt.h" and added
 * some other things it could be used as a standalone API.
 */

#ifndef _RB_PATRICIA_H
#define _RB_PATRICIA_H

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !(FALSE)
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

/* typedef unsigned int u_int; */
#define rb_prefix_touchar(prefix) ((unsigned char *)&(prefix)->add.sin)
#define MAXLINE 1024
#define BIT_TEST(f, b)  ((f) & (b))

typedef struct _rb_prefix_t
{
	unsigned short family;	/* AF_INET | AF_INET6 */
	unsigned short bitlen;	/* same as mask? */
	int ref_count;		/* reference count */
	union
	{
		struct in_addr sin;
#ifdef RB_IPV6
		struct in6_addr sin6;
#endif				/* RB_IPV6 */
	}
	add;
}
rb_prefix_t;


typedef struct _rb_patricia_node_t
{
	unsigned int bit;	/* flag if this node used */
	rb_prefix_t *prefix;	/* who we are in patricia tree */
	struct _rb_patricia_node_t *l, *r;	/* left and right children */
	struct _rb_patricia_node_t *parent;	/* may be used */
	void *data;
}
rb_patricia_node_t;

typedef struct _rb_patricia_tree_t
{
	rb_patricia_node_t *head;
	unsigned int maxbits;	/* for IP, 32 bit addresses */
	int num_active_node;	/* for debug purpose */
}
rb_patricia_tree_t;


rb_patricia_node_t *rb_match_ip(rb_patricia_tree_t *tree, struct sockaddr *ip);
rb_patricia_node_t *rb_match_ip_exact(rb_patricia_tree_t *tree, struct sockaddr *ip,
				      unsigned int len);
rb_patricia_node_t *rb_match_string(rb_patricia_tree_t *tree, const char *string);
rb_patricia_node_t *rb_match_exact_string(rb_patricia_tree_t *tree, const char *string);
rb_patricia_node_t *rb_patricia_search_exact(rb_patricia_tree_t *patricia, rb_prefix_t *prefix);
rb_patricia_node_t *rb_patricia_search_best(rb_patricia_tree_t *patricia, rb_prefix_t *prefix);
rb_patricia_node_t *rb_patricia_search_best2(rb_patricia_tree_t *patricia,
					     rb_prefix_t *prefix, int inclusive);
rb_patricia_node_t *rb_patricia_lookup(rb_patricia_tree_t *patricia, rb_prefix_t *prefix);

void rb_patricia_remove(rb_patricia_tree_t *patricia, rb_patricia_node_t *node);
rb_patricia_tree_t *rb_new_patricia(int maxbits);
void rb_clear_patricia(rb_patricia_tree_t *patricia, void (*func) (void *));
void rb_destroy_patricia(rb_patricia_tree_t *patricia, void (*func) (void *));
void rb_patricia_process(rb_patricia_tree_t *patricia, void (*func) (rb_prefix_t *, void *));
void rb_init_patricia(void);


#if 0
rb_prefix_t *ascii2prefix(int family, char *string);
#endif
rb_patricia_node_t *make_and_lookup(rb_patricia_tree_t *tree, const char *string);
rb_patricia_node_t *make_and_lookup_ip(rb_patricia_tree_t *tree, struct sockaddr *, int bitlen);


#define RB_PATRICIA_MAXBITS 128
#define RB_PATRICIA_NBIT(x)        (0x80 >> ((x) & 0x7f))
#define RB_PATRICIA_NBYTE(x)       ((x) >> 3)

#define RB_PATRICIA_DATA_GET(node, type) (type *)((node)->data)
#define RB_PATRICIA_DATA_SET(node, value) ((node)->data = (void *)(value))

#define RB_PATRICIA_WALK(Xhead, Xnode) \
    do { \
	rb_patricia_node_t *Xstack[RB_PATRICIA_MAXBITS+1]; \
	rb_patricia_node_t **Xsp = Xstack; \
	rb_patricia_node_t *Xrn = (Xhead); \
	while ((Xnode = Xrn)) { \
	    if (Xnode->prefix)

#define RB_PATRICIA_WALK_ALL(Xhead, Xnode) \
do { \
	rb_patricia_node_t *Xstack[RB_PATRICIA_MAXBITS+1]; \
	rb_patricia_node_t **Xsp = Xstack; \
	rb_patricia_node_t *Xrn = (Xhead); \
	while ((Xnode = Xrn)) { \
	    if (1)

#define RB_PATRICIA_WALK_BREAK { \
	    if (Xsp != Xstack) { \
		Xrn = *(--Xsp); \
	     } else { \
		Xrn = (rb_patricia_node_t *) 0; \
	    } \
	    continue; }

#define RB_PATRICIA_WALK_END \
	    if (Xrn->l) { \
		if (Xrn->r) { \
		    *Xsp++ = Xrn->r; \
		} \
		Xrn = Xrn->l; \
	    } else if (Xrn->r) { \
		Xrn = Xrn->r; \
	    } else if (Xsp != Xstack) { \
		Xrn = *(--Xsp); \
	    } else { \
		Xrn = (rb_patricia_node_t *) 0; \
	    } \
	} \
    } while (0)

#endif /* _RB_PATRICIA_H */
