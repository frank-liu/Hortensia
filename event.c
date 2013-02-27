#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>

#include "event.h"

struct token_info
{
	__u16	pk_len;
	__u8	is_point;
	__u8	reserve;
};

static const struct token_info ioctl_info[] = 
{
	[IW_IOCTL_IDX(SIOCGIWAP)]	= {IW_EV_ADDR_PK_LEN, 	0, 0},
	[IW_IOCTL_IDX(SIOCGIWFREQ)] 	= {IW_EV_FREQ_PK_LEN, 	0, 0},
	[IW_IOCTL_IDX(SIOCGIWENCODE)] 	= {IW_EV_POINT_PK_LEN, 	1, 0},
	[IW_IOCTL_IDX(SIOCGIWESSID)] 	= {IW_EV_POINT_PK_LEN, 	1, 0},
	[IW_IOCTL_IDX(SIOCGIWRATE)] 	= {IW_EV_PARAM_PK_LEN, 	0, 0},
	[IW_IOCTL_IDX(SIOCGIWMODE)] 	= {IW_EV_UINT_PK_LEN, 	0, 0},
};

static const struct token_info event_info[] = 
{
	[IW_EVENT_IDX(IWEVQUAL)] 	= {IW_EV_QUAL_PK_LEN,	0, 0},
	[IW_EVENT_IDX(IWEVCUSTOM)] 	= {IW_EV_POINT_PK_LEN,	1, 0},
	[IW_EVENT_IDX(IWEVGENIE)] 	= {IW_EV_POINT_PK_LEN,	1, 0},
};

void event_iter_init(struct event_iter *evi, struct iwreq *iwr)
{
	evi->stream = iwr->u.data.pointer;
	evi->length = iwr->u.data.length;
	evi->event = NULL;
	evi->value = NULL;
}

void event_iter_term(struct event_iter *evi)
{
}

/*
 * Get event token
 * evi: [in/out]
 * iwe: [out]
 * @RETURN: 0 = success, 1 = finish, -1 = error
 */
int event_iter_next(struct event_iter *evi, struct iw_event *iwe)
{
	struct token_info *info = NULL;

	/* Check if parse finish */
	if (evi->event >= evi->stream + evi->length)
	{
		return 1;
	}

	/* First time to run this function */
	if (NULL == evi->event)
	{
		evi->event = evi->stream;
	}

	/* Get iw_event.len and iw_event.cmd */
	memcpy((void*)iwe, evi->event, IW_EV_LCP_PK_LEN);

	/* Get event token's info */
	if (iwe->cmd > SIOCIWFIRST && iwe->cmd < SIOCIWLAST)
		info = (struct token_info*) &ioctl_info[IW_IOCTL_IDX(iwe->cmd)];
	else
		info = (struct token_info*) &event_info[IW_EVENT_IDX(iwe->cmd)];
	if (NULL == info || info->pk_len < IW_EV_LCP_PK_LEN)
	{
		evi->value = NULL;
		evi->event += iwe->len;
		return -1;
	}

	/* Calc address for value of this event */
	if (NULL == evi->value)
		evi->value = evi->event + IW_EV_LCP_PK_LEN; /* First value */

  	/* Special processing for iw_point events */
	if (info->is_point == 1)
	{
		memcpy((void*) &iwe->u + IW_EV_POINT_OFF, evi->value, info->pk_len - IW_EV_LCP_PK_LEN);
		iwe->u.data.pointer = evi->value + info->pk_len - IW_EV_LCP_PK_LEN;

		evi->value = NULL;
		evi->event += iwe->len;
	}
	else
	{
		memcpy((void*) &iwe->u, evi->value, info->pk_len - IW_EV_LCP_PK_LEN);

		/* Check if this event have next value */
		evi->value += info->pk_len - IW_EV_LCP_PK_LEN;
		if (evi->value >= (evi->event + iwe->len))
		{
			evi->value = NULL;
			evi->event += iwe->len;
		}
	}

	return 0;
}
