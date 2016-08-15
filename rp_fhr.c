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
	U8 items, hop, status;
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
		dest = *(MADR)(data + tmp_pos);
		tmp_pos += sizeof(MADR);
		if(dest != *sa)
		{
			tmp_pos ++;
			continue;
		}
		else
		{
			status = *(data + tmp_pos++);
			if(status >= LQ_UNSTABLE)
			{
				nt.fl[MR_AD2IN(src)].lstatus = status;

	  			//��ֵһ�����ڽӵ�src��·�ɣ�srcΪ��һ��������ԭ���Ƚϣ������������֮
				ritem_nup(src, NULL, 0);
				//���͸���һ��·����·���ڶ�������up=0˵�������ݰ����¶����Ƕ�ʱ������
				//�������ڲ�Ƕ�����ת������*****
				ritem_fsm(&rt.item[MR_AD2IN(src)], 0);
			}
			else
				EPT(stderr,"ERROR: node from %d to %d, status:%d\n",dest, *sa, status);
			break;
		}
	}

	if(i == item_l)
	{
		EPT(stderr,"~ ~ ~ find a uni_link %d -> %d\n", src, *sa);
		inform_uni_link(src, 0);
		//��Ϊ���ڵ�sa������src������src��·��Ҳ��û�вο���ֵ��(soֱ��return)
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
		EPT(stderr, "node[%d]: the sop message len is wrong, items=%d\n", *sa, items);
	}
    //�Ա�·�ɱ�����ת�������ת�����б仯����֪ͨ�ײ�
    update_fwt();
}

/*�յ�uip���Ĵ�����*/
void rp_fhruip_proc(MADR node, int len, U8 *data)
{
	int pos = 0;
	MADR src = *(MADR *)data;
	pos += sizeof(MADR);
	
	MADR dst = *(MADR *)data;
	pos += sizeof(MADR);
	
	U8 status = *(data + pos);

	if(src == *sa)
	{
		//�����յ��ĵ�����·��״̬�������νڵ�ĳ���·״̬
		updata_fl(dst, status);
		//��ֵһ�������νڵ��·�ɣ�1����������ԭ�����νڵ��·�ɱȽϣ����������֮
		ritem_nup(dst, NULL, 0);
		//�����νڵ㷢��ȷ�ϱ���
		rp_ul_ack_gen(data + pos);
	}
	else
	{
		inform_uni_link(src, status);
	}
}

void rp_fhruibp_proc(MADR node, int len, U8 *data)
{
	
}

void rp_fhrulack_proc(MADR node, int len, U8 *data)
{
	
}




void rp_fhrrii_proc(MADR node, int len, U8 *data)
{
}

void rp_fhrrir_proc(MADR node, int len, U8 *data)
{
}

