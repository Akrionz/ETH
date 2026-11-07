#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "lwip/memp.h"
#include "lwip/inet.h"


// ============================================================================================
// 回调函数控制宏 
// 

#define USE_ERROR_CALLBACK 1
#define USE_SENT_CALLBACK 0
#define USE_POLL_CALLBACK 0



// ============================================================================================
// 调试信息输出
// 

#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) do{printf(fmt, ##__VA_ARGS__);}while(0)
#else
#define debug(fmt, ...) do{;}while(0)
#endif


// ============================================================================================
// tcp接收发送缓存
// 

#define TCP_RX_LEN  8192
uint8_t TCP_RX_BUF[TCP_RX_LEN];
volatile uint16_t TCP_RX_STA = 0;  /* bit15：有无数据标志位        bit14-0:数据量计数 */



// TCP结构句柄 
struct tcp_pcb* tcppcb = NULL;

// ============================================================================================
// 私有函数预声明
// 

static void tcp_server_disconnect(struct tcp_pcb *tpcb);
static uint32_t tcp_server_send(struct tcp_pcb *tpcb, const void* buf, uint32_t len);



// ============================================================================================
// 私有函数实现
// 


#if (USE_ERROR_CALLBACK == 1)
static void error_callback(void *arg, err_t err)
{
	debug("\r\n error_callback:%d.", err);
	switch(err)
	{
	/* PC上位机如果正常运行中闪退或者不良退出会出现这个错误，此时服务器需要释放掉连接  */
	case ERR_RST:
	tcp_server_disconnect(tcppcb);
	break;
	default:break;
	}
}

#endif

static err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	if (p == NULL) /* 接收到空包表示对方断开连接 */
	{
		tcp_server_disconnect(tpcb);
		err = ERR_CLSD;
	}
	else if (err != ERR_OK) /* 收到非空包但是出现错误 */
	{
		pbuf_free(p);
	}
	else /* 接收数据正常，遍历pbuf拷贝出接收到的数据 */
	{
		struct pbuf* it = p;

		if ((TCP_RX_STA & 0x8000) == 0) /* 当前缓存为空  */
		{
			for (it = p; it != NULL; it = it->next)
			{
				if (TCP_RX_STA + it->len > TCP_RX_LEN) /* 缓存满了 */
					break;
				memcpy(TCP_RX_BUF + TCP_RX_STA, it->payload, it->len);  /* 将接收到的数据拷贝到自己的缓存中  */
				TCP_RX_STA += it->len;
			}
			TCP_RX_STA |= 0x8000;    /* 标记有数据收到 */
		}
		tcp_recved(tpcb, p->tot_len); /* 滑动TCP窗口 */
		pbuf_free(p); /* 释放pbuf */
	}
	return err;
}


static err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	if (tcppcb == NULL)
	{
		if (err == ERR_OK)
		{
			tcppcb = newpcb;
			tcp_arg(newpcb, NULL);
			tcp_recv(newpcb, recv_callback);

#if (USE_ERROR_CALLBACK == 1)
			tcp_err(newpcb, error_callback);
#endif

#if (USE_SENT_CALLBACK == 1)
			tcp_sent(newpcb, sent_callback);
#endif

#if (USE_POLL_CALLBACK == 1)
			tcp_poll(newpcb, poll_callback, 1);
#endif
			debug("\r\n %s:%d connect.", inet_ntoa(newpcb->remote_ip), newpcb->remote_port);
		}
		else
		{
			tcp_server_disconnect(newpcb);
		}
	}
	else
	{
		tcp_abort(newpcb);
		debug("\r\n already connected. ");
	}
	return err;
}

static void tcp_server_disconnect(struct tcp_pcb *tpcb)
{
	tcp_arg(tpcb, NULL);
	tcp_recv(tpcb, NULL);

#if (USE_SENT_CALLBACK == 1)
	tcp_sent(tpcb, NULL);
#endif

#if (USE_POLL_CALLBACK == 1)
	tcp_poll(tpcb, NULL, 0);
#endif

#if (USE_ERROR_CALLBACK == 1)
	tcp_err(tpcb, NULL);
#endif

	tcp_abort(tpcb); /* 关闭连接并释放tpcb控制块 */
	tcppcb = NULL;
	debug("\r\n disconnected.");
}

static uint32_t tcp_server_send(struct tcp_pcb *tpcb, const void* buf, uint32_t len)
{
	uint32_t nwrite = 0, total = 0;
	const uint8_t* p = (const uint8_t *) buf;
	err_t err = ERR_OK;
	if (!tpcb)
		return 0;
	while ((err == ERR_OK) && (len != 0) && (tcp_sndbuf(tpcb) > 0))
	{
		nwrite = tcp_sndbuf(tpcb) >= len ? len : tcp_sndbuf(tpcb);
		err = tcp_write(tpcb, p, nwrite, 1);
		if (err == ERR_OK)
		{
			len -= nwrite;
			total += nwrite;
			p += nwrite;
		}
		tcp_output(tpcb);
	}
	return total;
}



//extern int tcp_server_start(uint16_t port);
//extern int user_senddata(const void* buf,uint32_t len);
//extern int transfer_data();


/**
 * 启动TCP服务器
 * @param  port 本地端口号
 * @return      成功返回0
 */
int tcp_server_start(uint16_t port)
{
	int ret = 0;
	struct tcp_pcb* pcb = NULL;
	err_t err = ERR_OK;

	/* create new TCP PCB structure */
	pcb = tcp_new();
	if (!pcb)
	{
		debug("Error creating PCB. Out of Memory\n\r");
		ret = -1;
		goto __exit;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ADDR_ANY, port);
	if (err != ERR_OK)
	{
		debug("Unable to bind to port %d: err = %d\n\r", port, err);
		ret = -2;
		goto __exit;
	}

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb)
	{
		debug("Out of memory while tcp_listen\n\r");
		ret = -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	/* create success */
	debug("TCP echo server started @ port %d\n\r", port);
	return ret;

__exit:
	if (pcb)
		memp_free(MEMP_TCP_PCB, pcb);
	return ret;
}

/**
 * TCP发送数据
 * @param  buf 待发送的数据
 * @param  len 数据长度
 * @return     返回实际发送的字节数
 */
int user_senddata(const void* buf, uint32_t len)
{
	return tcp_server_send(tcppcb, buf, len);
}

/**
 * 轮询函数，放置于main函数的while死循环中
 * @return 无
 */
int transfer_data(void)
{
	uint32_t nsend = 0;
	if (tcppcb != NULL && tcppcb->state == ESTABLISHED) /* 连接有效 */
	{
		if (TCP_RX_STA & 0x8000)   /* 有数据收到  */
		{
			nsend = user_senddata(TCP_RX_BUF, TCP_RX_STA & 0x7FFF);    /* 将接收到的数据发回去  */
			TCP_RX_STA = 0;
			debug("\r\n send %d bytes success.", nsend);
		}
	}
	return 0;
}
