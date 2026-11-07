#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "main.h"
#include "lwip/udp.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

// ============================================================================================
// 配置
//

#define UDP_RX_LEN   1472     /* 以太网MTU 1500 - IP头20 - UDP头8 */

#define DEBUG
#ifdef DEBUG
#define debug(fmt, ...) do{printf(fmt, ##__VA_ARGS__);}while(0)
#else
#define debug(fmt, ...) do{;}while(0)
#endif

// ============================================================================================
// 私有变量
//

static struct udp_pcb *upcb = NULL;

static uint8_t   rx_buf[UDP_RX_LEN];
static uint16_t  rx_len = 0;          /* 0表示缓存空闲 */
static ip_addr_t peer_ip;             /* 最近一次发数据过来的对端 */
static uint16_t  peer_port = 0;

// ============================================================================================
// 私有函数
//

static err_t udp_send_pbuf(const ip_addr_t *ip, uint16_t port, const void *buf, uint16_t len)
{
	struct pbuf *p;
	err_t err;

	if (upcb == NULL)
		return ERR_CONN;
	p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
	if (p == NULL)
		return ERR_MEM;
	pbuf_take(p, buf, len);
	err = udp_sendto(upcb, p, ip, port);
	pbuf_free(p);
	return err;
}

static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port)
{
	uint16_t n;

	if (p == NULL)
		return;

	if (rx_len == 0)   /* 缓存空闲才接收，否则丢弃这个包 */
	{
		n = (p->tot_len > UDP_RX_LEN) ? UDP_RX_LEN : p->tot_len;
		pbuf_copy_partial(p, rx_buf, n, 0);
		rx_len = n;
		ip_addr_copy(peer_ip, *addr);
		peer_port = port;
		debug("\r\n udp recv %d bytes from %s:%d.", n, ipaddr_ntoa(addr), port);
	}
	else
	{
		debug("\r\n udp rx buffer busy, packet dropped.");
	}
	pbuf_free(p);   /* 必须释放 */
}

// ============================================================================================
// 对外接口
//

/**
 * 启动UDP（绑定本地端口开始监听）
 * @param  port 本地端口号
 * @return      成功返回0
 */
int udp_user_start(uint16_t port)
{
	err_t err;

	upcb = udp_new();
	if (upcb == NULL)
	{
		debug("\r\n udp_new failed, out of memory.");
		return -1;
	}
	err = udp_bind(upcb, IP_ADDR_ANY, port);
	if (err != ERR_OK)
	{
		debug("\r\n udp_bind port %d failed:%d.", port, err);
		udp_remove(upcb);
		upcb = NULL;
		return -2;
	}
	udp_recv(upcb, udp_recv_callback, NULL);
	debug("\r\n UDP started @ port %d.", port);
	return 0;
}

/**
 * 主动向指定地址发送UDP数据
 * @param  ip_str 目标IP字符串，例如 "192.168.1.100"
 * @param  port   目标端口
 * @return        成功返回发送的字节数，失败返回负数
 */
int udp_user_sendto(const char *ip_str, uint16_t port, const void *buf, uint16_t len)
{
	ip_addr_t ip;

	if (!ipaddr_aton(ip_str, &ip))
		return -1;
	if (udp_send_pbuf(&ip, port, buf, len) != ERR_OK)
		return -2;
	return len;
}

/**
 * 轮询函数，放在main函数的while(1)中：把收到的数据原样回发给对端
 */
void udp_transfer_data(void)
{
	err_t err;

	if (rx_len > 0)
	{
		err = udp_send_pbuf(&peer_ip, peer_port, rx_buf, rx_len);
		if (err == ERR_OK)
		{
			debug("\r\n udp send %d bytes success.", rx_len);
			rx_len = 0;
		}
		else if (err != ERR_MEM)   /* ERR_MEM只是暂时没内存，保留数据下次重试；其他错误直接丢弃 */
		{
			debug("\r\n udp send failed:%d.", err);
			rx_len = 0;
		}
	}
}
