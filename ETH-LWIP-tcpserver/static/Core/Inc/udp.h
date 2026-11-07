#ifndef __UDP_H
#define __UDP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * 启动UDP（绑定本地端口开始监听）
 * @param  port 本地端口号
 * @return      成功返回0
 */
int udp_user_start(uint16_t port);

/**
 * 主动向指定地址发送UDP数据
 * @param  ip_str 目标IP字符串，例如 "192.168.1.100"
 * @param  port   目标端口
 * @return        成功返回发送的字节数，失败返回负数
 */
int udp_user_sendto(const char *ip_str, uint16_t port, const void *buf, uint16_t len);

/**
 * 轮询函数，放在main函数的while(1)中：把收到的数据原样回发给对端
 */
void udp_transfer_data(void);

#ifdef __cplusplus
}
#endif

#endif /* __UDP_H */
