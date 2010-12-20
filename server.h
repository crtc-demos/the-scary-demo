#ifndef SERVER_H
#define SERVER_H 1

extern int srv_wait_for_connection (void);
extern int srv_printf (const char *fmt, ...);
extern void srv_disconnect (void);

#endif
