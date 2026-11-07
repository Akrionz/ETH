#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "main.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"

// ============================================================================================
// 配置
//

#define TCPC_RX_LEN        2048      /* 接收缓存大小 */
#define TCPC_RECONNECT_MS  3000      /* 断线/连接失败后重连间隔 */

#define DEBUG
#ifdef DEBUG
#define debug(fmt, ...) do{printf(fmt, ##__VA_ARGS__);}while(0)
#else
#define debug(fmt, ...) do{;}while(0)
#endif

extern struct netif gnetif;

// ============================================================================================
// 私有变量
//

typedef enum
{
	TCPC_IDLE = 0,       /* 未连接 */
	TCPC_CONNECTING,     /* 正在连接 */
	TCPC_CONNECTED       /* 已连接 */
} tcpc_state_t;

static struct tcp_pcb *cpcb = NULL;
static ip_addr_t       remote_ip;
static uint16_t        remote_port = 0;
static uint8_t         enabled = 0;
static tcpc_state_t    state = TCPC_IDLE;
static uint32_t        last_try = 0;

static uint8_t  rx_buf[TCPC_RX_LEN];
static uint16_t rx_len = 0;     /* 缓存中有效数据的末尾 */
static uint16_t tx_off = 0;     /* 已经回发出去的位置   */

static void tcp_client_close(void);

// ============================================================================================
// 回调函数
//

/* 连接异常（超时、对端RST等），此时lwIP已经释放了pcb，不能再调用tcp_close/tcp_abort */
static void client_err_callback(void *arg, err_t err)
{
	debug("\r\n client error_callback:%d.", err);
	cpcb = NULL;
	state = TCPC_IDLE;
	last_try = HAL_GetTick();
}

static err_t client_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	uint16_t n;

	if (p == NULL)  /* 对端关闭连接 */
	{
		debug("\r\n server closed the connection.");
		tcp_client_close();
		return ERR_OK;
	}
	if (err != ERR_OK)
	{
		pbuf_free(p);
		return err;
	}

	/* 追加到缓存，放不下的部分丢弃 */
	n = (p->tot_len > (TCPC_RX_LEN - rx_len)) ? (TCPC_RX_LEN - rx_len) : p->tot_len;
	if (n > 0)
	{
		pbuf_copy_partial(p, rx_buf + rx_len, n, 0);
		rx_len += n;
	}
	tcp_recved(tpcb, p->tot_len);   /* 滑动窗口 */
	pbuf_free(p);
	return ERR_OK;
}

static err_t client_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	if (err != ERR_OK)
	{
		tcp_client_close();
		return err;
	}
	state = TCPC_CONNECTED;
	rx_len = 0;
	tx_off = 0;
	debug("\r\n connected to %s:%d.", ipaddr_ntoa(&remote_ip), remote_port);
	return ERR_OK;
}

// ============================================================================================
// 私有函数
//

static void tcp_client_close(void)
{
	if (cpcb != NULL)
	{
		tcp_arg(cpcb, NULL);
		tcp_recv(cpcb, NULL);
		tcp_err(cpcb, NULL);
		if (tcp_close(cpcb) != ERR_OK)
			tcp_abort(cpcb);
		cpcb = NULL;
	}
	state = TCPC_IDLE;
	last_try = HAL_GetTick();
	debug("\r\n client disconnected.");
}

static void tcp_client_do_connect(void)
{
	err_t err;

	cpcb = tcp_new();
	if (cpcb == NULL)
	{
		debug("\r\n tcp_new failed, out of memory.");
		return;
	}
	tcp_arg(cpcb, NULL);
	tcp_recv(cpcb, client_recv_callback);
	tcp_err(cpcb, client_err_callback);

	state = TCPC_CONNECTING;
	err = tcp_connect(cpcb, &remote_ip, remote_port, client_connected_callback);
	if (err != ERR_OK)
	{
		debug("\r\n tcp_connect failed:%d.", err);
		tcp_abort(cpcb);
		cpcb = NULL;
		state = TCPC_IDLE;
	}
	else
	{
		debug("\r\n connecting to %s:%d ...", ipaddr_ntoa(&remote_ip), remote_port);
	}
}

// ============================================================================================
// 对外接口
//

/**
 * 启动TCP客户端（非阻塞，实际连接在 tcp_client_poll 中发起，并支持自动重连）
 * @param  ip_str 服务器IP字符串，例如 "192.168.1.100"
 * @param  port   服务器端口
 * @return        成功返回0
 */
int tcp_client_start(const char *ip_str, uint16_t port)
{
	if (!ipaddr_aton(ip_str, &remote_ip))
	{
		debug("\r\n invalid ip: %s", ip_str);
		return -1;
	}
	remote_port = port;
	enabled = 1;
	state = TCPC_IDLE;
	last_try = HAL_GetTick() - TCPC_RECONNECT_MS;   /* 第一次立刻尝试 */
	return 0;
}

/** 停止客户端并断开连接 */
void tcp_client_stop(void)
{
	enabled = 0;
	tcp_client_close();
}

/**
 * TCP客户端发送数据
 * @return 实际写入发送缓冲的字节数
 */
int tcp_client_send(const void *buf, uint32_t len)
{
	uint32_t n;

	if (cpcb == NULL || state != TCPC_CONNECTED)
		return 0;
	n = tcp_sndbuf(cpcb);
	if (n > len)
		n = len;
	if (n == 0)
		return 0;
	if (tcp_write(cpcb, buf, (u16_t)n, TCP_WRITE_FLAG_COPY) != ERR_OK)
		return 0;
	tcp_output(cpcb);
	return (int)n;
}

/**
 * 轮询函数，放在main函数的while(1)中，与MX_LWIP_Process()一起调用
 * 功能：自动连接/重连、把收到的数据原样回发
 */
void tcp_client_poll(void)
{
	uint32_t n;
	int sent;

	if (!enabled)
		return;

	if (state == TCPC_IDLE)
	{
		if ((HAL_GetTick() - last_try) >= TCPC_RECONNECT_MS)
		{
			last_try = HAL_GetTick();
			/* 网线已插且已经有IP，才发起连接 */
			if (netif_is_link_up(&gnetif) && !ip_addr_isany(&gnetif.ip_addr))
				tcp_client_do_connect();
		}
	}
	else if (state == TCPC_CONNECTED)
	{
		if (rx_len > tx_off)
		{
			n = rx_len - tx_off;
			sent = tcp_client_send(rx_buf + tx_off, n);
			if (sent > 0)
			{
				tx_off += sent;
				if (tx_off >= rx_len)    /* 全部回发完，缓存清空 */
				{
					debug("\r\n client send %d bytes success.", rx_len);
					rx_len = 0;
					tx_off = 0;
				}
			}
		}
	}
}
