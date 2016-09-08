#include "mr_common.h"

#ifdef _MR_TEST
qinfo_t  qinfs[] = {
	{PNAME_ROUTINGP, SN_ROUTINGP, -1, -1},
	{PNAME_MR_TEST,  SN_MRTEST, -1, -1}
};
#else
qinfo_t  qinfs[] = {
	{PNAME_NETLAYER, SN_NETPROC, -1, -1},
	{PNAME_HIGHMAC,  SN_HIGHMAC, -1, -1},
	{PNAME_ROUTINGP, SN_ROUTINGP, -1, -1},
	{PNAME_IF2TCPIP, SN_IF2TCPIP, -1, -1},
	{PNAME_MR_TEST,  SN_MRTEST, -1, -1}
};
#endif
const int cnt_p = sizeof(qinfs)/sizeof(qinfs[0]);

int qs = 0;
int re_qin= -1;
int nl_qid = -1;
int hm_qid = -1;
int vi_qid = -1;
int rp_qid = -1;
int mt_qid = -1;

#ifdef _MR_TEST
void mr_queues_init(void *arg)//获得route进程的队列id和test进程的队列id
{
	int i,j;
	int qid, rval, stop;
	MADR node = *(MADR *)arg;


//	pthread_detach(pthread_self());

	for (i = 0; i < cnt_p; i++)
	{
		if (NULL != strstr(qinfs[i].pname, PNAME_ROUTINGP))
		{
			re_qin = i;
			qinfs[i].key_q = ftok(PATH_CREATE_KEY, qinfs[i].sub + SN_MRTEST + node);
			//EPT(stderr, "process:%s, key_q:%x\n", qinfs[i].pname, qinfs[i].key_q);
		}
		else if (NULL != strstr(qinfs[i].pname, PNAME_MR_TEST))
		{
			//re_qin = i;
			qinfs[i].key_q = ftok(PATH_CREATE_KEY, qinfs[i].sub);
												//sub:SN_MRTEST 100
		}
	}

	if (-1 == re_qin)
	{
		rval = 1;
		EPT(stderr, "%s: can not create the key of myself queue\n", qinfs[re_qin].pname);
		goto thread_return;
	}

	stop = 0;
	while (0 == stop)
	{
		qs = 0;
		for (i = 0; i < cnt_p; i++)
		{
			if (-1 == qinfs[i].key_q)
				continue;
			qid = msgget(qinfs[i].key_q, IPC_CREAT|QUEUE_MODE);
			if (qid == -1)
				EPT(stderr, "%s: can not get queue for %s\n", qinfs[re_qin].pname, qinfs[i].pname);
			else
			{
//				EPT(stderr, "process:%s, qid:%d\n", qinfs[i].pname, qid);
				qinfs[i].qid = qid;
				qs += 1;

				/* set the qid */
				if (NULL != strstr(PNAME_MR_TEST, qinfs[i].pname)) {
					mt_qid = qid;
				}
				else if (NULL != strstr(PNAME_ROUTINGP, qinfs[i].pname)) {
					rp_qid = qid;
				}
			}
		}

		if (qs == cnt_p)
		{
			rval = 0;
			stop = 1;
		}
		else {
			sleep(2);
		}
	}

	EPT(stderr, "exit from finding queue thread.\n");

thread_return:
	pthread_exit(&rval);
}
#else
void mr_queues_init(void *arg)
{
	int i,j;
	int qid, rval, stop;
	char *name;

//	pthread_detach(pthread_self());
	name = (char *)arg;

	for (i = 0; i < cnt_p; i++)
	{
		qinfs[i].key_q = ftok(PATH_CREATE_KEY, qinfs[i].sub);
//		EPT(stderr, "process:%s, key_q:%x\n", qinfs[i].pname, qinfs[i].key_q);
		if (NULL != strstr(name, qinfs[i].pname))
		{
			re_qin = i;
		}
	}

	if (-1 == re_qin)
	{
		rval = 1;
		EPT(stderr, "%s: can not create the key of myself queue\n", name);
		goto thread_return;
	}

	stop = 0;
	while (0 == stop)
	{
		qs = 0;
		for (i = 0; i < cnt_p; i++)
		{
			qid = msgget(qinfs[i].key_q, IPC_CREAT|QUEUE_MODE);
			if (qid == -1)
				EPT(stderr, "%s: can not get queue for %s\n", name, qinfs[i].pname);
			else {
				EPT(stderr, "process:%s, qid:%d\n", qinfs[i].pname, qid);
				qinfs[i].qid = qid;
				qs += 1;

				/* set the qid */
				if (NULL != strstr(PNAME_NETLAYER, qinfs[i].pname)) {
					nl_qid = qid;
				}
				else if (NULL != strstr(PNAME_ROUTINGP, qinfs[i].pname)) {
					rp_qid = qid;
				}
				else if (NULL != strstr(PNAME_IF2TCPIP, qinfs[i].pname)) {
					vi_qid = qid;
				}
				else if (NULL != strstr(PNAME_HIGHMAC, qinfs[i].pname)) {
					hm_qid = qid;
				}
			}

			if (i == re_qin) {
				rval = 2;
				stop = 1;
			}
		}

		if (qs == cnt_p) {
			rval = 0;
			stop = 1;
		}
		else {
			sleep(2);
		}
	}

	EPT(stderr, "exit from finding queue thread.\n");

thread_return:
	pthread_exit(&rval);
}
#endif

int mr_queues_delete()
{
	int i;

	for (i = 0; i < cnt_p; i++)
	{
		if (qinfs[i].qid != -1);
			msgctl(qinfs[i].qid, IPC_RMID, NULL);
	}

	return 0;
}

