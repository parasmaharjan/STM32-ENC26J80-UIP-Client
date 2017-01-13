#ifndef __TCP_DEMO_H__
#define __TCP_DEMO_H__	

#include "uipopt.h"

#define UIP_APPCALL tcp_appcall

struct tcp_demo_appstate
{
	u8_t state;
	u8_t *textptr;
	int textlen;
};	 
typedef struct tcp_demo_appstate uip_tcp_appstate_t;

#endif
