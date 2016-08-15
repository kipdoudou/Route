#ifndef _RP_TIMER_H
#define _RP_TIMER_H

/* the parameter of signal timer */
#define RP_TDELAY_S		1			/* second */
#define RP_TDELAY_US	0			/* us 0~999999 */
#define RP_TINTVL_S		0			/* second */
#define RP_TINTVL_US	200000		/* us 0~999999 */

#define RT_ITEM_EXPI	5000		/* ms, expiration time of routing item */
#define RT_SOP_INTV		1000		/* ms, interval of sop message */
#define RT_LINK_CHK		5000		/* ms, expiration time of link maintain */

#define SOP_CNT_

void rp_tsch_init();
int  rp_start_timer();
void rp_timer_sche(int);

void rp_sop_gen(void*);
void rp_rt_check(void*);
void rp_lk_check(void*);


#endif
