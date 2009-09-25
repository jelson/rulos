#ifndef _notifyifc_h
#define _notifyifc_h

struct s_notify_ifc;

typedef void (*NotifyFunc)(struct s_notify_ifc *);

typedef struct s_notify_ifc {
	NotifyFunc func;
} NotifyIfc;

#endif // _notifyifc_h
