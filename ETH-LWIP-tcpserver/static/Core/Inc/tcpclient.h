#ifndef __TCPCLIENT_H
#define __TCPCLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * 启动TCP客户端（非阻塞，实际连接在 tcp_client_poll 中发起，支持自动重连）
 * @param  ip_str 服务器IP字符串，例如 "192.168.1.100"
 * @param  port   服务器端口
 * @return        成功返回0
 */
int tcp_client_start(const char *ip_str, uint16_t port);

/** 停止客户端并断开连接 */
void tcp_client_stop(void);

/**
 * TCP客户端发送数据
 * @return 实际写入发送缓冲的字节数
 */
int tcp_client_send(const void *buf, uint32_t len);

/**
 * 轮询函数，放在main函数的while(1)中，与MX_LWIP_Process()一起调用
 * 功能：自动连接/重连、把收到的数据原样回发
 */
void tcp_client_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __TCPCLIENT_H */
