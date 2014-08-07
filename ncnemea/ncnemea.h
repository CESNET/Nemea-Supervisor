#define MODULE_EVENT_STARTED 	1
#define MODULE_EVENT_STOPPED 	2
#define MODULE_EVENT_RESTARTED 	3
#define MODULE_EVENT_DISABLED 	4

extern void nc_notify(int module_event, const char * module_name);