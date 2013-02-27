#ifndef _WIFI_EVENT_
#define _WIFI_EVENT_

#define EI_IS_FIRST(ei) (ei.value == NULL)

struct event_iter
{
	void 	*stream;
	__u16	length;
	void 	*event;
	void 	*value;
};

void event_iter_init(struct event_iter *iter, struct iwreq *iwr);
void event_iter_term(struct event_iter *evi);

/* 0 = success, 1 = finish, -1 = error */
int event_iter_next(struct event_iter *evi, struct iw_event *iwe);

#endif
