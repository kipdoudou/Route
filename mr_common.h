#ifndef _MR_COMMON_H
#define _MR_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ioctl.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/sendfile.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <assert.h>
#include <pthread.h>
#include "malloc.h"

/* conditional compiltion */
#define _ERR_PRINT
#ifdef _ERR_PRINT
#define EPT			fprintf
#define ASSERT		assert
#else
#define EPT
#define ASSERT
#endif

/* used data types */
#define U8			unsigned char
#define U16			unsigned short
#define U32			unsigned int

/* determain address length of manet node */
//#define _MADR_32BIT 
//#define _MADR_16BIT 

#if _32BIT_ADDR
#define MADR		U32
#else 
#if _16BIT_ADDR
#define MADR		U16
#else
#define MADR		U8
#endif
#endif

/** very important **/
//#define _MR_TEST						/* for routing protocol test(routing), this macro must be turned on */

/* netlayer constants  */
#define MAX_NODE_CNT		32			/* maximum number of network nodes */
#define MAX_HOPS			10			/* maximum hops */
#define MADR_UNI_MIN		1
#define MADR_UNI_MAX		MAX_NODE_CNT
#define MADR_INVALID		0
#define MADR_BRDCAST		((MADR)0xFFFFFFFF)

#define MR_ISUNI(n)			(n >= MADR_UNI_MIN && n <= MADR_UNI_MAX)

/* mac layer contants */
#define MAX_SLTS_PCF		64			/* slot number per complex frame */
#define MAX_FSLS_PCF		8			/* fixed slot number of a complex frame */
#define MAX_DSLS_PCF		(MAX_SLTS_PCF - MAX_FSLS_PCF - 1)
										/* dynamic slot number of a complex frame */
#define MAX_CFS_PSF			32			/* complex frames per super frame */
#define CF_SBS_SPT			0
#define CF_FLT_SPT			(CF_SBS_SPT + 1)
#define CF_DLT_SPT			(CF_FLT_SPT + 1)

//added by wanghao5 on 3.2
typedef unsigned short int uint16;
#define BigLittleSwap16(A)  ((((uint16)(A) & 0xff00) >> 8) | (((uint16)(A) & 0x00ff) << 8))


/* constants related to creating message queues */
#define PATH_CREATE_KEY		"/etc/profile"	/* path name for creating all queue, can be modified */
#define SN_NETPROC			1					/* sub number of creating or getting the queue id of netlayer process */
#define SN_HIGHMAC			2					/* sub number of creating or getting the queue id of highmac process */
#define SN_IF2TCPIP			3					/* sub number of creating or getting the queue id of if2tcpip process */
#define SN_GUI_APP			4					/* sub number of creating or getting the queue id of GUI process */
#define SN_VOICE_APP		5					/* sub number of creating or getting the queue id of voice process */
#define SN_VIDEO_APP		6					/* sub number of creating or getting the queue id of video process */
#define SN_ROUTINGP			7					/* sub number of creating or getting the queue id of routing process */
#define SN_MNCONF			8					/* sub number of creating or getting the queue id of mnconf process */
#define SN_MAODV			9
#define SN_MRTEST			100					/* sub number of creating or getting the queue id of routing process */
/* constants related to creating shared memory */
#define SM_FWT				20

/* all message types used in MANET routing software */
#define MSG_NETLAY_BASE		10					/* all the types of messages received at neylayer process */
#define MSG_HIGHMAC_BASE	100					/* all the types of messages received at highmac process */
#define MSG_IF2TCPIP_BASE	200					/* all the types of messages received at if2tcpip process */
#define MSG_ROUTINGP_BASE	300					/* all the types of messages received at routingp process */
#define MSG_MAODV_BASE		400					/* all the types of messages received at maodv process */
#define MSG_GAVAPP_BASE		500					/* all the types of messages received at gui, audio and video process */
#define MSG_GENNERAL_BASE	600					/* all the types of messages received at all process */
#define MSG_MRTEST_BASE		1000				/* all the types of messages related mr_test process */
#define MSG_MNCONF_BASE		2000				/* all the types of messages originate from mnconf process */

typedef enum _MR_MSG_TYPE {
	MMSG_NULL = 0,
	MMSG_TEST = 1,
	MMSG_SELF = 2,								/* just wake the blocking thread */
	/* netlayer process */
	MMSG_EF_DATA = MSG_NETLAY_BASE,				/* ethernet frame */
	MMSG_IP_DATA,								/* ip packet */
	MMSG_IPC_DATA,								/* ip packet with header compression */	
	MMSG_RPM,									/* unicast routing protocol message */
	MMSG_RP_FT_DATA,							/* set forarding table from routingp */
	MMSG_MRPM,									/* multicate routing protocol message */
	MMSG_HM_DATA,								/* data packet received from highmac*/
	MMSG_FT_REP,								/* inquired forwarding table data from low mac */
	MMSG_URT_DATA,								/* unicast route table */
	MMSG_MRT_DATA,								/* multicast route table */
	/* highmac process */
	MMSG_MP_DATA = MSG_HIGHMAC_BASE,			/* manet data packet */
	MMSG_FT_DATA,								/* set forarding table from netlayer */
	MMSG_FT_REQ,								/* request fwt from netlayer */
	MMSG_BR_CFG,								/* BB/RF config data */
	MMSG_BR_REQ,								/* request BB/RF state	*/
	MMSG_ST_DATA,								/* set slot table to low mac */
	MMSG_ST_REQ,								/* request slot table in low mac */
	MMSG_LM_REQ,								/* request lowmac state */
	MMSG_LM_STATE,								/* state info of low mac */
	/* if2tcpi process */
	MMSG_REF_DATA = MSG_IF2TCPIP_BASE,			/* received ethernet frame */
	MMSG_RIP_DATA,								/* received ip packet */
	MMSG_RIPC_DATA,								/* received ip packet with header compression */	
	/* routingp process */
	MMSG_URP_DATA = MSG_ROUTINGP_BASE,			/* received unicast protocol message */
	/* maodv process */
	MMSG_MRP_DATA = MSG_MAODV_BASE,				/* received multicast protocol message */
	MMSG_RT_DATA,								/* route table */
	/* gui\aurio\vidoe process */
	MMSG_RT_REQ = MSG_GAVAPP_BASE,				/* request route table from gui */
	MMSG_VO_DATA,								/* voice data */
	MMSG_VD_DATA,								/* video data */
	/* general message */
	MMSG_GM_REQVER = MSG_GENNERAL_BASE,			/* request version info */
	MMSG_GM_VERINF,								/* version infomation */
	/* mrtest msg id */
	MMSG_MT_RQID = MSG_MRTEST_BASE,				/* qid data of routingp process */
	MMSG_MT_MQID,								/* qid data of mac process */
	MMSG_MT_SIN,								/* mr_test send it to mac for indicate the start of a slot, mac prcoess should send data based on its slot table */
	MMSG_MT_MDT,								/* mac data send by mac process to mr_test  */
	MMSG_MN_REQUEST = MSG_MNCONF_BASE,			/* mnconf request */
	MMSG_MN_RESPSTR,							/* mnconf response, string fromat, print it */
	MMSG_MN_RESPDATA,							/* mnconf response, struct data, needing analysis */
	MMSG_SMS_TEST
} MR_MSG;

#define QUEUE_R				0400
#define QUEUE_W				0200
#define QUEUE_MODE      	(QUEUE_R | QUEUE_W | QUEUE_W>>3 | QUEUE_W>>6) 

/* common constants, names of the processes in manet software */
#define PNAME_NETLAYER		"netlayer"
#define PNAME_ROUTINGP		"routingp"
#define PNAME_IF2TCPIP		"if2tcpip"
#define PNAME_HIGHMAC		"highmac"
#define PNAME_MR_TEST		"mrtest"
#define PNAME_MAODV		    "maodv"
#define PNAME_MNCONF		"mnconf"

typedef struct _qinfo_t {
	char  	pname[64];
	int   	sub;
	key_t 	key_q;
	int   	qid;
} qinfo_t;

typedef struct _tproc_t {
	char	name[64];		/* name of the timer */
	U32		wait;			/* time for waiting next periodical timer */
	U32		period;			/* timer interval */
	void	(*pf)(void*);	/* pointer to the executiove function when wait = 0 */
} tproc_t;

typedef struct _tsche_t {
	U32		tmap;			/* timer map for support multiple timer function */
	U32		tmask;			/* timer mask for disabling certain timer function 1: enbale */
	tproc_t	procs[32];		/* the maximum number of the functions is 32 */
//	tproc_t	procs[2];		/* for test */
} tsche_t;

#define MAX_DATA_LENGTH		2048
typedef struct _mmsg_t {
	long  mtype;
	MADR  node;				/* differnet meaning dependent on mtype */
	char  data[MAX_DATA_LENGTH];
} mmsg_t;

/* header of data in mmsg_t  struct, this header can tell the function how to process the data in mmsg_t */

typedef struct _mmhd_t {
	U16		type;			// sub type 
	U16		len;			// len of the data in mmsg_t except mmhd_t 
} mmhd_t;
#define MMHD_LEN			sizeof(mmhd_t)

#define MMSG_FIXLEN			(sizeof(MADR)+MMHD_LEN)

/* item of forwarding table */
typedef struct _fwi_t {
	MADR	dest;
	MADR	fnd;
	MADR	snd;
} fwi_t;

/* forwading table */
typedef struct _fwt_t {
	MADR	self;
	fwi_t	ft[MAX_NODE_CNT];
} fwt_t;

//组播网络中节点的类型，从上到下优先级递减，优先级高的类型能够覆盖优先级低的类型
typedef enum
{
	LEADER = 1,
	MEMBER,
	ROUTE,
	OTHER
} node_type;

//组播路由表共享内存
typedef struct maodv_i_shm
{
    MADR m_addr;        //组播地址
    MADR l_addr;        //组长节点
    int hop;     		//到组长跳数
    node_type Ntype;    //本节点类型，MACT添加的设为ROUTE ，GRPH添加的设为OTHER。
						//说明，节点收到RREP时不添加路由表项，收到MACT才添加，MACT携带路径
    MADR up_node;       //上游节点
    int low_num;		//下游节点总数
	MADR low_node[MAX_NODE_CNT];	//下游节点

}m_item_shm;

#define MAX_MAODV_ITEM 10

typedef struct maodv_t_shm
{
    int timeout;
    m_item_shm item[MAX_MAODV_ITEM];
}m_table_shm;

void  mr_queues_init(void *arg);
int   mr_queues_delete();

extern int qs, re_qin, nl_qid, hm_qid, vi_qid, rp_qid, ma_qid;    //Õâ¼¸¸öÏûÏ¢¶ÓÁÐµÄidÉùÃ÷ÎªÈ«¾Ö
extern qinfo_t qinfs[];									  //Ã¿¸öÏûÏ¢¶ÓÁÐµÄinfo
extern const int cnt_p;									  //ÏûÏ¢¶ÓÁÐµÄ¸öÊý

/* the common struct corrsponding monconf */
typedef struct _cmd_t {
	char			cmd[32];				/* name of the command */
	int				cnt;					/* size of the sub command array */
	struct _cmd_t	*sub;					/* array of sub command */
	void			(*pfunc) (int, char *); /* processing function pointer */
} cmd_t;


#if 0
#define DE_BUG

extern void sys_info(const char *msg);
extern void sys_err(const char *msg);
extern void sys_exit(const char *msg);
extern void sys_debug(const char *msg);
#endif
#endif

