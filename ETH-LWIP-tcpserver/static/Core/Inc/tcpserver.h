#ifndef __TCP_SERVER_H
#define __TCP_SERVER_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "main.h"

int tcp_server_start(uint16_t port);
int transfer_data(void);

#ifdef __cplusplus
}
#endif
#endif /* __TCP_SERVER_H */
