#include "librecursion.h"

int start_recursion(__rte_unused struct dns_query *query)
{
#if 0
	struct dns_question *question;
	
	if (query == NULL)
		return RET_R_FAIL;

	question = query->question;
	if (question == NULL)
		return RET_R_FAIL;
	if (question->qclass != DNS_CLASS_IN
		|| question->qtype != DNS_QTYPE_A 
		|| question->qtype != DNS_QTYPE_CNAME)
		return RET_R_UNSUPPORT;
#endif
    return 0;
}

