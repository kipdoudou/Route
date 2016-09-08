#include "mr_common.h"
#include "rp_common.h"
#include <sys/shm.h>

#ifdef _MR_TEST
#pragma message("mr_test enbale, only routingp can turn on this MACRO.")
#endif

extern qinfo_t qinfs[];
extern const int cnt_p;

extern int  qs, re_qin, nl_qid, hm_qid, vi_qid, rp_qid, mt_qid;

static rp_tshare_t  share = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_COND_INITIALIZER
};

static pthread_t fq_tid = -1;
static pthread_t mrx_tid = -1;

/* routing table, neighbour table */
rtable_t	rt;
ntable_t	nt;
MADR		*sa = &rt.self;

uni_link_t uni_link[MAX_UNILINK_NUM];

static int		ft_sid = -1;

static fwt_t *pft = NULL;
//该数组用于发送简化版的转发表
char fwt_send[MAX_NODE_CNT];

int main(int argc, char* argv[])
{

	int len;
	int j;
	int rval, stop;
	void *result = NULL;

	EPT(stderr, "%s: main thread id = %ld\n", argv[0], pthread_self());
//	EPT(stderr, "sizeof(MADR) = %d\n", sizeof(MADR));

	if (argc < 2)
	{
		EPT(stderr, "run %s: must privide the address of itself node\n", argv[0]);
		rval = 1;
		goto process_return;
	}
	else
	{
		j = atoi(argv[1]);
	}

	//mr_queues_init("routingp");

	if (j >= MADR_UNI_MIN && j <= MADR_UNI_MAX)
	{
		/* initialize routing table, neighbour table, and so on */
		rp_init(j);
	}
	else
	{
		rval = 2;
		EPT(stderr, "run %s: must privide the correct node address\n", argv[0]);
		goto process_return;
	}

	/* this thread obtain all queues for rx and tx */
#ifdef _MR_TEST
	rval = pthread_create(&fq_tid, NULL, mr_queues_init, sa);
#else
	rval = pthread_create(&fq_tid, NULL, mr_queues_init, argv[0]);
#endif

	if (rval != 0)
	{
		EPT(stderr, "process %s: create get queue thread fails", argv[0]);
		rval = 3;
		goto process_return;
	}
	while (qs == 0)
	{
		sleep(1);
	}

#ifdef _MR_TEST
	/* send self qid to mr_test process */
	mmsg_t tx_msg;
	tx_msg.mtype = MMSG_MT_RQID;
	tx_msg.node = *sa;
	len = sizeof(tx_msg.node);

	*(int *)tx_msg.data = rp_qid;
	len += sizeof(int);
	msgsnd(mt_qid, (void *)&tx_msg, len, 0);
#endif
	/* get shared memory id */
	//key_t shm_k = qinfs[re_qin].key_q;
	key_t shm_k = ftok(PATH_CREATE_KEY, 10010);
    ft_sid = shmget(shm_k, sizeof(fwt_t), 0640|IPC_CREAT);
    if(ft_sid == -1)
    {
        EPT(stderr, "routingp: shmget error\n");
		rval = 2;
        goto process_return;
    }

    void	*shmaddr = (void*) -1;
    shmaddr = shmat(ft_sid, NULL, 0);
    if (shmaddr == (void*)-1 )
    {
		EPT(stderr, "routingp: can not attach shared memory.\n");
		rval = 2;
        goto process_return;
	}

    pft = (fwt_t *)shmaddr;
	memset(pft,0,sizeof(pft));
	//初始化转发表
	pft->self = j;
    int i;
    for(i = 0; i < MAX_NODE_CNT; i++)
    {
        pft->ft[i].dest = i+1;
    }

	/* start timer */
	rval = rp_start_timer();
	if (0 != rval) {
		EPT(stderr, "%s: can not open start timer function\n", argv[0]);
		rval = 4;
		goto process_return;
	}

	/* create receiving msg thread */
	rval = pthread_create(&mrx_tid, NULL, rp_qrv_thread, &(qinfs[re_qin].qid));
	if (rval != 0) {
		EPT(stderr, "%s: can not open create msg receiving thread\n", argv[0]);
		rval = 3;
		goto process_return;
	}

	//接受外部信号打印路由表
	signal(SIGUSR1, signal_show);

	stop = 0;
	pthread_mutex_lock(&share.mutex);
	while(0 == stop)
	{
		pthread_cond_wait(&share.cond, &share.mutex);

		/* do something */
	}
//	EPT(stderr, "%s: certain thread quit\n", argv[0]);
	pthread_mutex_unlock(&share.mutex);

process_return:
	sleep(1);

    if((pft != (void*)-1)&&(shmdt(pft) == -1))
		EPT(stderr, "routingp: detach shared memory error\n");
    if ((ft_sid != -1)&&(shmctl(ft_sid, IPC_RMID, NULL)) == -1)
		EPT(stderr, "routingp: delete shared memory error\n");

	mr_queues_delete();
	exit(rval);
}

void* rp_qrv_thread(void *arg)
{
	int qid, rcnt;
	mmsg_t rx_msg;
	int rval, stop;

	pthread_detach(pthread_self());

	qid = *(int *)arg;
	ASSERT(qinfs[re_qin].qid == qid);
	EPT(stderr, "%s: msg receiving thread id = %ld\n", qinfs[re_qin].pname, pthread_self());
//	EPT(stderr, "%s: enter queue receiving thread, rqueue %d.\n", qinfs[re_qin].pname, qid);

	if (qid < 0) {
		EPT(stdout, "%s: wrong receive queue id %d", qinfs[re_qin].pname, qid);
		rval = 1;
		goto thread_return;
	}

	rval = 0;
	stop = 0;
	while(0 == stop) {
		memset(&rx_msg, 0, sizeof(rx_msg));
		rcnt = msgrcv(qid, &rx_msg, MAX_DATA_LENGTH, 0, 0);
//		EPT(stdout, "%s: reveive msg queue at qid %d\n", qinfs[re_qin].pname, qid);
		if (rcnt < 0) {
			if (EIDRM != errno)
			{
				EPT(stderr, "%s: error in receiving msg, no:%d, meaning:%s\n", qinfs[re_qin].pname, errno, strerror(errno));
			}
			else
			{
				EPT(stderr, "%s: quit msg receiving thread\n", qinfs[re_qin].pname);
			}
			rval = 2;
			break;
		}

		if (rcnt < MMSG_FIXLEN) {
			//EPT(stderr, "%s: the len of rx_msg  = %d\n", qinfs[re_qin].pname, rcnt);
			continue;
		}

		switch(rx_msg.mtype) {
			case MMSG_URP_DATA:
				rval = rp_rpm_proc(rx_msg.node, rcnt - sizeof(MADR), &rx_msg.data);
				if (rval != 0) {
					/* report error */
					EPT(stderr, "! rp_rpm_proc process error\n");
				}
				break;

			default:
				/* report error */
				EPT(stderr, "! %s: the mtype of rx_msg  = %ld\n", qinfs[re_qin].pname, rx_msg.mtype);
				break;
		}
	}

thread_return:
/*	pthread_mutex_lock(&share.mutex);
	share.qr_run = 0;
	pthread_cond_signal(&share.cond);
	pthread_mutex_unlock(&share.mutex);
*/
	sleep(1);
	pthread_exit((void *)&rval);
}
//收到数据后调用该函数,解析消息队列data部分的头部
//这里的len是消息队列data部分长度，第三个参数是data部分起始地址
int rp_rpm_proc(MADR node, int len, void *data)
{
	mmhd_t *pmhd;
	int pos = 0;

	/* the code here is related to rp message definition */
	pmhd = (mmhd_t*)data;
	pos += MMHD_LEN;

//	EPT(stdout, "node[%d]: reveive rp message, type=%ld\n", *sa, mt);
	switch(pmhd->type)
	{
		case RPM_FHR_SOP:
		case RPM_FHR_RII:
		case RPM_FHR_RIR:
			rp_fhrmsg_disp(node, pmhd->type, pmhd->len, data + pos);
			break;
		case RPM_FHR_UIP:
			rp_fhruip_proc(node, pmhd->len, data + pos);
			break;
		case RPM_FHR_UIBP:
			rp_fhruibp_proc(node, pmhd->len, data + pos);
			break;
		case RPM_FHR_ULACK:
			rp_fhrulack_proc(node, pmhd->len, data + pos);
			break;

		default:
			/* report errors */
			EPT(stdout, "node[%d]: reveive unkown rp message, type=%d\n", *sa, pmhd->type);
			break;
	}

	return 0;
}
//路由路径清零，状态置为IS_NULL
void rpath_clear(rpath_t *prp)
{
	prp->hop = 0;
	memset(prp->node, 0, sizeof(prp->node));
	prp->status = IS_NULL;
	prp->flag = 0;
}
//刷新并且发送转发表
//分别再收到sop包和计时器周期性检查路由时调用
void update_fwt()
{
    int change = 0;
    int i;
    for(i=0;i<MAX_NODE_CNT;i++)
    {
        if(pft->ft[i].fnd != rt.item[i].pfst.node[0])
        {
            pft->ft[i].fnd = rt.item[i].pfst.node[0];
            //简化版的转发表，目的地址是i+1，下一跳地址是右值
            fwt_send[i] = rt.item[i].pfst.node[0];
            change =1;
        }
        if(pft->ft[i].snd != rt.item[i].psnd.node[0])
        {
            pft->ft[i].snd = rt.item[i].psnd.node[0];
            change =1;
        }
    }
    //printf("check fwt!!!,length of pft is %zu，lengthof fwt_send is %zu\n",sizeof(pft->ft),sizeof(fwt_send));

    if(1 == change)
    {
        mmsg_t msg_fwt;
        //消息队列类型为转发表数据
        msg_fwt.mtype = MMSG_RP_FT_DATA;
        //节点号设置为本节点，因为值通知本地的进程，不许要设置目的节点
        msg_fwt.node = pft->self;
        //memcpy(msg_fwt.data,pft->ft,sizeof(pft->ft));
        //rp_tmsg_2nl(sizeof(pft->ft) + sizeof(MADR), &msg_fwt);
        memcpy(msg_fwt.data,fwt_send,sizeof(char)*MAX_NODE_CNT);
        rp_tmsg_2nl(sizeof(char)*MAX_NODE_CNT + sizeof(MADR), &msg_fwt);
    }
}

void ritem_clear(int addr, ritem_t *pri)
{
	pri->dest = addr;
	rpath_clear(&pri->pfst);
	rpath_clear(&pri->psnd);
}

void rpath_copy(rpath_t* dest, rpath_t* src)
{
	dest->hop = src->hop;
	dest->flag = src->flag;
	dest->status = src->status;
	memset(dest->node, 0, sizeof(dest->node));
	memcpy(dest->node, src->node, src->hop*sizeof(MADR));
}
//为一条路由路径赋值第三个参数是路径经过节点数组的地址
void rpath_set(rpath_t* path, int hop, MADR* pn, istat_t status, int flag)
{
	path->hop = hop;
	memcpy(path->node, pn, hop*sizeof(MADR));
	path->status = status;
	path->flag = flag;
}

int rp_init(int myaddr)
{
	int i;

	rt.self = myaddr;
	for(i = 0; i < MAX_NODE_CNT; i++)
	{
		ritem_clear(MR_IN2AD(i), &rt.item[i]);
		rlink_clear(&nt.fl[i]);
		rlink_clear(&nt.rl[i]);
	}
}

//route to neighbour node
//赋值一条到src的路由并与原来到节点src的路由比较，若更优则更新之
void ritem_nup(MADR src, U8* pn, int hop)
{
	int id = MR_AD2IN(src);
	//指针变量nl，邻接点表中的一条链路
	rlink_t *nl = &nt.fl[id];

	ASSERT(NULL == pn && hop == 0);
	ASSERT(src = rt.item[id].dest);

	int sn;
	rpath_t path;
	//为到邻接点src的路由项path赋值，hop=1，下一跳节点是src，状态是nl->lstatus，flag=1（收到包）
	rpath_set(&path, 1, &src, nl->lstatus, 1);
	ritem_t *ri = &rt.item[id];
#if 0
	EPT(stderr, "node[%d]: enter to rapth_nup\n", *sa);
	rpath_show(ri->dest, &path);
	rpath_show(ri->dest, &ri->pfst);
	rpath_show(ri->dest, &ri->psnd);
#endif
    //rpath_up函数比较和更新到dest的两条路径，返回值为0说明path更优，ri->pfst被path替换
	if (0 == rpath_up(ri->dest, &path, &ri->pfst, &sn))
	{
	    //sn=0说明两条路径下一跳节点不同
		if (0 == sn)
		{
#if 1
			if (path.node[0] == ri->pfst.node[0])
			{
				EPT(stderr, "node[%d]: ritem_nup the same next hop error\n", *sa);
			}
#endif
			if (1 == rpath_up(ri->dest, &path, &ri->psnd, &sn))
			{
				ASSERT(0);
			}
		}
	}
	else
	{
		if (0 == sn)
			rpath_up(ri->dest, &path, &ri->psnd, &sn);
	}
	
	//changed by wanghao on 7.11
#if 1
	EPT(stderr, "");
	rpath_show(ri->dest, &ri->pfst);
	rpath_show(ri->dest, &ri->psnd);
#endif
}

/* route to nodes except neighbour */
void ritem_up(ritem_t *ri, MADR src, U8 hop, MADR* pn)
{
	if (hop == RP_INHOPS)
	{
		/* indicate that the src can not provide route to dest */
		EPT(stderr, "node[%d]: indicating no route to dest is processed in ritem_del()\n", *sa);
		return;
	}

	/* illegal parameters */
	if (hop > 0)
	{
		if ((NULL == pn || hop > MAX_HOPS))
		{
			EPT(stderr, "update routing item:illegal parameter\n");
			return;
		}
		else if (hop == MAX_HOPS)
		{
			EPT(stderr, "update routing item: too many hops\n");
			return;
		}
	}

	//下一跳地址或者目的地址是自己，则忽略，不更新
	if (pn[0] == *sa || ri->dest == *sa)
		return;

	/* loop in path */
	//若本节点sa是路径pn上节点，则说明更新后会存在环路，直接返回
	if (0 != rpath_cklp(*sa, hop, pn))
	{
		EPT(stderr, "node[%d]: find loop, dest=%d, src=%d, hop=%d, next=%d\n", *sa, ri->dest, src, hop, pn[0]);
		return;
	}
    //用sop包携带的item初始化一个path变量
	rpath_t path;
	int sn;
	path.node[0] = src;
	memcpy(&path.node[1], pn, sizeof(MADR)*hop);
	path.hop = hop + 1;
	path.status = (LQ_ACTIVE == nt.fl[MR_AD2IN(src)].lstatus)? IS_ACTIVE : IS_UNSTABLE;
	path.flag = 1;
    //比较path和pfst，若path路径更优，则将ri-->pfst变更为path
	if (0 == rpath_up(ri->dest, &path, &ri->pfst, &sn))
	{
	    //若下一跳节点不同
		if (0 == sn)
		{
#if 1
			if (path.node[0] == ri->pfst.node[0])
				EPT(stderr, "node[%d], ritem_up the same next hop error\n", *sa);
#endif
			if (1 == rpath_up(ri->dest, &path, &ri->psnd, &sn))
			{
				ASSERT(0);
			}
		}
	}
	//若path路径劣于已存在优先路径，则将path与次级路径比较更新
	else
	{
		if (0 == sn)
			rpath_up(ri->dest, &path, &ri->psnd, &sn);
	}

}

//第一个参数是路由item，第二个参数是携带此item的通知节点
//再收到sop包，发现达到最大跳数才调用执行次函数？
void ritem_del(ritem_t *ri, MADR src)
{
	int sn;
	rpath_t *path;

	EPT(stderr, "node[%d]: receive no route indication to %d from %d\n", *sa, ri->dest, src);
	/* second path */
	path = &ri->psnd;
	//路由路径path的状态非空，且src节点是path上的节点
	//说明存在路由环路！path是基于src视角的，不应该包括src节点
	if ((IS_NULL != path->status)&&(0 != rpath_cklp(src, path->hop, path->node)))
	{
	    //则路由清零，路径状态置为IS_NULL
		rpath_clear(path);
	}

	/* primary path */
	path = &ri->pfst;
	if ((IS_NULL != path->status)&&(0 != rpath_cklp(src, path->hop, path->node)))
	{
		rpath_clear(path);
		//若此时备用路由没有清空，则将备用路由变更为优先路由
		if (IS_NULL != ri->psnd.status)
		{
			if (2 != rpath_up(ri->dest, &ri->psnd, &ri->pfst, &sn))
			ASSERT(0 == sn);
		}
	}

	//update_fwt(ri);
}

#ifndef _MR_TEST
int rp_tmsg_2nl(int len, mmsg_t *msg)
{
	int rval = 0;
	if (nl_qid != -1)  {
		rval = msgsnd(nl_qid, (void *)msg, len, 0);
		//EPT(stderr, "%s: msgsnd() write msg at qid %d\n", qinfs[re_qin].pname, nl_qid);
		if ( rval < 0 ) {
			EPT(stderr, "%s: msgsnd() write msg failed,errno=%d[%s]\n", qinfs[re_qin].pname, errno, strerror(errno));
		}
	}
	else {
		rval = -1;
		EPT(stderr, "can not get netlayer qid\n");
	}
	return rval;
}
#else
//发送sop包
int rp_tmsg_2nl(int len, mmsg_t *msg)
{
	int rval = 0;
	if (mt_qid != -1)  {
		rval = msgsnd(mt_qid, (void *)msg, len, 0);
//		EPT(stderr, "%s: msgsnd() write msg at qid %d\n", qinfs[re_qin].pname, mt_qid);
		if ( rval < 0 ) {
			EPT(stderr, "%s: msgsnd() write msg failed,errno=%d[%s]\n", qinfs[re_qin].pname, errno, strerror(errno));
		}
	}
	else {
		rval = -1;
		EPT(stderr, "can not get mrtest qid\n");
	}
	return rval;
}
#endif


//if the return value is 0, it means that this item is not filled in sop message
//把item写入消息队列
int ritem_sopget(ritem_t *ri, U8 *buf, int buflen)
{
	rpath_t *rp = &ri->pfst;
	int len = 0;

	if (WH_RP_VALD(rp->status))
	{
        //IS_ACTIVE == status || IS_UNSTABLE == status
		*(MADR *)buf = ri->dest;
		len += sizeof(MADR);

		ASSERT(rp->hop > 0 && rp->hop < MAX_HOPS);
		*(buf + len++) = rp->hop;
		memcpy(buf+len, rp->node, rp->hop*sizeof(MADR));
		len += rp->hop*sizeof(MADR);
	}
	else if (IS_EXPIRE == rp->status) {//time dead
		*(MADR *)buf = ri->dest;
		len += sizeof(MADR);
		/* setting this value means no route to the dest */
		*(buf + len++) = RP_INHOPS;
	}
	else {
		/* do nothing */
	}

	if (len > buflen) {
		EPT(stderr, "ritem_sopget: less message buffer\n");
		len = -1;
	}
	return len;
}


//check_loop,检查src是否为路径pn上的节点，返回0不是（说明非环路），返回1是（说明存在环路）
int rpath_cklp(MADR src, U8 hop, MADR *pn)
{
	int i;
	for (i = 0; i < hop; i++)
	{
		if (src == pn[i])
			break;
	}
	if (i >= hop)
		return 0;
	else
		return 1;
}

/*
 check path joint type
 return value:
	0:  node disjoint
	1:  node joint, but link disjoint
	2:  link joint
 */
int rpath_ckjt(rpath_t* p1, rpath_t *p2)
{
	int i, j;
	MADR n1;
	int rval = 0;

	for (i = 0; i < p1->hop-1; i++) {
		n1 = p1->node[i];
		for (j = 0; j < p2->hop-1; j++) {
			if (n1 == p2->node[j])
				break;
		}
		if (j < p2->hop-1) {
			break;
		}
	}

	if ( j >= p2->hop-1) {
		ASSERT(i >= p1->hop-1);
		ASSERT(rval == 0);
	}
	else {
		ASSERT(i < p1->hop-1);
		if (p1->node[i+1] == p2->node[j+1])
			rval = 2;
		else
			rval = 1;
	}
	return rval;
}

void ritem_show(ritem_t *ri)
{
	if (ri->pfst.status == IS_NULL) {
		//EPT(stderr, "node[%d]:no route to %d\n", *sa, ri->dest);
		return;
	}
	else {
		EPT(stderr, "node[%d]: 1st route to %d, status=%d, h=%d, n=%d\n", *sa, ri->dest, ri->pfst.status, ri->pfst.hop, ri->pfst.node[0]);
	}

	if (ri->psnd.status != IS_NULL) {
		EPT(stderr, "node[%d]: 2nd route to %d, status=%d, h=%d, n=%d\n", *sa, ri->dest, ri->psnd.status, ri->psnd.hop, ri->psnd.node[0]);
	}
}

void rpath_show(MADR dest, rpath_t* rp)
{
	EPT(stderr, "node[%d]: route to %d, s=%d, h=%d, n=%d\n", *sa, dest, rp->status, rp->hop, rp->node[0]);
}

void rt_show()
{
	int i;
	ritem_t *ri;
	printf("route table : =========================\n");
	for (i = 0; i < MAX_NODE_CNT; i++) {
		ri = &rt.item[i];
		ritem_show(ri);
	}
}

void nt_show()
{
	int i;
	printf("neighbour table : =========================\n");
	for (i = 0; i < MAX_NODE_CNT; i++) {
		if (nt.fl[i].lstatus == LQ_NULL)
			continue;

		EPT(stderr, "node[%d]: link to node %d, status=%d, cnt=%d\n", *sa, MR_IN2AD(i), nt.fl[i].lstatus, nt.fl[i].rcnt);
		
	}
	for (i = 0; i < MAX_NODE_CNT; i++) {
		if (nt.rl[i].lstatus == LQ_NULL)
			continue;

		EPT(stderr, "node[%d]: link from node %d, status=%d, cnt=%d\n", *sa, MR_IN2AD(i), nt.rl[i].lstatus, nt.rl[i].rcnt);
		
	}

}

void rlink_clear(rlink_t *lk)
{
	lk->lstatus = LQ_NULL;
	lk->lstatus = LQ_NULL;
	lk->rcnt = 0;
}

void rlink_inc(MADR nb)
{
	nt.rl[MR_AD2IN(nb)].rcnt++;//邻居表入链路收到的包数+1
}

void rlink_dec(MADR nb)
{
	nt.rl[MR_AD2IN(nb)].rcnt--;
}

void update_fl(MADR dst, U8 status)
{
	nt.fl[MR_AD2IN(dst)].lstatus = status;
}

/*
 up=0, message drive, remain rcnt
 up=1, timer drive, clear rcnt
 */
//根据收到的包数进行入链路状态的更新
int rlink_fsm(MADR nb, int up)
{
	int change = 0;
	//ni是邻居表nt中到nb节点的出链路
	rlink_t *ni = &nt.rl[MR_AD2IN(nb)];
	//链路当前状态记为旧状态lold，待更新
	lstat_t lold =  ni->lstatus;

	//以下是链路状态转移，只根据条件rcnt，即收到包数

	//若此时的情况为无链路
	if (LQ_NULL == ni->lstatus)
	{
	    //收到的链路包大于等于5，状态由无链路变为活跃链路
		if (ni->rcnt >= LM_NUL2ACT) {//LM_NUL2ACT = 5
			ni->lstatus = LQ_ACTIVE;//active link to a node
			change = 1;
		}
		//收到的链路包大于等于1，状态由无链路变为不稳定链路
		else if (ni->rcnt >= LM_NUL2UNS) {
			ni->lstatus = LQ_UNSTABLE;
			change = 1;
		}
		//其他情况断言收到的链路包为0
		else {
			ASSERT(LM_NUL2NUL == ni->rcnt);
		}
	}
	//若此时情况为已经存在活跃链路
	else if (LQ_ACTIVE == ni->lstatus) {
	    //若收到的链路包小于等于0，链路状态由活跃变为超时
		if (ni->rcnt <= LM_ACT2EXP) {
			ni->lstatus = LQ_EXPIRE;
			change = 1;
		}
		//若收到的链路包小于4，则链路状态由活跃变为不稳定
		else if (ni->rcnt < LM_ACT2ACT) {
			ni->lstatus = LQ_UNSTABLE;
			change = 1;
		}
		//其他情况断言收到的链路包大于等于4
		else {
			ASSERT(LM_ACT2ACT <= ni->rcnt);
		}
	}
	//若此时的情况为链路不稳定
	else if (LQ_UNSTABLE == ni->lstatus) {
	    //若收到的链路包大于等于4，则由不稳定变为活跃
		if (ni->rcnt >= LM_UNS2ACT) {
			ni->lstatus = LQ_ACTIVE;
			change = 1;
		}
		//若收到的链路包限于等于0，则由不稳定变为超时
		else if (ni->rcnt <= LM_UNS2EXP) {
			ni->lstatus = LQ_EXPIRE;
			change = 1;
		}
		//其他情况断言大于0小于4
		else {
			ASSERT(LM_UNS2EXP < ni->rcnt && LM_UNS2ACT > ni->rcnt);
		}
	}
	//若此时的情况为超时
	else if (LQ_EXPIRE == ni->lstatus) {
	    //若收到的链路包大于等于5， 则由超时变为活跃
		if (ni->rcnt >= LM_EXP2ACT) {
			ni->lstatus = LQ_ACTIVE;
			change = 1;
		}
		//若收到的链路包大于等于1，则由超时变为不稳定
		else if (ni->rcnt >= LM_EXP2UNS) {
			ni->lstatus = LQ_UNSTABLE;
			change = 1;
		}
		//其他情况断言收到为0，且从超时变为无链路
		else {
			ASSERT(ni->rcnt == LM_EXP2NUL);
			ni->lstatus = LQ_NULL;
			change = 1;
		}
	}
	else {
		EPT(stderr,"rlink_fsm():error status\n");
	}
    //如果在链路状态转移过程中发生了改变
	if (1 == change)
	{
	    //若是sop包发起的更新，且更新后的状态劣于原状态（lold），则还原原状态
		if (0 == up && lold > ni->lstatus)
		{
			ni->lstatus = lold;
			change = 0;
		}
		//否则将原状态赋值给链路旧状态，（当前状态变为更新后的状态）
		else
		{
			ni->lold = lold;
			EPT(stderr, "node[%d]: link [%d]->[%d] changed org=%d, now=%d , cnt=%d\n", *sa, nb, *sa, lold, ni->lstatus, ni->rcnt);
		}
	}
	//如果是定时器发起的更新，则收到包数清零（每隔6s）
	if (1 == up)
		ni->rcnt = 0;
	return change;
}





//检查并比较更新一条路由表
void ritem_fsm(ritem_t *ri, int up)
{
	int sn;
    //分别检查两条路由路径的状态，进行状态转移
    //rpath_fsm若返回1说明状态改变（只能变为更优状态），否则说明状态保持
    //只要两条路径有一条更新（返回1），则更新和发送转发表
    //if(rpath_fsm(&ri->psnd, up) || rpath_fsm(&ri->pfst, up))
       // update_fwt(ri);
	rpath_fsm(&ri->psnd, up);
	rpath_fsm(&ri->pfst, up);
	
	rpath_t *rp;
	rp = &ri->pfst;
	//7.11
	if(up == 1)
		EPT(stderr,"  timer drive: rpfst to %d - flag(%d),status(%d)\n", ri->dest, rp->flag, rp->status);
	else	
		EPT(stderr,"  package drive: rpfst to %d - flag(%d),status(%d)\n", ri->dest, rp->flag, rp->status);
	
	rp = &ri->psnd;
	//7.11
	if(up == 1)
		EPT(stderr,"    timer drive: rpsnd to %d - flag(%d),status(%d)\n", ri->dest, rp->flag, rp->status);
	else	
		EPT(stderr,"    package drive: rpsnd to %d - flag(%d),status(%d)\n", ri->dest, rp->flag, rp->status);
	
    //若次级路径活跃或者不稳定,则比较优先链路和次级链路，决定是否替换
	if (WH_RP_VALD(ri->psnd.status))
	{
		rpath_up(ri->dest, &ri->psnd, &ri->pfst, &sn);
		if (sn == 1)
		{
			EPT(stderr, "node[%d]: error occurs in routing table maintaining\n", *sa);
			rpath_clear(&ri->psnd);
		}
	}
}

//compare path, p1 must be valid path
//return value:
//0: equal, 1:p1 is better, 2: p2 is better
//sn:
//0: another next hop, 1:same next hop
//比较两条路由路径，先比较状态，再比较跳数，返回1说明p1更优，返回2说明p2更优
//若两条链路下一跳节点相同，则sn=1，否则sn=0
int	rpath_comp(rpath_t *p1, rpath_t *p2, int *sn)
{
	int rval;

	if (p1->status == p2->status)
	{
		ASSERT(p2->hop > 0 && p2->hop <= MAX_HOPS);
		if (p1->hop < p2->hop)
		{
			rval = 1;
		}
		else if (p1->hop > p2->hop)
		{
			rval = 2;
		}
		else
		{
			rval = 0;
		}
	}
	else if (p1->status > p2->status)
	{
		rval = 1;
	}
	else
	{
		rval = 2;
	}
#if 1
	if (p1->hop > 0 && p2->hop > 0)
	{
		if (p1->node[0] == p2->node[0])
			*sn = 1;
		else
			*sn = 0;
	}
	else
	{
		*sn = 1;
	}
#else

#endif
	return rval;
}
//compare new and original route, and update if better
//	node	next hop
//return value:
//	0:		new is better(update), the original route is copied to new
//	1:		org is better, do nothing
//	2:      error
//比较两条路由路径并更新
int rpath_up(MADR node, rpath_t *new_p, rpath_t *org, int *sn)
{
	int comp = -1;
	int rval = 0;
	rpath_t path;
    //若次级路由路径的状态并非不稳定或者激活（能用），直接返回2，不需要进行比较
	if (!WH_RP_VALD(new_p->status))
	{
		EPT(stderr, "node[%d]: new route to %d must be active or unstable, s=%d\n", *sa, node, new_p->status);
		rval = 2;
		return rval;
	}
    //清楚path，状态置为IS_NULL
	rpath_clear(&path);
    //比较两条路由路径，先比较状态，再比较跳数，返回1说明new更优，返回2说明org更优
    //若两条链路下一跳节点相同，则sn=1，否则sn=0
	comp = rpath_comp(new_p, org, sn);
    //下一跳节点不同，对应原来到节点node非一跳的情况，则选择更优路径和状态更新
	if (0 == *sn)
	{
	    //若更新链路new_p更优，将更新链路和优先链路交换
		if (1 == comp)
		{
			if (WH_RP_VALD(org->status))
			{
				rpath_copy(&path, org);
			}
			rpath_copy(org, new_p);
			rpath_copy(new_p, &path);
			ASSERT(rval == 0);
		}
		//若优先链路更优，则返回值置为1，不更新
		else if (2 == comp)
		{
			rval = 1;
		}
		else
		 {
			ASSERT(0 == comp && 0 == rval);
			/* another route to dest with equal hops */
		}
	}
	//若下一跳节点相同，对应原来也是一跳，则更新链路直接变更为优先链路，相当与更新路径状态
	else
	{
		new_p->flag = org->flag + 1;
		rpath_copy(org, new_p);
	}
	return rval;
}

//up=1, timer drive, clear flag
//up=0, message drive, remain flag
//根据flag和链路状态进行路由路径状态的转移
//只能向更优状态转移，不能向更差状态转移
int rpath_fsm(rpath_t *rp, int up)
{
    //该路由链路的下一跳邻接点地址
	MADR next = rp->node[0];
	//到该邻接点的邻接（出）链路
	rlink_t *lk = &nt.fl[MR_AD2IN(next)];
	//旧的路由路径状态
	istat_t iold = rp->status;
    //若路由路径状态为空
	if (IS_NULL == rp->status)
	{
	    //若该路径flag大于等于1，即收到了sop包
		if (rp->flag >= IM_NUL2ACT)
		{
		    //若相应的邻接链路状态为活跃，则将这条路由路径状态置为活跃
			if (LQ_ACTIVE == lk->lstatus)
				rp->status = IS_ACTIVE;
            //若相应的邻接链路状态为不稳定，则将这条路由路径状态置为不稳定
			else
			{
				ASSERT(lk->lstatus == LQ_UNSTABLE);
				rp->status = IS_UNSTABLE;
			}
		}
	}
	//若路由路径状态为超时，如果收到了sop包，则同上处理，否则将路径状态置为空
	else if (IS_EXPIRE == rp->status)
	{
		if (rp->flag >= IM_EXP2ACT)
		{
			if (LQ_ACTIVE == lk->lstatus)
				rp->status = IS_ACTIVE;
			else
			{
				ASSERT(lk->lstatus == LQ_UNSTABLE);
				rp->status = IS_UNSTABLE;
			}
		}
		else {
			rp->status = IS_NULL;
		}
	}
	//若路径状态为不稳定
	else if (IS_UNSTABLE == rp->status)
	{
	    //如果没有收到sop包，则将路径状态置为超时
		if (rp->flag < IM_ACT2EXP)
		{
			rp->status = IS_EXPIRE;
		}
		//若相应的邻接链路状态为活跃，则将这条路由路径状态置为活跃
		else if (LQ_ACTIVE == lk->lstatus)
		{
			rp->status = IS_ACTIVE;
		}
	}
	//其他情况（断言路径状态活跃）
	else
	{
		ASSERT(rp->status == IS_ACTIVE);
		//如果没有收到sop包，则置为超时
		if (rp->flag < IM_ACT2EXP)
		{
			rp->status = IS_EXPIRE;
		}
		//若收到sop包，但链路状态为不稳定，则将路径状态置为不稳定
		else if (LQ_UNSTABLE== lk->lstatus)
		{
			rp->status = IS_UNSTABLE;
		}
	}

	int change = 0;
	if (iold != rp->status)
	{
	    //若旧状态优于新状态且本次更新是msg更新的（非timer更新），则保持旧状态
	    //比如，sop包发起check（up=0），但是新的状态差于旧状态，则不更新
		if (iold > rp->status && 0 == up)
			rp->status = iold;
		//否则最终确定更新状态（新），返回change=1
		else
			change = 1;
	}
	//如果up为1，说明是timer发起的更新，则将flag置0，表示新的check阶段没有收到sop包
	if (1 == up)
		rp->flag = 0;
	


	return change;
}

void signal_show(int signal)
{
    printf("Signal = %d\n",signal);
    nt_show();
    rt_show();
    return;
}

void print_uip(int msg_len, mmsg_t * tmsg)
{
	mmhd_t *phd = (mmhd_t *)tmsg->data;
	int len = 0;
	int node_cnt;
	int i;
	EPT(stderr,"SEND uip\n");
	EPT(stderr,"LEN:%d\n", msg_len);
	EPT(stderr," mtype:%ld\n", tmsg->mtype);
	EPT(stderr," node:%d\n", tmsg->node);
	EPT(stderr," phd->type:%d\n", phd->type);
	EPT(stderr," phd->len:%d\n", phd->len);
	
	len += MMHD_LEN;
	
	MADR *buf_tmp;
	buf_tmp = tmsg->data + len;
	
	EPT(stderr," data:%d(src) ", *buf_tmp);
	
	buf_tmp ++;
	
	EPT(stderr,"%d(dst) ", *buf_tmp);
	buf_tmp ++;
	
	EPT(stderr,"%d(status) ", *buf_tmp);
	buf_tmp ++;
	
	EPT(stderr,"%d(node_cnt) ", *buf_tmp);
	node_cnt = *buf_tmp;
	buf_tmp ++;
	
	for(i = 0; 
		i < msg_len - sizeof(MADR) - len - 4;
		i++)
	{
		EPT(stderr,"%d ", *buf_tmp);
		buf_tmp++;
	}
	EPT(stderr,"\n");
}

void print_uibp(int msg_len, mmsg_t * tmsg)
{
	mmhd_t *phd = (mmhd_t *)tmsg->data;
	uibp_hd *ubphd = (uibp_hd *)(tmsg->data + MMHD_LEN);
	int len = 0;
	int node_cnt;
	int i;
	EPT(stderr,"SEND uibp\n");
	EPT(stderr,"LEN:%d\n", msg_len);
	EPT(stderr," mtype:%ld\n", tmsg->mtype);
	EPT(stderr," node:%d\n", tmsg->node);
	EPT(stderr," phd->type:%d\n", phd->type);
	EPT(stderr," phd->len:%d\n", phd->len);
	EPT(stderr," ubphd->node:%d\n", ubphd->node);
	EPT(stderr," ubphd->icnt:%d\n", ubphd->icnt);
	
	
	len += MMHD_LEN + sizeof(uibp_hd);
	
	MADR *buf_tmp;
	buf_tmp = tmsg->data + len;
	
	EPT(stderr," data:%d(src) ", *buf_tmp);
	
	buf_tmp ++;
	
	EPT(stderr,"%d(dst) ", *buf_tmp);
	buf_tmp ++;
	
	EPT(stderr,"%d(status) ", *buf_tmp);
	buf_tmp ++;
	
	EPT(stderr,"%d(node_cnt) ", *buf_tmp);
	node_cnt = *buf_tmp;
	buf_tmp ++;
	
	for(i = 0; 
		i < msg_len - sizeof(MADR) - len - 4;
		i++)
	{
		EPT(stderr,"%d ", *buf_tmp);
		buf_tmp++;
	}
	EPT(stderr,"\n");
}

void print_ulack(int msg_len, mmsg_t * tmsg)
{
	mmhd_t *phd = (mmhd_t *)tmsg->data;
	int len = 0;
	int node_cnt;
	int i;
	EPT(stderr,"SEND ulack\n");
	EPT(stderr,"LEN:%d\n", msg_len);
	EPT(stderr," mtype:%ld\n", tmsg->mtype);
	EPT(stderr," node:%d\n", tmsg->node);
	EPT(stderr," phd->type:%d\n", phd->type);
	EPT(stderr," phd->len:%d\n", phd->len);
	
	len += MMHD_LEN;
	
	MADR *buf_tmp;
	buf_tmp = tmsg->data + len;
	
	EPT(stderr," data:%d(src) ", *buf_tmp);
	
	buf_tmp ++;
	
	EPT(stderr,"%d(dst) ", *buf_tmp);
	buf_tmp ++;
	
	EPT(stderr,"%d(status) ", *buf_tmp);
	buf_tmp ++;
	
	EPT(stderr,"%d(node_cnt) ", *buf_tmp);
	node_cnt = *buf_tmp;
	buf_tmp ++;
	
	for(i = 0; i < node_cnt; i++)
	{
		EPT(stderr,"%d ", *buf_tmp);
		buf_tmp++;
	}
	

	
	EPT(stderr,"\n");
}

//单向链路单播报文(有到dst的路由) 组包function
int rp_uip_gen(int id, int node_cnt, int item_len, U8* node)
{
//	ASSERT(uni_link[id].flag == 3);
	
	int len = 0;
	
	mmsg_t tmsg;
	mmhd_t *phd = (mmhd_t *)tmsg.data;
	
	tmsg.mtype = MMSG_RPM;
#ifdef _MR_TEST
    //本节点号的地址
	tmsg.node = *sa;
#else
	tmsg.node = uni_link[id].src;
#endif
	
	phd->type = RPM_FHR_UIP;

	len += MMHD_LEN;
	
	MADR *buf_tmp;
	buf_tmp = tmsg.data + len;
	
	*buf_tmp = uni_link[id].src;
	buf_tmp ++;
	
	*buf_tmp = uni_link[id].dst;
	buf_tmp ++;
	
	*buf_tmp = (MADR)(uni_link[id].status);
	buf_tmp ++;
	
	*buf_tmp = node_cnt + 1;
	buf_tmp ++;
	
	memcpy(buf_tmp, node, node_cnt);
	*(buf_tmp + node_cnt) = *sa;
	
	phd->len = 4 + node_cnt + 1;

	if(item_len == 0)
	{
		U8* item_r;
		int i, len_temp = 0;
		
		item_r = buf_tmp + node_cnt + 1;
		*item_r = 0;
		
		buf_tmp = buf_tmp + node_cnt + 2;
		//把下游节点的路由表也发给上游节点，告诉上游节点通过本节点（下游节点）还可以到哪些节点
		for(i = 0; i < MAX_NODE_CNT; i++)
		{
			//这是类似哈系表的形式，下标和表项的目的地址映射对应
			ASSERT(MR_IN2AD(i) == rt.item[i].dest);
			//到本节点的表项跳过
			if (MR_IN2AD(i) == *sa)
				continue;
			//到上游节点的表项跳过
			if (MR_IN2AD(i) == uni_link[id].src)
				continue;
			
			int rval = 0;
			//将目的地址，跳数，每跳节点依次填入tmsg.data + len开始的地址，返回填充长度，最后参数验证是否越界
			rval = ritem_sopget(&rt.item[i], buf_tmp + len_temp, MAX_DATA_LENGTH - len);
			if (rval == -1)
				EPT(stderr, "error occurs in ritem_sogget()\n");
			else
			{
				if (rval > 0)
				{
					*item_r += 1;
					len_temp += rval;
					EPT(stderr,"ADD rt.item[%d] (item_r = %d, rval = %d)in ul_ack\n", i, *item_r, rval);
				}
			}
		}
		phd->len += 1 + len_temp; 
	}
	else
	{
		memcpy(buf_tmp + node_cnt + 1, node + node_cnt, item_len);
		phd->len += item_len; 
	}
	
	len += phd->len;

	
	uni_link[id].ctime = time(NULL);
	
	rp_tmsg_2nl(len + sizeof(MADR), &tmsg);
	
	print_uip(len+sizeof(MADR), &tmsg);
	
	
	uni_link[id].flag = 1;
}
//单向链路广播报文 组包function
int rp_uibp_gen(int id, int node_cnt, int item_len, U8* node)
{
	int i;
	int len = 0;
	
	mmsg_t tmsg;
	mmhd_t *phd = (mmhd_t *)tmsg.data;
	uibp_hd *ubphd = (uibp_hd *)(tmsg.data + MMHD_LEN);
	
	tmsg.mtype = MMSG_RPM;
#ifdef _MR_TEST
    //本节点号的地址
	tmsg.node = *sa;
#else
	tmsg.node = MADR_BRDCAST;
#endif

	phd->type = RPM_FHR_UIBP;

	ubphd->node = *sa;
	ubphd->icnt = 1;
	//U8 *item = &ubphd->icnt;

	len += MMHD_LEN + sizeof(uibp_hd);
	
	U8 *buf_tmp;
		
	//	ASSERT(uni_link[id].flag == 3);
	
	buf_tmp = tmsg.data + len;

	*buf_tmp = uni_link[id].src;
	buf_tmp ++;
	
	*buf_tmp = uni_link[id].dst;
	buf_tmp ++;
	
	*buf_tmp = (MADR)(uni_link[id].status);
	buf_tmp ++;
	
	*buf_tmp = node_cnt + 1;
	buf_tmp ++;
	
	memcpy(buf_tmp, node, node_cnt);
	*(buf_tmp + node_cnt) = *sa;
	
	phd->len = 4 + node_cnt + 1;
	

	if(item_len == 0)
	{
		U8* item_r;
		int i, len_temp = 0;
		
		item_r = buf_tmp + node_cnt + 1;
		*item_r = 0;
		
		buf_tmp = buf_tmp + node_cnt + 2;
		//把下游节点的路由表也发给上游节点，告诉上游节点通过本节点（下游节点）还可以到哪些节点
		for(i = 0; i < MAX_NODE_CNT; i++)
		{
			//这是类似哈系表的形式，下标和表项的目的地址映射对应
			ASSERT(MR_IN2AD(i) == rt.item[i].dest);
			//到本节点的表项跳过
			if (MR_IN2AD(i) == *sa)
				continue;
			//到上游节点的表项跳过
			if (MR_IN2AD(i) == uni_link[id].src)
				continue;
			
			int rval = 0;
			//将目的地址，跳数，每跳节点依次填入tmsg.data + len开始的地址，返回填充长度，最后参数验证是否越界
			rval = ritem_sopget(&rt.item[i], buf_tmp + len_temp, MAX_DATA_LENGTH - len);
			if (rval == -1)
				EPT(stderr, "error occurs in ritem_sogget()\n");
			else
			{
				if (rval > 0)
				{
					*item_r += 1;
					len_temp += rval;
					EPT(stderr,"ADD rt.item[%d] (item_r = %d, rval = %d)in ul_ack\n", i, *item_r, rval);
				}
			}
		}
		phd->len += 1 + len_temp; 
	}
	else
	{
		memcpy(buf_tmp + node_cnt + 1, node + node_cnt, item_len);
		phd->len += item_len; 
	}
	
	len += phd->len;

	uni_link[id].ctime = time(NULL);
	
	rp_tmsg_2nl(len + sizeof(MADR), &tmsg);
	
	uni_link[id].flag = 1;
	
	print_uibp(len+sizeof(MADR), &tmsg);
	
}

//单向链路确认报文 组包function
int rp_ul_ack_gen(U8 *data)
{
	int msg_len = 0;
	int hop = 0;
	ul_record *ul = (ul_record *)data;
	
	mmsg_t tmsg;
	mmhd_t *phd = (mmhd_t *)tmsg.data;
	
	if(ul->src != *sa)
		EPT(stderr,"! ! Wanna send ul_ack while ul->src=%d, *sa=%d\n\n", ul->src, *sa);
	
	tmsg.mtype = MMSG_RPM;
#ifdef _MR_TEST
    //本节点号的地址
	tmsg.node = *sa;
#else
	tmsg.node = ul->dst;
#endif
	
	phd->type = RPM_FHR_ULACK;

	msg_len += MMHD_LEN;
	
	U8 *buf_tmp;
	buf_tmp = tmsg.data + msg_len;
	
	ul->node_cnt ++;
	
	phd->len = 2*(sizeof(MADR)) + 2 + ul->node_cnt;

	memcpy(buf_tmp, ul, phd->len - 1);
	

	*(buf_tmp + phd->len - 1) = *sa;

	msg_len += phd->len;
	
	
	rp_tmsg_2nl(msg_len + sizeof(MADR), &tmsg);
	
	print_ulack(msg_len + sizeof(MADR), &tmsg);
}

void inform_uni_link(MADR src, MADR dst, U8 status, int node_cnt, int len, MADR *node)
{
	int id = 0;
	int sn = 0;
	id = find_useful_uni_link(src, dst, 
			status ? status : nt.rl[MR_AD2IN(src)].lstatus, &sn);
	
	if(id == -1)
	{
		EPT(stderr,"!!! UNI_LINK is FULL\n");
		return;
	}
	if(uni_link[id].flag == 0)
	{
		uni_link[id].flag = 3;
		uni_link[id].src = src;
		uni_link[id].dst = dst;
		uni_link[id].ctime = time(NULL);
		uni_link[id].status = status?status:nt.rl[MR_AD2IN(src)].lstatus;
		if(uni_link[id].dst == *sa)
		{
			if(sn == 0)
				EPT(stderr,"!!! assert(sn == 1) error\n");
			sn = 0;
			EPT(stderr,"~ ~ ~ find a uni_link %d -> %d\n", src, *sa);
		}
	}
	if(sn == 0)
	{
		EPT(stderr, "Doesn't need to send the uni_link:[%d]->[%d]\n", src, *sa);
		return;
	}
	else if(sn == 1)
	{
		EPT(stderr,"Send uni_link(uip or uibp):\n");
		
		if(WH_RP_VALD(rt.item[MR_AD2IN(src)].pfst.status))
			rp_uip_gen(id, node_cnt, len?len - 4 - node_cnt:0, node);
		else
			rp_uibp_gen(id, node_cnt, len?len - 4 - node_cnt:0, node);
	}
}


int find_useful_uni_link(MADR src, MADR dst, U8 status, int *sn)
{
	int i;

	//check if the uni link is existed already
	for(i = 0; i < MAX_UNILINK_NUM; i++)
	{
		if((uni_link[i].flag >= 1)&&(uni_link[i].src == src)&&(uni_link[i].dst == dst))
		{
			EPT(stderr,"uni_link is exist on %d\n", i);
			*sn = need_send2other(i, status);
			
			//clear the other src->dst's uni_link
			int j;
			for(j = i+1; j < MAX_UNILINK_NUM; j++)
			{
				if((uni_link[j].src == src)&&(uni_link[j].dst == dst))
				{
					EPT(stderr,"clear the older uni_link of %d\n", j);
					uni_link[j].flag = 0;
				}
					
			}
	
			return i;
		}	
	}
	//no -> find an empty space
	for(i = 0; i < MAX_UNILINK_NUM; i++)
	{
		if(uni_link[i].flag == 0)
		{
			*sn = 1;
			EPT(stderr,"Find an empty space on %d\n", i);
			return i;
		}	
		
	}
	//no empty space -> try to clear the old( >60s ) uni_link
	for(i = 0; i < MAX_UNILINK_NUM; i++)
	{
		int ctime;
		ctime = time(NULL);
		if((ctime - uni_link[i].ctime) > 60 && (uni_link[i].flag == 1))
		{
			clear_uni_link(i);
			EPT(stderr,"Clear to get an empty space on %d\n", i);
			*sn = 1;
			return i;
		}
	}

	return -1;
}

int need_send2other(int id, U8 status)
{
	if(uni_link[id].flag == 3)
	{
		if(uni_link[id].dst == *sa)
		{
			int ctime = time(NULL);
			if(ctime - uni_link[id].ctime < 5)
			{
				EPT(stderr," Doesn't send the uni_link %d to %d\n", uni_link[id].src, uni_link[id].dst);
				EPT(stderr," Now:%d, ctime:%d\n", ctime, uni_link[id].ctime);
				uni_link[id].status = status;
				return 0;	
			}
			else
			{
				EPT(stderr," Ready to send the uni_link %d to %d\n", uni_link[id].src, uni_link[id].dst);
				uni_link[id].flag = 2;
			}
		}
		else 
			uni_link[id].flag = 1;
		
		uni_link[id].status = status;
		uni_link[id].ctime = time(NULL);
		return 1;
	}
		
	else if(uni_link[id].flag == 1)
	{
		int ctime;
		ctime = time(NULL);
		if((ctime - uni_link[id].ctime) > 6)
		{
			uni_link[id].flag = 3;
			uni_link[id].status = status;
			uni_link[id].ctime = ctime;
			return 1;
		}
		else
			return 0;
	}
	else if(uni_link[id].flag == 2)
		return 0;
	else
		EPT(stderr,"!!! uni_link[%d].flag = %d\n",id, uni_link[id].flag);
}

void confirm_ul(U8 src, U8 status)
{
	int i;

	//check if the uni link is existed already
	for(i = 0; i < MAX_UNILINK_NUM; i++)
	{
		if((uni_link[i].flag >= 1)&&(uni_link[i].src == src)&&(uni_link[i].dst == *sa))
		{
			if(status >= uni_link[i].status)
			{
				EPT(stderr,"   Confirm the ul_ack(%d->%d) on uni_link[%d]\n", uni_link[i].src, uni_link[i].dst, i);
				uni_link[i].status = status;
				uni_link[i].flag = 1;
			}
		}	
	}
}

void clear_uni_link(int id)
{
	uni_link[id].flag = 0;
	uni_link[id].src = 0;
	uni_link[id].dst = 0;
	uni_link[id].status = 0;
	uni_link[id].timer_ttl = 0;
}