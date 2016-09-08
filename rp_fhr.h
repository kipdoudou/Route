#ifndef _RP_FHR_H
#define _RP_FHR_H
#include "mr_common.h"
#include "rp_common.h"

/* sop message header */
typedef struct _sop_hd {
	MADR	node;
	U8		icnt_l;
	U8 		icnt_r;
} sop_hd;


void rp_fhrmsg_disp(MADR, int, int, U8*);

void rp_fhruip_proc(MADR, int, U8*);
void rp_fhruibp_proc(MADR, int, U8*);
void rp_fhrulack_proc(MADR, int, U8*);

void rp_fhrsop_proc(MADR, int, U8*);
void rp_fhrrii_proc(MADR, int, U8*);
void rp_fhrrir_proc(MADR, int, U8*);

#endif
