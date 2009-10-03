#ifndef FILE_NODE_H
#define FILE_NODE_H


/***** dependency graph *******/

#include <time.h>
#include "tree.h"

/**/
struct NODELINK
{
	struct NODE *node;
	struct NODELINK *next;
};

struct SCANNER
{
	struct SCANNER *next;
	int (*scannerfunc)(struct NODE *, struct SCANNER *info);
};

/*
	a node in the dependency graph
	NOTE: when adding variables to this structure, remember to add
		initilization of those variables in node_create().
	TODO: these should be allocated cache aligned, and padded to 64 byte?
*/
struct NODE
{
	/* *** */
	struct GRAPH *graph; /* graph that the node belongs to */
	struct NODE *next; /* next node in the graph */
	struct NODELINK *firstdep; /* list of dependencies */
	struct NODELINK *firstparent; /* list of parents */
	
	char *filename; /* this contains the filename with the FULLPATH */
	
	char *cmdline; /* command line that should be executed to build this node */
	char *label; /* what to print when we build this node */
	
	RB_ENTRY(NODE) rbentry; /* RB-tree entry sorted by hashid */

	/* filename and the tool to build the resource */
	/* unsigned filename_len; */ /* including zero term */
	unsigned cmdhash; /* hash of the command line for detecting changes */
	 
	unsigned hashid; /* hash of the filename/nodename */
	
	time_t timestamp; /* timestamp of the node, 0 == does not exist */
	unsigned id; /* used when doing traversal with marking (bitarray) */
	
	unsigned short filename_len; /* length of filename including zero term */
	unsigned short depth;	/* depth in the graph. used for priority when buliding */
	
	/* various flags (4 bytes in the end) */
	unsigned dirty:8; /* non-zero if the node has to be rebuilt */
	unsigned depchecked:1; /* set if a dependency checker have processed the file */
	unsigned cached:1;
	unsigned parenthastool:1; /* set if a parent has a tool */
	unsigned counted:1;
	unsigned isdependedon:1; /* set if someone depends on this node */
	
	volatile unsigned workstatus:2; /* build status of the node, NODESTATUS_* flags */
};

RB_HEAD(NODERB, NODE);

/* cache node */
struct CACHENODE
{
	RB_ENTRY(CACHENODE) rbentry;

	unsigned hashid;
	time_t timestamp;
	char *filename;
	
	unsigned cmdhash;
	
	unsigned deps_num;
	unsigned *deps; /* index id, not hashid */
};

/* */
struct GRAPH
{
	struct NODERB nodehash[0x10000];
	struct NODE *first;
	struct NODE *last;
	struct HEAP *heap;
	
	/* needed when saving the cache */
	int num_nodes;
	int num_deps;
};

struct HEAP;
struct CONTEXT;

/* node status */
#define NODESTATUS_UNDONE 0   /* node needs build */
#define NODESTATUS_WORKING 1  /* a thread is working on this node */
#define NODESTATUS_DONE 2     /* node built successfully */
#define NODESTATUS_BROKEN 3   /* node tool reported an error or a dependency is broken */

/* node creation error codes */
#define NODECREATE_OK 0
#define NODECREATE_EXISTS 1  /* the node already exists */
#define NODECREATE_NOTNICE 2 /* the path is not normalized */

/* node walk flags */
#define NODEWALK_FORCE 1    /* skips dirty checks and*/
#define NODEWALK_TOPDOWN 2  /* callbacks are done top down */
#define NODEWALK_BOTTOMUP 4 /* callbacks are done bottom up */
#define NODEWALK_UNDONE 8   /* causes checking of the undone flag, does not decend if it's set */
#define NODEWALK_QUICK 16   /* never visit the same node twice */
#define NODEWALK_REVISIT (32|NODEWALK_QUICK) /* will do a quick pass and revisits all nodes thats
	have been marked by node_walk_revisit(). path info won't be available when revisiting nodes */

/* node dirty status */
/* make sure to update node_debug_dump_jobs() when changing these */
#define NODEDIRTY_NOT 0
#define NODEDIRTY_MISSING 1     /* the output file is missing */
#define NODEDIRTY_CMDHASH 2     /* the command doesn't match the one in the cache */
#define NODEDIRTY_DEPDIRTY 3    /* one of the dependencies is dirty */
#define NODEDIRTY_DEPNEWER 4    /* one of the dependencies is newer */
#define NODEDIRTY_GLOBALSTAMP 5 /* the globaltimestamp is newer */


/* you destroy graphs by destroying the heap */
struct GRAPH *node_create_graph(struct HEAP *heap);
struct HEAP *node_graph_heap(struct GRAPH *graph);

/* */
int node_create(struct NODE **node, struct GRAPH *graph, const char *filename, const char *label, const char *cmdline);
struct NODE *node_find(struct GRAPH *graph, const char *filename);
struct NODE *node_add_dependency(struct NODE *node, const char *filename);
struct NODE *node_add_dependency_withnode(struct NODE *node, struct NODE *depnode);

struct NODEWALKPATH
{
	struct NODEWALKPATH *parent;
	struct NODE *node;
};

struct NODEWALKREVISIT
{
	struct NODE *node;
	struct NODEWALKREVISIT *next;
};

struct NODEWALK
{
	int flags; /* flags for this node walk */
	struct NODE *node; /* current visiting node */

	/* path that we reached this node by (not available during revisit due to activation) */
	struct NODEWALKPATH *parent;
	unsigned depth;
	
	void *user;
	int (*callback)(struct NODEWALK *); /* function that is called for each visited node */
	
	unsigned char *mark; /* bits for mark and sweep */

	int revisiting; /* set to 1 if we are doing revisits */
	struct NODEWALKREVISIT *firstrevisit;
	struct NODEWALKREVISIT *revisits;
};

/* walks though the dependency tree with the set options and calling callback()
	on each node it visites */
int node_walk(
	struct NODE *node,
	int flags,
	int (*callback)(struct NODEWALK *info),
	void *u);

/* marks a node for revisit, only works if NODEWALK_REVISIT flags
	was specified to node_walk */
void node_walk_revisit(struct NODEWALK *walk, struct NODE *node);

/* node debug dump functions */
void node_debug_dump(struct GRAPH *graph);
void node_debug_dump_jobs(struct GRAPH *graph);
void node_debug_dump_tree(struct NODE *root);

#endif /* FILE_NODE_H */
