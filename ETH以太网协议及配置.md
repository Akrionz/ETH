

# ETH以太网协议及配置

> ##### ETH以太网：对于内部集成了以太网外设的MCU，它实际是一个通过 DMA 控制器进行介质访问控制(MAC)，它的功能就是实现 MAC层的任务。借助以太网外设，MCU通过ETH 外设按照 IEEE 802.3-2002 标准发送和接收 MAC 数据包。

## 1.ETH简介：

ETH 内部自带专用的 DMA 控制器用于 MAC，ETH 支持两个工业标准接口介质独立接口(MII)和简化介质独立接口(RMII)用于与外部 PHY芯片连接。

MII 和 RMII 接口用于 MAC 数据包传输，ETH 还集成了站管理接口(SMI)接口专门用于与外部 PHY 通信，用于访问 PHY 芯片寄存器。物理层定义了以太网使用的传输介质、传输速度、数据编码方式和冲突检测机制，PHY 芯片是物理层功能实现的实体，生活中常用水晶头网线+水晶头插座+PHY 组合构成了物理层。

ETH 有专用的 DMA 控制器，它通过 AHB 主从接口与内核和存储器相连，AHB 主接口用于控制数据传输，而 AHB 从接口用于访问“控制与状态寄存器”(CSR)空间。

**数据发送时，先将数据有存储器以 DMA 传输到发送 TX FIFO 进行缓冲，然后由 MAC 内核发送；**

**接收数据时，RX FIFO 先接收以太网数据帧，再由 DMA 传输至存储器。**

ETH系统框图如下图

![image-20261107084203586](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107084203586.png)

## 2.LWIP简介：

LwIP 是 Light Weight Internet Protocol 的缩写，是由瑞士计算机科学院 Adam Dunkels等开发的适用于嵌入式领域的开源轻量级 TCP/IP协议栈。它可以移植到含有操作系统的平台中，也可以在无操作系统的平台下运行。由于它开源、占用的 RAM 和 ROM 比较少、支持较为完整的TCP/IP 协议、且十分便于裁剪、调试，被广泛应用在中低端的 32 位控制器平台。

LWIP官网：

[https://savannah.nongnu.org/projects/lwip/]: 

## 3.LAN8720A简介

LAN8720A 是 SMSC 公司(已被 Microchip 公司收购)设计的一个体积小、功耗低、全能型 10/100Mbps 的以太网物理层(PHY)收发器。LAN8720A 总共只有 24Pin，仅支持 RMII 接口。

由它组成的网络结构见下图

![image-20261107084059331](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107084059331.png)

LAN8720A 通过 RMII 与 MAC 连接。RJ45 是网络插座，在与 LAN8720A 连接之间还需要一个变压器，所以一般使用带电压转换和 LED 指示灯的 HY911105A 型号的插座。一般来说，必须为使用 RMII 接口的 PHY 提供 50MHz 的时钟源输入到 REF_CLK 引脚，不过 LAN8720A 内部集成 PLL，可以将 25MHz 的时钟源陪频到 50MHz 并在指定引脚输出该时钟，所以我们可以直接使其与 REF_CLK 连接达到提供 50MHz 时钟的效果。

## 4.DHCP

### 4.1DHCP简介

DHCP（Dynamic Host Configuration Protocol，动态主机配置协议） 通常被应用在大型的局域网络环境中，主要作用是集中的管理、分配IP地址，使网络环境中的主机动态的获得IP地址、Gateway地址、DNS服务器地址等信息，并能够提升地址的使用率。DHCP协议采用客户端/服务器模型，主机地址的动态分配任务由网络主机驱动。当DHCP服务器接收到来自网络主机申请地址的信息时，才会向网络主机发送相关的地址配置等信息，以实现网络主机地址信息的动态配置。

**DHCP具有以下功能：**

保证任何IP地址在同一时刻只能由一台DHCP客户机所使用。

DHCP应当可以给用户分配永久固定的IP地址。

DHCP应当可以同用其他方法获得IP地址的主机共存（如手工配置IP地址的主机）。

DHCP服务器应当向现有的客户端提供服务。

**DHCP有三种机制分配IP地址：**

自动分配方式（Automatic Allocation），DHCP服务器为主机指定一个永久性的IP地址，一旦DHCP客户端第一次成功从DHCP服务器端租用到IP地址后，就可以永久性的使用该地址。

动态分配方式（Dynamic Allocation），DHCP服务器给主机指定一个具有时间限制的IP地址，时间到期或主机明确表示放弃该地址时，该地址可以被其他主机使用。

手工分配方式（Manual Allocation），客户端的IP地址是由网络管理员指定的，DHCP服务器只是将指定的IP地址告诉客户端主机。

**三种地址分配方式中，只有动态分配可以重复使用客户端不再需要的地址。**

### 4.2协议配置

1.本实验硬件采用正点原子STM32F4开发板，连接原理图如下：
![image-20261107084939014](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107084939014.png)

2.cubemx配置：
ETH设置：

![image-20261107090150403](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107090150403.png)
**选择后根据实际电路图检查引脚配置，不是需修改。**

MII： Medium Independent Interface(介质独立接口)，用于连接介质访问控制层(MAC)子层和物理层(PHY)之间的标准以太网接口，提供数据传输路径。由于 MII 需要多达16根信号线，由此产生的 I/O 口需求及功耗较大。

![image-20261107090239448](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107090239448.png)

RMII： Reduced Medium Independent Interface，RMII 接口是 MII 接口的简化版本，MII 需要 16 根通信线，RMII 只需 7 根通信，在功能上是相同的。对于 RMII 接口，一般需要外部直接提供 50MHz 时钟源，同时接入 MAC 和 PHY。**现在一般都用RMII模式。**

![image-20261107090338847](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107090338847.png)

网卡复位引脚设置：
![image-20261107090631284](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107090631284.png)

LWIP设置：
![image-20261107090843789](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107090843789.png)
**注意：选择 Enabled 。如果使用开发板连接带 DHCP 服务功能的路由器，可以使能。否则开发板直接连接电脑，电脑是没办法提供 DHCP 服务功能的。**

![image-20261107091209961](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107091209961.png)

选择LAN8742兼容LAN8720A芯片

最终引脚设置：
![image-20261107090929324](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107090929324.png)
添加代码：

ethernetif.c,添加复位的代码， 让网卡准备就绪， 否则网卡不工作

```
static void low_level_init(struct netif *netif)

{

/* USER CODE BEGIN MACADDRESS */

// 复位网卡的代码

HAL_GPIO_WritePin(ETH_NRST_GPIO_Port,ETH_NRST_Pin,GPIO_PIN_RESET);

HAL_Delay(55);

HAL_GPIO_WritePin(ETH_NRST_GPIO_Port,ETH_NRST_Pin,GPIO_PIN_SET);

HAL_Delay(55);

/* USER CODE END MACADDRESS */

}
```

在 main() 的死循环中添加 MX_LWIP_Process() 函数

```
/* USER CODE BEGIN PFP */

extern struct netif gnetif;

struct dhcp *dhcp;

/* USER CODE END PFP */

/* Initialize all configured peripherals */

MX_GPIO_Init();

MX_DMA_Init();

MX_USART1_UART_Init();

MX_RTC_Init();

MX_FSMC_Init();

MX_SDIO_SD_Init();

MX_LWIP_Init();

/* USER CODE BEGIN 2 */

__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE); //使能串UART1 IDLE(空闲)中断

HAL_UART_Receive_DMA(&huart1, RX1_Buf, DMA_BUF_SIZE); //设置DMA传输，将uart1的数据搬运到RX1_Buf中
printf("Uart1 init is ok\n");

/* USER CODE END 2 */

/* Infinite loop */

/* USER CODE BEGIN WHILE */

while (1)

{

MX_LWIP_Process();

/* USER CODE END WHILE */

/* USER CODE BEGIN 3 */

dhcp = netif_dhcp_data(&gnetif);

printf("DHCP IP address: %s\n", ip4addr_ntoa(&dhcp->offered_ip_addr));

printf("DHCP Subnet mask: %s\n", ip4addr_ntoa(&dhcp->offered_sn_mask));

printf("DHCP Default gateway: %s\n", ip4addr_ntoa(&dhcp->offered_gw_addr));

HAL_Delay(1000);

}

/* USER CODE END 3 */
```

代码测试：
![image-20261107093143612](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107093143612.png)

下载复位等待连接，路由器自动分配IP成功。

## 5.TCP/IP协议

cubemx配置：
LWIP设置：设置静态IP使开发板和电脑直连通信

![image-20261107094109960](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107094109960.png)

其余配置和DHCP的cubemx配置一致，不再赘述

添加代码：

ethernetif.c,添加复位的代码， 让网卡准备就绪， 否则网卡不工作

```
static void low_level_init(struct netif *netif)

{
/* USER CODE BEGIN MACADDRESS */

// 复位网卡的代码

HAL_GPIO_WritePin(ETH_NRST_GPIO_Port,ETH_NRST_Pin,GPIO_PIN_RESET);

HAL_Delay(55);

HAL_GPIO_WritePin(ETH_NRST_GPIO_Port,ETH_NRST_Pin,GPIO_PIN_SET);

HAL_Delay(55);

/* USER CODE END MACADDRESS */

}
```

tcpclicent.c

- 主动连接指定服务器（`192.168.0.100:9001`），断线后每 3 秒自动重连。
- 连接动作**不在 `tcp_client_start` 里完成**，而是放到主循环 `tcp_client_poll` 中发起，避免阻塞启动流程。
- 用状态机管理连接：`IDLE → CONNECTING → CONNECTED → IDLE`。
- 收到数据先缓存，在主循环里通过 `tcp_client_send` 回发；发送不完整时用 `tx_off` 记录进度。

```
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
```

tcpserver.c

- 在本地 `8000` 端口监听，接受 1 个 TCP 客户端连接（单客户端模型）。
- 收到数据后先存入应用自己的缓冲区，置“数据就绪”标志；主循环里再把数据原样发回去（echo）。
- 用 `tcp_err` 兜底处理对端 RST 等异常，及时释放 PCB。

```
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

```

udp.c

- 本地绑定 `9000` 端口接收 UDP 数据报。
- 记录“最近一次发数据给我的对端 IP/端口”，主循环把缓存的数据报原样回发。
- UDP **无连接**：没有握手、没有重传、没有窗口；一个 `recv` 回调对应一个完整数据报，边界天然保留。
- 缓存只放一个数据报（`rx_len == 0` 表示空闲），忙时新包直接丢弃——UDP 本身允许丢包，这样实现最简单。

```
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

```

main.c

```

#include "main.h"
#include "dma.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "tcpserver.h"
#include "tcpclient.h"
#include "udp.h"
/* USER CODE END Includes */
int main(void)
{

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_LWIP_Init();
  /* USER CODE BEGIN 2 */
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE); //使能串UART1 IDLE(空闲)中断
	HAL_UART_Receive_DMA(&huart1, RX1_Buf, DMA_BUF_SIZE); //设置DMA传输，将uart1的数据搬运到RX1_Buf中
		printf("Uart1 init is ok\n");
	/*tcp_server*/
	tcp_server_start(8000);
  tcp_client_start("192.168.0.100", 9001);   /* 改成PC的IP和端口 */
  udp_user_start(9000);
	printf("Static IP address: %s\n", ip4addr_ntoa(&gnetif.ip_addr));
	printf("Subnet mask      : %s\n", ip4addr_ntoa(&gnetif.netmask));
	printf("Default gateway  : %s\n", ip4addr_ntoa(&gnetif.gw));
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
     MX_LWIP_Process(); // 收包 + 协议栈超时处理 + 链路检测
  		transfer_data(); //tcp_server回发
		  tcp_client_poll();      /* TCP Client 自动连接 + 回发 */
    udp_transfer_data();    /* UDP 回发 */
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}
```

代码内容：

| 文件                                            | 作用                                                         |
| ----------------------------------------------- | ------------------------------------------------------------ |
| [main.c](static/Core/Src/main.c)                | 初始化外设和 LwIP，启动三个通信模块，主循环轮询              |
| [lwip.c](static/LWIP/App/lwip.c)                | LwIP 初始化、网卡注册、主循环收包/超时/链路检测              |
| [ethernetif.c](static/LWIP/Target/ethernetif.c) | 网卡驱动适配层：pbuf 与 STM32 ETH DMA 之间的收发             |
| [tcpclient.c](static/Core/Src/tcpclient.c)      | TCP 客户端（主动连接、自动重连、回环）                       |
| [tcpserver.c](static/Core/Src/tcpserver.c)      | TCP 服务器（监听、接受连接、回环）                           |
| [udp.c](static/Core/Src/udp.c)                  | UDP 收发（绑定端口、记录对端、回环）                         |
| [lwipopts.h](static/LWIP/Target/lwipopts.h)     | LwIP 裁剪配置：`NO_SYS 1`、`LWIP_SOCKET 0`、`LWIP_NETCONN 0`、硬件校验和 |
| [usart.c](static/Core/Src/usart.c)              | 串口 printf 调试输出                                         |

因为 `NO_SYS = 1`、`LWIP_SOCKET = 0`、`LWIP_NETCONN = 0`，工程没有使用 socket 编程接口，而是直接使用 LwIP Raw API（回调式 API）。所有协议处理都在主循环调用 `ethernetif_input` 时同步执行，没有独立协议栈线程。

代码测试：
TCP Client测试，连接指定的IP、端口，TCP发送可回传；串口显示接收字节数：
![image-20261107100913049](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107100913049.png)

 TCP Server测试，连接指定的IP、端口，TCP发送可回传；串口显示接收字节数：


![image-20261107104845551](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107104845551.png)

UDP测试，设置开发板指定IP、端口，串口显示接收字节数：
![image-20261107105107184](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20261107105107184.png)