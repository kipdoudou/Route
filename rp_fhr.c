#include "mr_common.h"
#include "rp_common.h"
#include "rp_fhr.h"


extern rtable_t rt;
extern ntable_t nt;
extern MADR *sa;
extern uni_link_t uni_link[MAX_UNILINK_NUM];

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
	int tmp_pos = 0;

	U8 item_l, item_r, hop, status;
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
	//邻居表，src到本节点的链路（本节点邻居链表的入链路）收到的包数+1，用于决定链路状态的改变
	rlink_inc(src);
	//根据收到的包数进行链路状态转移，更新src节点的入链路状态，第二个参数为0说明是sop包发起更新,不清零收到包数
	//若更新的状态优于当前状态，则以更新状态替换为当前状态，当前状态替换为旧状态，否则不替换
	rlink_fsm(src, 0);

	/* drop the message of LQ_NULL or LQ_EXPIRE */
	if (!WH_NL_FEAS(nt.rl[MR_AD2IN(src)].lstatus))
	{
	    //WH_NL_FEAS链路状态为活跃或者不稳定，若条件不成立则丢弃返回
		EPT(stderr, "node[%d]: drop the message from link to %d, status=%d\n", *sa, MR_AD2IN(src), nt.fl[MR_AD2IN(src)].lstatus);
		return;
	}

	item_l = *(data + pos++);
	item_r = *(data + pos++);

	tmp_pos = pos;
	for(i = 0; i < item_l; i++)
	{
		dest = *(MADR* )(data + tmp_pos);
		tmp_pos += sizeof(MADR);
		
		EPT(stderr,"  item_l[%d] = %d\n", i, dest);
		
		if(dest != *sa)
		{
			tmp_pos ++;
			continue;
		}
		else 
		{
			status = *(data + tmp_pos++);
			
			EPT(stderr,"   status = %d\n", status);
			
			if(status >= LQ_UNSTABLE)
			{
				nt.fl[MR_AD2IN(src)].lstatus = status;

	  			//赋值一条到邻接点src的路由（src为下一跳）并与原来比较，若更优则更新之
				
				EPT(stderr,"*** sop driver update the rpath to %d: ***\n", dest);
				
				ritem_nup(src, NULL, 0);
				//检查和更新一条路由链路，第二个参数up=0说明是数据包更新而不是定时器更新
				//本函数内部嵌入跟新转发表部分*****
				ritem_fsm(&rt.item[MR_AD2IN(src)], 0);
			}
			else
				EPT(stderr,"ERROR: link [%d]->[%d], status:%d\n",dest, *sa, status);
			break;
		}
	}

	if( (i != 0) && (i == item_l) )
	{
		
		inform_uni_link(src, *sa, 0, 0, 0, NULL);
		//因为本节点sa到不了src，所以src的路由也就没有参考价值了(so直接return)
		
		EPT(stderr,"Sop's content non-use\n");
		return;
	}

	pos += item_l*(sizeof(MADR) + 1);

    //上面是根据sop包的头部和sop包数量更新这条到一跳邻节点的路由路径，下面开始读取sop包的item数据

	
    //EPT(stderr, "sop message: items=%d\n", items);

	for(i = 0; i < item_r; i++)
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
		EPT(stderr, "node[%d]: the sop message len(%d) is wrong, item_l=%d item_r=%d\n", *sa, len, item_l, item_r);
	}
    
    
#ifndef _MR_TEST
	//对比路由表，更新转发表，如果转发表有变化，则通知底层
	update_fwt();
#endif
}

/*收到uip报文处理函数*/
void rp_fhruip_proc(MADR node, int len, U8 *data)
{
	int pos = 0;
	MADR src = *(MADR *)data;
	
	pos += sizeof(MADR);	
	
	MADR dst = *(MADR *)(data+pos);
	pos += sizeof(MADR);	
	
	U8 status = *(data + pos);
	pos += 1;	

	if(src == *sa)
	{
		
		/*！！！这里应该也记录下该单向链路，然后避免重复发送*/

		//根据收到的单向链路的状态更新上游节点的出链路状态
		update_fl(dst, status);
		//赋值一条到下游节点(dst)的路由（1跳），并与原到下游节点的路由比较，更优则更新之
		
		EPT(stderr,"*** uip driver update the rpath to %d: ***\n", dst);
		
		ritem_nup(dst, NULL, 0);

		/*******根据下游节点的路由路径，更新上游节点的路由路径***********/
		int i, item_r;
		U8 dest, hop;
		
		U8 node_cnt = *(data + pos);	
		pos += node_cnt + 1;	
		
		item_r = (int)*(data + pos++);	

		for(i = 0; i < item_r; i++)
		{
		    //该条路由路径目的节点
			dest = *(data + pos++);
			
	        //到目的节点dest跳数
			hop = *(data + pos++);
	        //若达到最大跳数才检查路由环路？
			if (RP_INHOPS == hop)
			{
			    //检查路由环路，若存在则清空路由
			    //本函数内嵌入跟更新转发表部分*****
				ritem_del(&rt.item[MR_AD2IN(dest)], dst);
			}
			else
			{
				if ((hop > MAX_HOPS)||(pos + hop*sizeof(MADR) > len))
				{
					EPT(stderr, "! Wrong uip message dest=%d,hop=%d,len=%d\n", dest, hop, len);
					break;
				}
				//将dst(下游节点)作为下一跳更新路由并比较，若更优则替换
				ritem_up(&rt.item[MR_AD2IN(dest)], dst, hop, (MADR*)(data+pos));
				ritem_fsm(&rt.item[MR_AD2IN(dest)], 0);
				pos += hop*sizeof(MADR);
			}
		}
		
		/********向下游节点发送确认报文**********/
		rp_ul_ack_gen(data);
	}
	
	else
	{
		U8 node_cnt = data[pos];
		pos ++;

		U8* path;
		path = data + pos;
		inform_uni_link(src, dst, status, node_cnt, len, path);
	}
}

/*收到uibp报文处理函数*/
void rp_fhruibp_proc(MADR node, int len, U8 *data)
{
	int pos = 0;
	//忽略第一个data[0]:node
	pos += sizeof(MADR);
	U8 icnt = data[pos];
	
	//先默认icnt为1，即一个uibp包只包含一个单向链路
	if(icnt != 1)
		EPT(stderr,"! ! icnt = %d\n\n", icnt);

	pos ++;
	U8 src = data[pos];

	pos += sizeof(MADR);
	U8 dst = data[pos];

	pos += sizeof(MADR);
	U8 status = data[pos];

	pos++;
	U8 node_cnt = data[pos];

	pos++;
	U8 *path;
	path = data + pos;

	if(src == *sa)
	{
		//根据收到的单向链路的状态更新上游节点的出链路状态
		update_fl(dst, status);
		//赋值一条到下游节点的路由（1跳），并与原到下游节点的路由比较，更优则更新之
		
		EPT(stderr,"*** uibp driver update the rpath to %d: ***\n", dst);
		
		ritem_nup(dst, NULL, 0);
		//向下游节点发送确认报文
		rp_ul_ack_gen(data);
	}
	else
	{
		inform_uni_link(src, dst, status, node_cnt, len, path);
	}
}

/*收到ul_ack报文处理函数*/
void rp_fhrulack_proc(MADR node, int len, U8 *data)
{	

	U8 src = *data;
	U8 dst = *(data + 1);
	U8 status = *(data + 2);

	if(dst != *sa)
	{
		EPT(stderr,"! Recv ul_ack while dst(%d) != sa(%d)\n", dst, *sa);
		return;
	}
	if(data[4] != *sa)
	{
		EPT(stderr,"! Recv ul_ack while node[0](%d) != sa(%d)\n", data[3], *sa);
		return;
	}

	confirm_ul(src, status);


	rpath_t *rp = &rt.item[MR_AD2IN(dst)].pfst;
	if(! WH_RP_VALD(rp->status))
	{
		U8 node_cnt = *(data + 3);
		rp->hop = node_cnt - 1;
		rp->status = status;
		memcpy(rp->node, data + 5, (node_cnt - 1)*sizeof(MADR));

		EPT(stderr, "** ul_ack make: **\n");
		rpath_show(src, rp);
	}


#ifndef _MR_TEST
	//对比路由表，更新转发表，如果转发表有变化，则通知底层
	update_fwt();
#endif
}


void rp_fhrrii_proc(MADR node, int len, U8 *data)
{
}

void rp_fhrrir_proc(MADR node, int len, U8 *data)
{
}

