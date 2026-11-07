# ETH-LWIP 工程 TCP Client / TCP Server / UDP 代码解析

> 工程：`STM32F407ZGT6 + LAN8742 + LwIP 2.1.2`，裸机（`NO_SYS = 1`，无 FreeRTOS），
> 静态 IP `192.168.0.30 / 255.255.255.0`，网关 `192.168.0.1`。

本文按“底层数据通路 → TCP Server → TCP Client → UDP → 设计思想总结”的顺序，解释本工程中三种通信方式是怎么写出来的，以及每一步为什么要这么做。

---

## 1. 工程结构与数据通路

### 1.1 相关文件

| 文件 | 作用 |
| --- | --- |
| [main.c](static/Core/Src/main.c) | 初始化外设和 LwIP，启动三个通信模块，主循环轮询 |
| [lwip.c](static/LWIP/App/lwip.c) | LwIP 初始化、网卡注册、主循环收包/超时/链路检测 |
| [ethernetif.c](static/LWIP/Target/ethernetif.c) | 网卡驱动适配层：pbuf 与 STM32 ETH DMA 之间的收发 |
| [tcpclient.c](static/Core/Src/tcpclient.c) | TCP 客户端（主动连接、自动重连、回环） |
| [tcpserver.c](static/Core/Src/tcpserver.c) | TCP 服务器（监听、接受连接、回环） |
| [udp.c](static/Core/Src/udp.c) | UDP 收发（绑定端口、记录对端、回环） |
| [lwipopts.h](static/LWIP/Target/lwipopts.h) | LwIP 裁剪配置：`NO_SYS 1`、`LWIP_SOCKET 0`、`LWIP_NETCONN 0`、硬件校验和 |
| [usart.c](static/Core/Src/usart.c) | 串口 printf 调试输出 |

因为 `NO_SYS = 1`、`LWIP_SOCKET = 0`、`LWIP_NETCONN = 0`，工程**没有使用 socket 编程接口**，而是直接使用 **LwIP Raw API**（回调式 API）。所有协议处理都在主循环调用 `ethernetif_input` 时同步执行，没有独立协议栈线程。

### 1.2 初始化顺序（main.c）

```c
HAL_Init();
SystemClock_Config();
MX_GPIO_Init();
MX_DMA_Init();
MX_USART1_UART_Init();
MX_LWIP_Init();                 // 初始化 LwIP + 注册网卡

__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
HAL_UART_Receive_DMA(&huart1, RX1_Buf, DMA_BUF_SIZE);

tcp_server_start(8000);                     // TCP 服务器监听 8000
tcp_client_start("192.168.0.100", 9001);    // TCP 客户端连接 PC:9001
udp_user_start(9000);                       // UDP 本地端口 9000
```

`MX_LWIP_Init()` 内部（[lwip.c:60](static/LWIP/App/lwip.c#L60)）：

1. `lwip_init()`：初始化协议栈内存、memp、pbuf 池、TCP/UDP 等。
2. 用 `IP4_ADDR` 填好静态 IP、掩码、网关。
3. `netif_add(&gnetif, ..., &ethernetif_init, &ethernet_input)`：注册网卡，`ethernetif_init` 中挂接底层收发函数 `low_level_output`，并初始化 ETH 外设/DMA 描述符。
4. `netif_set_default()` / `netif_set_up()`：把网卡设为默认并启用。
5. `netif_set_link_callback()`：网线插拔状态变化时回调。

### 1.3 主循环与数据通路

```c
while (1)
{
    MX_LWIP_Process();      // 收包 + 协议栈超时处理 + 链路检测
    transfer_data();        // TCP Server 回发
    tcp_client_poll();      // TCP Client 自动连接/重连 + 回发
    udp_transfer_data();    // UDP 回发
}
```

`MX_LWIP_Process()`（[lwip.c:142](static/LWIP/App/lwip.c#L142)）做三件事：

```c
ethernetif_input(&gnetif);   // 从 ETH DMA 取出所有已收数据包，交给协议栈
sys_check_timeouts();        // 处理 LwIP 定时器（TCP 重传、ARP、keepalive 等）
Ethernet_Link_Periodic_Handle(&gnetif);  // 每 100ms 读 PHY，检测网线插拔
```

收包方向：

```
ETH DMA 收到帧
  → MX_LWIP_Process → ethernetif_input
  → low_level_input → HAL_ETH_ReadData（零拷贝，pbuf 直接指向 DMA 缓冲）
  → netif->input = ethernet_input
  → ip4_input →（按协议分流）
       ├─ tcp_input → 找到对应 tcp_pcb → 调用 recv_callback
       └─ udp_input → 找到对应 udp_pcb → 调用 udp_recv_callback
```

发包方向：

```
应用调用 tcp_write / udp_sendto
  → ip4_output → etharp_output（查/发 ARP 得 MAC）
  → netif->linkoutput = low_level_output
  → HAL_ETH_Transmit（把 pbuf 链挂到 DMA 发送描述符）
```

关键点：**Raw API 中所有回调（accept/recv/err/connected 等）都在 `ethernetif_input` 的调用栈里执行**，也就是在主循环上下文中。因此回调里不能长时间阻塞，复杂业务应像本工程一样放到主循环轮询函数里做。

---

## 2. TCP Server（tcpserver.c）

### 2.1 设计目标与思路

- 在本地 `8000` 端口监听，接受 1 个 TCP 客户端连接（单客户端模型）。
- 收到数据后先存入应用自己的缓冲区，置“数据就绪”标志；主循环里再把数据原样发回去（echo）。
- 用 `tcp_err` 兜底处理对端 RST 等异常，及时释放 PCB。

这是典型的**“回调里只收数据，主循环里做业务和发送”**写法，避免在协议栈回调里做耗时操作。

### 2.2 启动流程（`tcp_server_start`，[tcpserver.c:201](static/Core/Src/tcpserver.c#L201)）

```c
pcb = tcp_new();                          // 1. 分配一个 TCP 控制块
tcp_bind(pcb, IP_ADDR_ANY, port);         // 2. 绑定本地任意 IP + 8000 端口
pcb = tcp_listen(pcb);                    // 3. 进入 LISTEN 状态
tcp_accept(pcb, accept_callback);         // 4. 注册“有新连接”回调
```

各步骤含义：

| 步骤 | API | 说明 |
| --- | --- | --- |
| 1 | `tcp_new()` | 从 `MEMP_TCP_PCB` 池分配 PCB，失败说明内存池耗尽 |
| 2 | `tcp_bind(IP_ADDR_ANY, port)` | 绑定端口；`IP_ADDR_ANY` 表示任意本机地址 |
| 3 | `tcp_listen(pcb)` | 把 PCB 变为 LISTEN 状态。**返回值必须接住**：lwIP 可能返回一个新分配的精简 PCB，原 PCB 已被释放 |
| 4 | `tcp_accept(pcb, cb)` | 有客户端握手成功后，协议栈回调 `accept_callback` |

注意：`tcp_listen` 返回 `NULL` 时本工程只置 `ret = -3`，却仍继续调用 `tcp_accept(pcb, ...)` 并打印成功。实际使用时这里应 `goto __exit` 返回错误。

### 2.3 接受连接（`accept_callback`，[tcpserver.c:109](static/Core/Src/tcpserver.c#L109)）

```c
if (tcppcb == NULL) {                 // 还没有活动连接
    tcppcb = newpcb;                  // 保存新连接的 PCB
    tcp_arg(newpcb, NULL);
    tcp_recv(newpcb, recv_callback);  // 注册“收到数据”回调
    tcp_err(newpcb, error_callback);  // 注册“连接异常”回调
} else {
    tcp_abort(newpcb);                // 已有连接，拒绝第二个客户端
}
```

- `accept_callback` 收到的 `newpcb` 是一个**已经建立连接（ESTABLISHED）的新 PCB**，与监听 PCB 不是同一个。
- 本工程只保留一个全局 `tcppcb`，所以是单客户端；第二个客户端会被 `tcp_abort` 直接 RST 掉。
- 这里注册了 `tcp_recv`（必须）和 `tcp_err`（可选）；`tcp_sent`、`tcp_poll` 由宏开关关闭，默认不注册。

### 2.4 接收数据（`recv_callback`，[tcpserver.c:76](static/Core/Src/tcpserver.c#L76)）

```c
if (p == NULL) {                      // 对端优雅关闭（收到 FIN）
    tcp_server_disconnect(tpcb);
    err = ERR_CLSD;
}
else if (err != ERR_OK) {             // 异常空包
    pbuf_free(p);
}
else {
    if ((TCP_RX_STA & 0x8000) == 0) { // 应用缓冲区空闲
        for (it = p; it; it = it->next) {
            if (TCP_RX_STA + it->len > TCP_RX_LEN) break;
            memcpy(TCP_RX_BUF + TCP_RX_STA, it->payload, it->len);
            TCP_RX_STA += it->len;
        }
        TCP_RX_STA |= 0x8000;         // 置“一帧数据已收到”标志
    }
    tcp_recved(tpcb, p->tot_len);     // 告诉协议栈：这段数据已被应用消费，扩大窗口
    pbuf_free(p);                     // 必须释放 pbuf，否则内存泄漏
}
```

设计要点：

1. **pbuf 是链式的**：一个 TCP 段可能由多个 `pbuf->next` 组成，所以用 `for` 遍历整条链拷贝。
2. **标志位复用**：`TCP_RX_STA` 的 bit15 表示“有新数据”，低 15 位表示长度。缓冲区忙（bit15 置位）时，后续到达的数据虽然仍会 `tcp_recved + pbuf_free`，但**不再拷贝**——相当于主动丢弃，保证主循环处理期间缓冲区不被覆盖。
3. **`tcp_recved()` 必须调用**：TCP 是滑动窗口协议，不调用它，接收窗口不会前移，发送方最终会停发。
4. **`pbuf_free()` 必须调用**：`recv` 回调把 pbuf 所有权交给应用，协议栈不会再释放。

> 补充：这里的“一帧”其实是应用层约定，TCP 是字节流，没有消息边界。本工程把一次或连续到达的数据当成一批处理，对 echo 演示足够，但如果做帧协议，需要自己加长度/分隔符解析。

### 2.5 发送数据（`tcp_server_send`，[tcpserver.c:167](static/Core/Src/tcpserver.c#L167)）

```c
while ((err == ERR_OK) && (len != 0) && (tcp_sndbuf(tpcb) > 0)) {
    nwrite = min(tcp_sndbuf(tpcb), len);       // 本次能写多少
    err = tcp_write(tpcb, p, nwrite, 1);       // flags=1 即 TCP_WRITE_FLAG_COPY
    if (err == ERR_OK) { len -= nwrite; total += nwrite; p += nwrite; }
    tcp_output(tpcb);                          // 立即触发组包发送
}
```

- `tcp_sndbuf()` 查询发送缓冲区剩余空间，**不能无条件 `tcp_write` 超过它**，否则返回 `ERR_MEM`。
- `tcp_write` 只是把数据挂到发送队列（`TCP_WRITE_FLAG_COPY` 表示由 LwIP 拷贝一份，应用缓冲区可立即复用），`tcp_output` 才真正交给 IP 层发送。
- 循环处理“发送缓冲区不够”的情况，返回实际写入的字节数。
- 未注册 `tcp_sent` 回调，所以用轮询方式发送；如果数据量很大，更规范的是注册 `tcp_sent` 在缓冲区腾空后继续发。

### 2.6 断开连接（`tcp_server_disconnect`，[tcpserver.c:145](static/Core/Src/tcpserver.c#L145)）

```c
tcp_arg(tpcb, NULL);
tcp_recv(tpcb, NULL);
tcp_err(tpcb, NULL);
tcp_abort(tpcb);        // 立即复位连接并释放 PCB
tcppcb = NULL;
```

- 先清空所有回调，防止 PCB 释放过程中再回调到应用。
- `tcp_abort` 发 RST 立即断开；若想优雅关闭（发 FIN），应改用 `tcp_close`。
- **异常回调 `error_callback` 中的隐患**：lwIP 调用 `tcp_err` 时，PCB 已经被协议栈释放。本工程在 `error_callback` 里又用全局 `tcppcb` 调 `tcp_server_disconnect` → `tcp_abort(tcppcb)`，属于对已释放 PCB 的操作（悬空指针），且 `tcppcb` 也可能指向别的连接。正确做法参考客户端：在 err 回调里只重置状态、置 `NULL`，**不要**再 `tcp_close/tcp_abort`。

### 2.7 主循环回显（`transfer_data`，[tcpserver.c:261](static/Core/Src/tcpserver.c#L261)）

```c
if (tcppcb != NULL && tcppcb->state == ESTABLISHED) {
    if (TCP_RX_STA & 0x8000) {
        user_senddata(TCP_RX_BUF, TCP_RX_STA & 0x7FFF);  // 原样发回
        TCP_RX_STA = 0;                                  // 清标志和长度
    }
}
```

- 用 `tcppcb->state == ESTABLISHED` 二次确认连接有效。
- 发送完成后清空 `TCP_RX_STA`，缓冲区重新可用。

### 2.8 TCP Server 整体时序

```
PC                            STM32 (8000)
 |  SYNC                          |
 |------------------------------->|  tcp_input → accept_callback
 |  SYNC-ACK                       |  保存 tcppcb，注册 recv/err
 |<-------------------------------|
 |  ACK                            |
 |------------------------------->|
 |  data ────────────────────────>|  recv_callback：拷贝进 TCP_RX_BUF，置 bit15
 |                                 |  main loop transfer_data → tcp_write/tcp_output
 |<──────────────────── data echo |
 |  FIN ─────────────────────────>|  recv_callback(p == NULL) → tcp_abort，释放 PCB
```

---

## 3. TCP Client（tcpclient.c）

### 3.1 设计目标与思路

- 主动连接指定服务器（`192.168.0.100:9001`），断线后每 3 秒自动重连。
- 连接动作**不在 `tcp_client_start` 里完成**，而是放到主循环 `tcp_client_poll` 中发起，避免阻塞启动流程。
- 用状态机管理连接：`IDLE → CONNECTING → CONNECTED → IDLE`。
- 收到数据先缓存，在主循环里通过 `tcp_client_send` 回发；发送不完整时用 `tx_off` 记录进度。

### 3.2 状态机

```c
typedef enum {
    TCPC_IDLE = 0,      // 未连接（含等待重连）
    TCPC_CONNECTING,    // 已发起 tcp_connect，等待握手结果
    TCPC_CONNECTED      // 已连接
} tcpc_state_t;
```

```
          start()                 tcp_connect 成功
  IDLE ────────────> IDLE ──────────────────────> CONNECTED
   ^                  │  (link up 且 3s 到)           │
   │                  │                              │ 对端关闭/出错/stop
   └──────────────────┴──────────────────────────────┘
                     回到 IDLE，记录 last_try，3s 后重试
```

### 3.3 启动（`tcp_client_start`，[tcpclient.c:165](static/Core/Src/tcpclient.c#L165)）

```c
if (!ipaddr_aton(ip_str, &remote_ip)) return -1;  // 字符串 IP → ip_addr_t
remote_port = port;
enabled = 1;
state = TCPC_IDLE;
last_try = HAL_GetTick() - TCPC_RECONNECT_MS;      // 让第一次轮询立即尝试
```

这一步只保存参数、使能客户端，不做任何网络操作，因此**不阻塞**。`last_try` 往前拨 3 秒，保证第一次 `tcp_client_poll` 就满足重连时间条件。

### 3.4 发起连接（`tcp_client_do_connect`，[tcpclient.c:126](static/Core/Src/tcpclient.c#L126)）

```c
cpcb = tcp_new();
tcp_arg(cpcb, NULL);
tcp_recv(cpcb, client_recv_callback);
tcp_err(cpcb, client_err_callback);
state = TCPC_CONNECTING;
err = tcp_connect(cpcb, &remote_ip, remote_port, client_connected_callback);
```

- `tcp_connect` 是**异步**的：它发出 SYN 后立即返回。握手完成（或失败）时协议栈回调 `client_connected_callback`。
- 必须先注册 `recv/err`，因为连接建立后马上可能收到数据。
- `tcp_connect` 返回错误（如无路由、内存不足）时，`tcp_abort` 释放 PCB，回到 IDLE。

### 3.5 连接结果回调（`client_connected_callback`，[tcpclient.c:92](static/Core/Src/tcpclient.c#L92)）

```c
if (err != ERR_OK) { tcp_client_close(); return err; }
state = TCPC_CONNECTED;
rx_len = 0; tx_off = 0;
```

握手成功后进入 `CONNECTED`，清空收发缓存计数。

### 3.6 接收数据（`client_recv_callback`，[tcpclient.c:64](static/Core/Src/tcpclient.c#L64)）

```c
if (p == NULL) {                 // 对端关闭
    tcp_client_close();
    return ERR_OK;
}
if (err != ERR_OK) { pbuf_free(p); return err; }

n = min(p->tot_len, TCPC_RX_LEN - rx_len);   // 放不下的部分丢弃
if (n > 0) {
    pbuf_copy_partial(p, rx_buf + rx_len, n, 0);
    rx_len += n;
}
tcp_recved(tpcb, p->tot_len);    // 更新接收窗口
pbuf_free(p);                    // 释放 pbuf
```

与服务器的区别：

- 用 `pbuf_copy_partial` 一次性把整条 pbuf 链拷到连续数组，代码更简洁。
- 缓冲区满时截断（丢弃尾部），不覆盖已有数据。
- 缓存是“追加”模式：`rx_len` 只增不减，直到主循环发完才清零。

### 3.7 发送数据（`tcp_client_send`，[tcpclient.c:190](static/Core/Src/tcpclient.c#L190)）

```c
if (cpcb == NULL || state != TCPC_CONNECTED) return 0;
n = min(tcp_sndbuf(cpcb), len);              // 只写发送缓冲区装得下的部分
if (n == 0) return 0;
if (tcp_write(cpcb, buf, n, TCP_WRITE_FLAG_COPY) != ERR_OK) return 0;
tcp_output(cpcb);
return n;                                     // 返回实际写入量，供上层记录进度
```

采用“**能写多少写多少，返回实际写入量**”的接口设计；主循环据此维护 `tx_off` 偏移，实现分批发送。

### 3.8 轮询（`tcp_client_poll`，[tcpclient.c:211](static/Core/Src/tcpclient.c#L211)）

```c
if (!enabled) return;

if (state == TCPC_IDLE) {
    if (HAL_GetTick() - last_try >= TCPC_RECONNECT_MS) {
        last_try = HAL_GetTick();
        if (netif_is_link_up(&gnetif) && !ip_addr_isany(&gnetif.ip_addr))
            tcp_client_do_connect();          // 网线插好且有 IP 才连接
    }
}
else if (state == TCPC_CONNECTED) {
    if (rx_len > tx_off) {                    // 有未回发的数据
        sent = tcp_client_send(rx_buf + tx_off, rx_len - tx_off);
        if (sent > 0) {
            tx_off += sent;
            if (tx_off >= rx_len) { rx_len = 0; tx_off = 0; }  // 全部发完，清缓存
        }
    }
}
```

要点：

1. **重连节流**：每 3 秒最多尝试一次，避免疯狂重连拖垮协议栈。
2. **连接前置条件**：`netif_is_link_up()` 且 IP 非零，否则 `tcp_connect` 必然失败。
3. **发送偏移 `tx_off`**：`tcp_sndbuf` 不足时只发一部分，下次继续从断点发，保证数据不重复、不丢失（在本应用缓存内）。
4. 回显逻辑放主循环而不是 recv 回调里，回调保持短小。

### 3.9 关闭（`tcp_client_close`，[tcpclient.c:110](static/Core/Src/tcpclient.c#L110)）

```c
tcp_arg(cpcb, NULL);
tcp_recv(cpcb, NULL);
tcp_err(cpcb, NULL);
if (tcp_close(cpcb) != ERR_OK)
    tcp_abort(cpcb);      // close 失败（如内存不足）则强制 RST
cpcb = NULL;
state = TCPC_IDLE;
last_try = HAL_GetTick();
```

- 优先 `tcp_close` 优雅关闭（发 FIN），失败再 `tcp_abort`。
- `client_err_callback` 中**只重置状态，不调用 close/abort**，因为此时 PCB 已被协议栈释放。这一点在源码注释里有明确说明，也是和服务器 `error_callback` 的关键差别（服务器那处存在隐患，见 2.6）。

### 3.10 TCP Client 整体时序

```
STM32 (client)                       PC (server 192.168.0.100:9001)
 |  tcp_new / tcp_connect               |
 |  SYN ───────────────────────────────>|
 |<───────────────────────────── SYN-ACK|
 |  ACK ───────────────────────────────>|  client_connected_callback → CONNECTED
 |  data ──────────────────────────────>|  (主循环 rx 缓冲回发)
 |<────────────────────────────── data  |  client_recv_callback → rx_buf
 |  FIN / RST ─────────────────────────>|  p==NULL 或 err → close → IDLE
 |                                      |  3s 后自动重连
```

---

## 4. UDP（udp.c）

### 4.1 设计目标与思路

- 本地绑定 `9000` 端口接收 UDP 数据报。
- 记录“最近一次发数据给我的对端 IP/端口”，主循环把缓存的数据报原样回发。
- UDP **无连接**：没有握手、没有重传、没有窗口；一个 `recv` 回调对应一个完整数据报，边界天然保留。
- 缓存只放一个数据报（`rx_len == 0` 表示空闲），忙时新包直接丢弃——UDP 本身允许丢包，这样实现最简单。

### 4.2 启动（`udp_user_start`，[udp.c:87](static/Core/Src/udp.c#L87)）

```c
upcb = udp_new();                                  // 1. 分配 UDP PCB
udp_bind(upcb, IP_ADDR_ANY, port);                 // 2. 绑定本地端口 9000
udp_recv(upcb, udp_recv_callback, NULL);           // 3. 注册收包回调
```

失败时 `udp_remove(upcb)` 释放 PCB。与 TCP 不同，UDP 不需要 listen/accept。

### 4.3 接收（`udp_recv_callback`，[udp.c:54](static/Core/Src/udp.c#L54)）

```c
if (p == NULL) return;

if (rx_len == 0) {                        // 缓存空闲
    n = min(p->tot_len, UDP_RX_LEN);      // 最多 1472 字节
    pbuf_copy_partial(p, rx_buf, n, 0);
    rx_len = n;
    ip_addr_copy(peer_ip, *addr);         // 记录发送方，用于回发
    peer_port = port;
} else {
    /* 缓存忙，丢弃 */
}
pbuf_free(p);                             // 无论收不收，都必须释放
```

要点：

1. **必须 `pbuf_free(p)`**，`p == NULL` 才直接返回。
2. 回调参数 `addr/port` 就是发送方地址，本工程保存下来做“回谁”。
3. `p->tot_len` 超长时截断；正常以太网 MTU 1500，UDP 载荷最大 1472（1500 − 20 IP 头 − 8 UDP 头），所以 `UDP_RX_LEN = 1472`。
4. 没有 `udp_recved` 这种窗口函数——UDP 不需要流控。

### 4.4 发送（`udp_send_pbuf`，[udp.c:38](static/Core/Src/udp.c#L38)）

```c
p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);   // 1. 分配发送用 pbuf
pbuf_take(p, buf, len);                          // 2. 把应用数据拷入 pbuf
err = udp_sendto(upcb, p, ip, port);             // 3. 指定目标 IP/端口发送
pbuf_free(p);                                    // 4. 发送完释放 pbuf
```

- `udp_sendto` 不保存数据，返回后即可释放 pbuf（lwIP 在内部已完成组包/发送或已接管）。
- 每次发送都重新分配 pbuf，`ERR_MEM` 表示暂时没内存。

对外接口 `udp_user_sendto`（[udp.c:116](static/Core/Src/udp.c#L116)）先用 `ipaddr_aton` 把字符串 IP 转成 `ip_addr_t`，再调用上面的函数。

### 4.5 主循环回显（`udp_transfer_data`，[udp.c:130](static/Core/Src/udp.c#L130)）

```c
if (rx_len > 0) {
    err = udp_send_pbuf(&peer_ip, peer_port, rx_buf, rx_len);
    if (err == ERR_OK)          rx_len = 0;   // 发送成功，清缓存
    else if (err != ERR_MEM)    rx_len = 0;   // 其他错误，丢弃
    /* ERR_MEM：暂时没内存，保留数据，下次循环重试 */
}
```

- `ERR_MEM` 是可恢复错误，保留数据下次重试；其他错误视为本次发送失败，丢弃。
- 因为缓存会在发送成功前一直被占用，新到的 UDP 包会被丢弃——这是用“单缓冲 + 轮询”换简单性的取舍。

### 4.6 UDP 整体时序

```
PC (任意端口)                        STM32 (9000)
 |  UDP datagram ────────────────────>|  udp_recv_callback
 |                                    |  拷贝 rx_buf，记录 peer_ip/peer_port
 |                                    |  main loop udp_transfer_data
 |<──────────────── UDP datagram echo |  udp_sendto(peer_ip, peer_port)
```

---

## 5. TCP 与 UDP 的对比

| 维度 | TCP Server | TCP Client | UDP |
| --- | --- | --- | --- |
| 角色 | 被动监听 | 主动连接 | 无连接 |
| 核心 PCB API | `tcp_new → tcp_bind → tcp_listen → tcp_accept` | `tcp_new → tcp_connect` | `udp_new → udp_bind` |
| 建立连接 | 三次握手（协议栈完成） | 三次握手（异步回调通知） | 无 |
| 数据边界 | 字节流，需应用自己定界 | 字节流，需应用自己定界 | 数据报，天然保留边界 |
| 可靠性/顺序 | 有（重传、排序） | 有 | 无 |
| 流程控制 | `tcp_recved` + 滑动窗口 | 同左 | 无 |
| 发送接口 | `tcp_write` + `tcp_output`，受 `tcp_sndbuf` 限制 | 同左 | `pbuf_alloc` + `pbuf_take` + `udp_sendto` |
| 回调 | `accept / recv / err` | `connected / recv / err` | `recv` |
| 关闭 | `tcp_abort`（RST） | `tcp_close`（FIN），失败再 abort | `udp_remove` |
| 本工程端口 | 8000 | 连 `192.168.0.100:9001` | 本地 9000 |
| 本工程缓存 | 8 KB，bit15 标志 | 2 KB，追加 + 偏移回发 | 1472 B，单包 |

---

## 6. 三种实现的共同设计思想

### 6.1 Raw API + 事件回调

- 没有 socket、没有 select、没有阻塞等待，全部由协议栈在收包时**回调**通知应用。
- `accept/connected/recv/err` 都是事件入口；应用只在这些入口里做“登记状态、拷贝数据、置标志”这类短操作。

### 6.2 回调轻量化，业务放主循环

- `recv_callback` 只负责把数据搬进应用缓冲区并更新窗口；真正的回发在 `transfer_data` / `tcp_client_poll` / `udp_transfer_data` 中完成。
- 原因：Raw API 回调运行在 `ethernetif_input` 的调用链上，阻塞会拖慢整个协议栈；而且发送可能因 `tcp_sndbuf` 不足需要重试，放主循环更好控制。

### 6.3 明确的 pbuf 所有权规则

- 收：回调收到 pbuf → 应用用完必须 `pbuf_free`。
- 发：`tcp_write(..., TCP_WRITE_FLAG_COPY)` 由协议栈拷贝；`udp_sendto` 返回后可立即释放发送 pbuf。
- 漏掉 `pbuf_free` 会耗尽 `PBUF_POOL`，最终整机断网。

### 6.4 应用缓冲区 + 状态标志

- TCP Server：`TCP_RX_BUF + TCP_RX_STA`（bit15 就绪标志，低 15 位长度）。
- TCP Client：`rx_buf + rx_len + tx_off`（追加与发送进度分离）。
- UDP：`rx_buf + rx_len`（单包缓存，忙则丢包）。
- 共同目的：让“协议栈回调上下文”和“应用发送上下文”解耦，避免在回调里直接做发送。

### 6.5 分层清晰

```
应用层     tcpserver.c / tcpclient.c / udp.c
   │        （业务缓冲、回显、状态机、重连）
Raw API    tcp_*.c / udp_*.c
   │        （建立/关闭连接、收发、回调分发）
协议核心   ip4 / etharp / tcp_in / udp
   │
网卡适配   ethernetif.c（pbuf ↔ ETH DMA）
   │
硬件       STM32 ETH MAC + LAN8742 PHY
```

### 6.6 非阻塞与健壮性

- TCP Client 用状态机 + 定时重连，`tcp_connect` 异步化；连接前检查 link 和 IP。
- 错误处理区分“可恢复”和“不可恢复”：如 UDP 的 `ERR_MEM` 保留数据重试，其他错误丢弃；TCP 的 `ERR_RST` 及时释放连接。
- 关闭连接前先清空所有回调（`tcp_arg/recv/err` 置 `NULL`），防止协议栈在释放 PCB 期间回调到已失效的应用状态。

---

## 7. 使用建议与可改进点

1. **服务器异常回调的悬空 PCB**：`error_callback` 中 PCB 已被 lwIP 释放，再 `tcp_abort(tcppcb)` 有风险。建议只做 `tcppcb = NULL;` 并复位接收标志，不再操作 PCB（参考 `client_err_callback`）。
2. **`tcp_listen` 失败分支**：应在 `pcb == NULL` 时直接返回错误，不要继续 `tcp_accept(NULL, ...)`。
3. **单客户端/单包缓存**：若需要多连接或多包并发，需要连接表/队列；当前实现适合演示与点对点透传。
4. **TCP 消息边界**：回显场景无需定界；实际协议建议加包头长度或分隔符，并在 `recv_callback` 中做粘包/拆包。
5. **发送完成通知**：大数据量时建议注册 `tcp_sent` 回调，在发送缓冲区腾空后继续写，减少主循环忙等。
6. **调试方法**：串口 115200 输出各回调和收发日志；PC 侧可用网络调试助手监听 `9001`、连接 `192.168.0.30:8000`、向 `192.168.0.30:9000` 发 UDP；先用 `ping 192.168.0.30` 确认链路与 ARP 正常。

---

## 8. 关键 API 速查

| 功能 | TCP | UDP |
| --- | --- | --- |
| 创建控制块 | `tcp_new()` | `udp_new()` |
| 绑定端口 | `tcp_bind(pcb, ip, port)` | `udp_bind(pcb, ip, port)` |
| 监听 | `tcp_listen(pcb)` + `tcp_accept()` | 无 |
| 主动连接 | `tcp_connect(pcb, ip, port, cb)` | 无（`udp_sendto` 时指定目标） |
| 注册接收回调 | `tcp_recv(pcb, cb)` | `udp_recv(pcb, cb)` |
| 注册错误回调 | `tcp_err(pcb, cb)` | 无 |
| 发送 | `tcp_write()` + `tcp_output()` | `pbuf_alloc` + `pbuf_take` + `udp_sendto()` |
| 查询发送空间 | `tcp_sndbuf(pcb)` | 无（按需分配 pbuf） |
| 确认接收/更新窗口 | `tcp_recved(pcb, len)` | 无 |
| 关闭 | `tcp_close()` / `tcp_abort()` | `udp_remove()` |
| 数据缓冲区 | `pbuf`（链式） | `pbuf`（单包/链式） |

---

