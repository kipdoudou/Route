#ifndef _RP_COMMON_H
#define _RP_COMMON_H

/* struct for the data sharing among thread */
typedef struct _rp_tshare_t {
	pthread_mutex_t mutex;
	pthread_cond_t  cond;
//	int qr_run;
//	int gi_run;
} rp_tshare_t;

typedef enum {
	IS_NULL = 0,
	IS_EXPIRE,			/* expired */
	IS_UNSTABLE,		/* the link to the next hop is unstable */
	IS_ACTIVE			/* active */
} istat_t;

typedef enum {
	LQ_NULL = 0,		/* no link to a node */
	LQ_EXPIRE,			/* expired link to a node */
	LQ_UNSTABLE,		/* unstable link to a node */
	LQ_ACTIVE			/* active link to a node  */
} lstat_t;

typedef struct _rpath_t {
	U8		hop;		/* 0 means invalid */
	MADR	node[MAX_HOPS];
	istat_t	status;		/* status */
	int		flag;		/* 0: no sop  received */
} rpath_t;

typedef struct _ritem_t {
	MADR	dest;		/* dets node */
	rpath_t	pfst;		/* primary path */
	rpath_t	psnd;		/* second path */
} ritem_t;

/* since MADR = 0 mean invalid addess, so the node index should be (MADR-1) */
typedef struct _rtable_t {
	MADR	self;
	ritem_t	item[MAX_NODE_CNT];
} rtable_t;

/*uibp message header*/
typedef struct _uibp_hd{
	MADR node;
	U8 icnt;
}uibp_hd;

/*ul record struct*/
typedef struct _ul_record{
	MADR src;
	MADR dst;
	U8 status;
	U8 node_cnt;
	U8 node[MAX_HOPS];
}ul_record;

/* link state maintainence */
#define LM_NUL2NUL			0
#define LM_NUL2UNS			1
#define LM_NUL2ACT			5
#define	LM_ACT2ACT			4
#define	LM_ACT2UNS			1
#define LM_ACT2EXP			0
#define LM_UNS2ACT			4
#define LM_UNS2UNS			1
#define LM_UNS2EXP			0
#define LM_EXP2ACT			5
#define LM_EXP2UNS			1
#define LM_EXP2NUL			0

/* routing item maintainence*/
#define IM_NUL2ACT			1
#define IM_EXP2ACT			1
#define IM_ACT2EXP			1
#define IM_EXP2NUL			1

typedef struct _rlink_t {
	int 	rcnt; 		/* number of receive packets in link check interval */
	lstat_t	lstatus;	/* current status */
	lstat_t lold;		/* old status */
} rlink_t;

typedef struct _ntable_t {
	rlink_t	fl[MAX_NODE_CNT];	/* output link */
	rlink_t rl[MAX_NODE_CNT];	/* input link */
} ntable_t;

typedef struct _uni_link_t{
	U8 flag;
	int ctime;
	MADR src;
	MADR dst;
	U8 status;
	U8 timer_ttl;
}uni_link_t;


/* routing protocol message type is defined here */
#define RPM_ERROR			0
/* FHR */
#define RPM_FHR_SOP 		1
#define RPM_FHR_UIP			2
#define RPM_FHR_UIBP		3
#define RPM_FHR_ULACK		4
#define RPM_FHR_RII			5
#define RPM_FHR_RIR			6
/* other protocols */

/* end of defining rp message*/

#if _32BIT_ADDR
#define MR_IN2AD(n)			(n+1)
#define MR_AD2IN(n)			(n-1)
#else
#if  _16BIT_ADDR
#define MR_IN2AD(n)			(n+1)
#define MR_AD2IN(n)			(n-1)
#else
#define MR_IN2AD(n)			(n+1)
#define MR_AD2IN(n)			(n-1)
#endif
#endif

/* macro for efficiency */
#define WH_NL_FEAS(s)		(LQ_ACTIVE == s || LQ_UNSTABLE == s)
#define WH_RP_VALD(s)		(IS_ACTIVE == s || IS_UNSTABLE == s)

#define RP_INHOPS			0xFF
#define RP_UNSMASK			0x80

int	 rpath_comp(rpath_t*, rpath_t*, int*);
int  rpath_cklp(MADR, U8, MADR*);
int  rpath_ckjt(rpath_t*, rpath_t*);
void rpath_set(rpath_t*, int, MADR*, istat_t, int);
void rpath_clear(rpath_t*);
void rpath_copy(rpath_t*, rpath_t*);
int  rpath_fsm(rpath_t*, int);
int  rpath_up(MADR, rpath_t*, rpath_t*, int*);
void rpath_show(MADR, rpath_t*);

void ritem_clear(int, ritem_t*);
int  ritem_sopget(ritem_t*, U8*, int);
void ritem_nup(MADR, U8*, int);
void ritem_up(ritem_t*, MADR, U8, MADR*);
void ritem_del(ritem_t*, MADR);
void ritem_show(ritem_t*);
void ritem_fsm(ritem_t*, int);
int  rp_init(int);

void rt_show();
void nt_show();

void rlink_inc(MADR);
void rlink_dec(MADR);
int  rlink_fsm(MADR, int);
void rlink_clear(rlink_t*);

void update_fl(MADR dst, U8 status);


int  rp_rpm_proc(MADR, int, void*);
void*rp_qrv_thread(void*);

int  rp_tmsg_2nl(int, mmsg_t*);

void update_fwt();

void signal_show(int signal);

int find_useful_uni_link(MADR src, MADR dst, U8 status, int *sn);
int need_send2other(int id, U8 status);
void confirm_ul(U8 src, U8 status);
void clear_uni_link(int id);
#endif
