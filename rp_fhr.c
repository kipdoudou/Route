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

//�����data����Ϣ����data������psh����ʼ��ַ��len������phd->len����psh+item[n]�ĳ���
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
    //src�ǽ���sop���������ڽӵ�
	MADR src = *(MADR *)data;
	//pos�Ƕ�ָ��λ��
	pos += sizeof(MADR);
	ASSERT(src == node);
	//�ھӱ�src�����ڵ����·�����ڵ��ھ����������·���յ��İ���+1�����ھ�����·״̬�ĸı�
	rlink_inc(src);
	//�����յ��İ���������·״̬ת�ƣ�����src�ڵ������·״̬���ڶ�������Ϊ0˵����sop���������,�������յ�����
	//�����µ�״̬���ڵ�ǰ״̬�����Ը���״̬�滻Ϊ��ǰ״̬����ǰ״̬�滻Ϊ��״̬�������滻
	rlink_fsm(src, 0);

	/* drop the message of LQ_NULL or LQ_EXPIRE */
	if (!WH_NL_FEAS(nt.rl[MR_AD2IN(src)].lstatus))
	{
	    //WH_NL_FEAS��·״̬Ϊ��Ծ���߲��ȶ�����������������������
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

	  			//��ֵһ�����ڽӵ�src��·�ɣ�srcΪ��һ��������ԭ���Ƚϣ������������֮
				
				EPT(stderr,"*** sop driver update the rpath to %d: ***\n", dest);
				
				ritem_nup(src, NULL, 0);
				//���͸���һ��·����·���ڶ�������up=0˵�������ݰ����¶����Ƕ�ʱ������
				//�������ڲ�Ƕ�����ת������*****
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
		//��Ϊ���ڵ�sa������src������src��·��Ҳ��û�вο���ֵ��(soֱ��return)
		
		EPT(stderr,"Sop's content non-use\n");
		return;
	}

	pos += item_l*(sizeof(MADR) + 1);

    //�����Ǹ���sop����ͷ����sop����������������һ���ڽڵ��·��·�������濪ʼ��ȡsop����item����

	
    //EPT(stderr, "sop message: items=%d\n", items);

	for(i = 0; i < item_r; i++)
	{
	    //����·��·��Ŀ�Ľڵ�
		dest = *(MADR *)(data + pos);
		pos += sizeof(MADR);
        //��Ŀ�Ľڵ�dest����
		hop = *(data + pos++);
        //���ﵽ��������ż��·�ɻ�·��
		if (RP_INHOPS == hop)
		{
		    //���·�ɻ�·�������������·��
		    //��������Ƕ�������ת������*****
			ritem_del(&rt.item[MR_AD2IN(dest)], src);
		}
		else
		{
			if ((hop > MAX_HOPS)||(pos + hop*sizeof(MADR) > len))
			{
				EPT(stderr, "wrong sop message dest=%d,hop=%d,len=%d\n", dest, hop, len);
				break;
			}
			//��src��Ϊ��һ������·�ɲ��Ƚϣ����������滻
			ritem_up(&rt.item[MR_AD2IN(dest)], src, hop, (MADR*)(data+pos));
			ritem_fsm(&rt.item[MR_AD2IN(dest)], 0);
			pos += hop*sizeof(MADR);
		}
	}

	if (pos != len) {
		EPT(stderr, "node[%d]: the sop message len(%d) is wrong, item_l=%d item_r=%d\n", *sa, len, item_l, item_r);
	}
    
    
#ifndef _MR_TEST
	//�Ա�·�ɱ�����ת�������ת�����б仯����֪ͨ�ײ�
	update_fwt();
#endif
}

/*�յ�uip���Ĵ�����*/
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
		
		/*����������Ӧ��Ҳ��¼�¸õ�����·��Ȼ������ظ�����*/

		//�����յ��ĵ�����·��״̬�������νڵ�ĳ���·״̬
		update_fl(dst, status);
		//��ֵһ�������νڵ�(dst)��·�ɣ�1����������ԭ�����νڵ��·�ɱȽϣ����������֮
		
		EPT(stderr,"*** uip driver update the rpath to %d: ***\n", dst);
		
		ritem_nup(dst, NULL, 0);

		/*******�������νڵ��·��·�����������νڵ��·��·��***********/
		int i, item_r;
		U8 dest, hop;
		
		U8 node_cnt = *(data + pos);	
		pos += node_cnt + 1;	
		
		item_r = (int)*(data + pos++);	

		for(i = 0; i < item_r; i++)
		{
		    //����·��·��Ŀ�Ľڵ�
			dest = *(data + pos++);
			
	        //��Ŀ�Ľڵ�dest����
			hop = *(data + pos++);
	        //���ﵽ��������ż��·�ɻ�·��
			if (RP_INHOPS == hop)
			{
			    //���·�ɻ�·�������������·��
			    //��������Ƕ�������ת������*****
				ritem_del(&rt.item[MR_AD2IN(dest)], dst);
			}
			else
			{
				if ((hop > MAX_HOPS)||(pos + hop*sizeof(MADR) > len))
				{
					EPT(stderr, "! Wrong uip message dest=%d,hop=%d,len=%d\n", dest, hop, len);
					break;
				}
				//��dst(���νڵ�)��Ϊ��һ������·�ɲ��Ƚϣ����������滻
				ritem_up(&rt.item[MR_AD2IN(dest)], dst, hop, (MADR*)(data+pos));
				ritem_fsm(&rt.item[MR_AD2IN(dest)], 0);
				pos += hop*sizeof(MADR);
			}
		}
		
		/********�����νڵ㷢��ȷ�ϱ���**********/
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

/*�յ�uibp���Ĵ�����*/
void rp_fhruibp_proc(MADR node, int len, U8 *data)
{
	int pos = 0;
	//���Ե�һ��data[0]:node
	pos += sizeof(MADR);
	U8 icnt = data[pos];
	
	//��Ĭ��icntΪ1����һ��uibp��ֻ����һ��������·
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
		//�����յ��ĵ�����·��״̬�������νڵ�ĳ���·״̬
		update_fl(dst, status);
		//��ֵһ�������νڵ��·�ɣ�1����������ԭ�����νڵ��·�ɱȽϣ����������֮
		
		EPT(stderr,"*** uibp driver update the rpath to %d: ***\n", dst);
		
		ritem_nup(dst, NULL, 0);
		//�����νڵ㷢��ȷ�ϱ���
		rp_ul_ack_gen(data);
	}
	else
	{
		inform_uni_link(src, dst, status, node_cnt, len, path);
	}
}

/*�յ�ul_ack���Ĵ�����*/
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
	//�Ա�·�ɱ�����ת�������ת�����б仯����֪ͨ�ײ�
	update_fwt();
#endif
}


void rp_fhrrii_proc(MADR node, int len, U8 *data)
{
}

void rp_fhrrir_proc(MADR node, int len, U8 *data)
{
}

