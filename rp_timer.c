#include "mr_common.h"
#include "rp_common.h"
#include "rp_timer.h"
#include "rp_fhr.h"

tsche_t rp_tsch = {
	0,              //map
	0,              //mask
	{
		{"sop generate", 0, RT_SOP_INTV,  rp_sop_gen},	//RT_SOP_INTV1000ms，5个系统定时器周期调用一次rp_sop_gen
		{"route expire", 0, RT_ITEM_EXPI, rp_rt_check},//RT_ITEM_EXPI 5000ms
		{"link maintain", 0, RT_LINK_CHK, rp_lk_check} //RT_LINK_CHK 5000ms
		//name,wait,period,function
	}
};

const int rp_tfs = sizeof(rp_tsch.procs)/sizeof(rp_tsch.procs[0]);//numbers of timer
//itimerval上面应该是定时器间隔，下面应该是首次启动的延迟
struct itimerval new_value = {
{RP_TINTVL_S, RP_TINTVL_US},  //0s,200000us
{RP_TDELAY_S, RP_TDELAY_US}   //1s,0us
};

extern rtable_t rt;
extern ntable_t nt;
extern MADR*	sa;

void rp_tsch_init()
{
	int i, cnt, sopcnt = 0;;
	tproc_t *p;//定时器结构提指针，包括名字，等待，间隔，调用函数四个成员

	ASSERT(rp_tsch.tmap == 0);
	ASSERT(rp_tsch.tmask == 0);
    //用户设置了rp_tfs个映射定时器
	for (i = 0; i < rp_tfs; i++)
	{
		p = &rp_tsch.procs[i];
		if (p->period == 0)
		{
			ASSERT(rp_tsch.procs[i].pf == NULL);
			continue;
		}

		rp_tsch.tmap = rp_tsch.tmap|(1<<i);
		//等待cnt个系统定时器周期激活一次定时器rp_tsch.procs[i],分母是200000us
		//定时器的初值period如1000ms，或者5000ms等并不是说其周期是1000ms，而是要用这个值计算比例次数
		//经过这个计算，rp_sop_gen，rp_rt_check，rp_lk_check的period分别为1,5,5
		cnt = (p->period * 1000)/RP_TINTVL_US;
		if (cnt < 1)
			p->period = 1;
		else
			p->period = cnt;
#if 0
        //确定三个映射定时器的调用周期,#if 1 里面的内容似乎没有用到？
        //sop generate定时器为每一秒调用一次rp_sop_gen函数生成sop包
		if (1 == strcmp("sop generate", p->name))
		{
			sopcnt = p->period;
		}
		//route expire定时器为每若干次定时器调用一次rp_rt_check函数检查路由
		else if (1 == strcmp("route expire", p->name))
		{
			if (0 != sopcnt)
			{
				p->period = sopcnt*(RT_ITEM_EXPI/RT_SOP_INTV) + 1;
			}
		}
		//route expire定时器为每若干次定时器调用一次rp_lk_check函数检查链路
		else if (1 == strcmp("link maintain", p->name))
		{
			if (0 != sopcnt)
			{
				p->period = sopcnt*(RT_LINK_CHK/RT_SOP_INTV) + 1;
			}
		}
#endif
		p->wait = p->period;
	}
}
//被main函数调用
int rp_start_timer()
{
	int rval;

	rp_tsch_init();//确定三个映射定时器的调用周期（基于系统定时器）

	signal(SIGALRM, rp_timer_sche);//rp_timer_sche  wait for SIGALRM
	rval = setitimer(ITIMER_REAL, &new_value, NULL);//计时到达将发送SIGVTALRM信号给进程
	if (-1 == rval)	{
		/* failure */
		EPT(stderr, "error occurs in setting timer %d[%s]\n", errno, strerror(errno));
	}
	else {
		/* success */
		rval = 0;
	}
	return rval;
}
//收到SIGALRM信号，则调用该函数，即周期为1s
void rp_timer_sche(int signo)
{
   // printf("rcv timer\n");
	int i;
	U32	emap;

	if (SIGALRM != signo)
	{
		EPT(stderr, "Caught the other signal %d\n", signo);
		return;
	}

//	EPT(stderr, "Caught the SIGALRM signal\n");
    //&与运算，^异或运算
	emap = rp_tsch.tmap&(rp_tsch.tmap^rp_tsch.tmask);
	for (i = 0; i < rp_tfs; i++)
	{
		if (!((1<<i)&emap))
			continue;
        //wait初值等于period，每一次经历系统定时器则wait减一
		rp_tsch.procs[i].wait -= 1;
		if (rp_tsch.procs[i].wait <= 0)
		{
		    //当wait减到0则重新初始化为period
			rp_tsch.procs[i].wait = rp_tsch.procs[i].period;
			//调用该定时器的函数，参数为什么为i？
			(*rp_tsch.procs[i].pf)(&i);
		}
	}
}
//每收到1次定时器信号调用一次，即周期为1s
void rp_sop_gen(void *data)
{
    //int id = *(int *)data;
    //EPT(stderr, "Caught the SIGALRM signal for %s\n", rp_tsch.procs[id].name);
	int rval, i;
	int len = 0;
    //消息队列=mtyp + node + (mmhd)pkg_headd + sop_head + true_data
	mmsg_t tmsg;
	mmhd_t *phd = (mmhd_t *)tmsg.data;
	sop_hd *psh = (sop_hd *)(tmsg.data + MMHD_LEN);
	//sop_head = node + icnt_l + icnt_r 即地址加two数量
	U8 *item_l = &psh->icnt_l;
	U8 *item_r = &psh->icnt_r;
	
    //路由协议消息类型
	tmsg.mtype = MMSG_RPM;
#ifdef _MR_TEST
    //本节点号的地址
	tmsg.node = *sa;
#else
	tmsg.node = MADR_BRDCAST;
#endif
	/*rp message header */
	phd->type = RPM_FHR_SOP;
	phd->len = MMHD_LEN + sizeof(sop_hd);
	//加上phd的长度
	len += MMHD_LEN;
	/* sop header */
	psh->node = *sa;
	*item_l = 0;
	*item_r = 0;
	//加上psh的长度
	len += sizeof(sop_hd);
//	EPT(stderr, "psh->icnt=%d\n", psh->icnt);
	
	MADR *buf_tmp;
	buf_tmp = tmsg.data + len;
	for(i = 0; i < MAX_NODE_CNT; i++)
	{
		if (MR_IN2AD(i) == *sa)
			continue;
		if(nt.rl[i].lstatus >= LQ_UNSTABLE)
		{
			*buf_tmp = MR_IN2AD(i);
			buf_tmp ++;
			
			*buf_tmp = (U8)(nt.rl[i].lstatus);
			buf_tmp ++;
			
			len += (sizeof(MADR) + 1);
			*item_l += 1;
		}
	}
	for(i = 0; i < MAX_NODE_CNT; i++)
	{
	    //这是类似哈系表的形式，下标和表项的目的地址映射对应
		ASSERT(MR_IN2AD(i) == rt.item[i].dest);
		//到本节点的表项跳过
		if (MR_IN2AD(i) == *sa)
			continue;
		rval = 0;
		//将目的地址，跳数，每跳节点一次填入tmsg.data + len开始的地址，返回填充长度，最后参数验证是否越界
		rval = ritem_sopget(&rt.item[i], tmsg.data + len, MAX_DATA_LENGTH - len);
		if (rval == -1)
			EPT(stderr, "error occurs in ritem_sogget()\n");
		else
		{
			if (rval > 0)
			{
				*item_r += 1;
				len += rval;
			}
		}
	}
//	EPT(stderr, "psh->icnt=%d\n", psh->icnt);
    //即代表从psh到消息末尾的长度，注意，len是消息队列的data总长度！
	phd->len = len - MMHD_LEN;
	//通过消息队列发出tmsg，第一个参数是发送长度
	rp_tmsg_2nl(len + sizeof(MADR), &tmsg);//msg_snd
}
//每收到5次定时器信号调用一次，即周期为5s
void rp_rt_check(void *data)
{
	//int id = *(int *)data;
	//EPT(stderr, "Caught the SIGALRM signal for %s\n", rp_tsch.procs[id].name);
	int i;
	int comp, sn;

	//rt_show();
	for(i = 0; i < MAX_NODE_CNT; i++) {
		ASSERT(MR_IN2AD(i) == rt.item[i].dest);
        //ritem_fsm(&rt.item[i], 1);
		if (IS_NULL == rt.item[i].pfst.status)
			continue;

		ritem_fsm(&rt.item[i], 1);
	}
	//检查和更新转发表
	update_fwt();
}
//每收到5次定时器信号调用一次，即周期为5s
void rp_lk_check(void *data)
{
	//int id = *(int *)data;
	//EPT(stderr, "Caught the SIGALRM signal for %s\n", rp_tsch.procs[id].name);
	int i, j, nn, change;
	rpath_t *path;
	rlink_t *lk;

	//nt_show();

	for (i = 0; i < MAX_NODE_CNT; i++)
	{
		lk = &nt.fl[i];
		//如果收到包数为0或者链路状态为LQ_NULL则跳过该条链路
		if (LQ_NULL == lk->lstatus && 0 == lk->rcnt) {
			continue;
		}
        //EPT(stderr, "node[%d]: enter to check the link to node %d\n", *sa, MR_IN2AD(i));
		//根据收到的包数rcnt进行链路状态的转移，转移成功则返回1
		change = rlink_fsm(MR_IN2AD(i), 1);

		if (change == 0)
		{
			continue;
		}
        //为何此处断言？
		ASSERT(LQ_ACTIVE != lk->lstatus);
		nn = MR_IN2AD(i);
		EPT(stderr, "node[%d]: find link to %d changed, status=%d\n", *sa, nn, lk->lstatus);

		/* update route */
		for (j = 0; j < MAX_NODE_CNT; j++)
		{
			/* second path */
			path = &rt.item[j].psnd;
			//该路由路径的下一跳是正在check的链路邻接点
			if ((IS_NULL !=path->status)&&(nn == path->node[0]))
			{
			    //如果路由路径状态优于链路状态，则以链路状态替换路由路径状态
				if (path->status > lk->lstatus)
				{
					path->status = lk->lstatus;
					if (IS_NULL == path->status)
						rpath_clear(path);
				}
			}
			/* primary path */
			path = &rt.item[j].pfst;
			if ((IS_NULL !=path->status)&&(nn == path->node[0]))
			{
				if (path->status > lk->lstatus)
				{
					path->status = lk->lstatus;
					if (IS_NULL == path->status)
						rpath_clear(path);
				}
				int sn;
				if (IS_NULL != rt.item[j].psnd.status)
				{
				    //路由路径比较返回非2说明次级路径可用，若sn置0说明下一跳节点不同
					if (2 != rpath_up(MR_IN2AD(j), &rt.item[j].psnd, path, &sn))
						ASSERT(0 == sn);
				}
			}
		}
		nt_show();
		rt_show();
	}
}
