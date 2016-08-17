#include "mr_common.h"
#include "rp_common.h"
#include "rp_fhr.h"


extern rtable_t rt;
extern ntable_t nt;
extern MADR *sa;

void rp_fhrmsg_disp(MADR node, int sub, int len, U8 *data)
{
	switch(sub) {
		case RPM_FHR_SOP:
			rp_fhrsop_proc(node, len, data);
			break;

		case RPM_FHR_RII:
			rp_fhrrii_proc(node, len, data);
			break;

		case RPM_FHR_RIR:
			rp_fhrrir_proc(node, len, data);
			break;

		default:
			EPT(stderr, "fhr: unknowm protocol message\n");
			break;
	}
}

//这里的data是消息队列data部分中psh的起始地址，len参数是phd->len，即psh+item[n]的长度
void rp_fhrsop_proc(MADR node, int len, U8 *data)
{
	int i, pos = 0;
	U8 items, hop;
	MADR dest;
//	EPT(stdout, "node[%d]: reveive sop message, nb=%d, len=%d\n", *sa, node, len);
#if 0
	for (i = 0; i < len; i++) {
		EPT(stderr, "%3d", data[i]);
	}
	EPT(stderr, "\n");
#endif
    //src是将此sop包发来的邻接点
	MADR src = *(MADR *)data;
	//pos是读指针位置
	pos += sizeof(MADR);
	ASSERT(src == node);
	//邻居表出到src节点链路收到的包数+1，用于决定链路状态的改变
	rlink_inc(src);
	//根据收到的包数进行链路状态转移，更新到src节点的链路状态，第二个参数为0说明是sop包发起更新,不清零收到包数
	//若更新的状态优于当前状态，则以更新状态替换为当前状态，当前状态替换为旧状态，否则不替换
	rlink_fsm(src, 0);

	/* drop the message of LQ_NULL or LQ_EXPIRE */
	if (!WH_NL_FEAS(nt.fl[MR_AD2IN(src)].lstatus))
	{
	    //WH_NL_FEAS链路状态为活跃或者不稳定，若条件不成立则丢弃返回
		EPT(stderr, "node[%d]: drop the message from link to %d, status=%d\n", *sa, MR_AD2IN(src), nt.fl[MR_AD2IN(src)].lstatus);
		return;
	}
    //赋值一条到邻接点src的路由（src为下一跳）并与原来比较，若更优则更新之
	ritem_nup(src, NULL, 0);
	//检查和更新一条路由链路，第二个参数up=0说明是数据包更新而不是定时器更新
	//本函数内部嵌入跟新转发表部分*****
	ritem_fsm(&rt.item[MR_AD2IN(src)], 0);

    //上面是根据sop包的头部和sop包数量更新这条到一跳邻节点的路由路径，下面开始读取sop包的item数据

	items = *(data + pos++);
    //EPT(stderr, "sop message: items=%d\n", items);

	for(i = 0; i < items; i++)
	{
	    //该条路由路径目的节点
		dest = *(MADR *)(data + pos);
		pos += sizeof(MADR);
        //到目的节点dest跳数
		hop = *(data + pos++);
        //若达到最大跳数才检查路由环路？
		if (RP_INHOPS == hop)
		{
		    //检查路由环路，若存在则清空路由
		    //本函数内嵌入跟更新转发表部分*****
			ritem_del(&rt.item[MR_AD2IN(dest)], src);
		}
		else
		{
			if ((hop > MAX_HOPS)||(pos + hop*sizeof(MADR) > len))
			{
				EPT(stderr, "wrong sop message dest=%d,hop=%d,len=%d\n", dest, hop, len);
				break;
			}
			//将src作为下一跳更新路由并比较，若更优则替换
			ritem_up(&rt.item[MR_AD2IN(dest)], src, hop, (MADR*)(data+pos));
			ritem_fsm(&rt.item[MR_AD2IN(dest)], 0);
			pos += hop*sizeof(MADR);
		}
	}

	if (pos != len) {
		EPT(stderr, "node[%d]: the sop message len is wrong, items=%d\n", *sa, items);
	}
    //对比路由表，更新转发表，如果转发表有变化，则通知底层
    update_fwt();
}



void rp_fhrrii_proc(MADR node, int len, U8 *data)
{
}

void rp_fhrrir_proc(MADR node, int len, U8 *data)
{
}

