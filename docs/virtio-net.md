# virtio-net 与最小 IPv4/ICMP 网络层

## 驱动边界

AukOS 支持 QEMU transitional virtio-net PCI 设备 `1af4:1000` 的 legacy
I/O transport。`kernel/virtio_net.c` 只负责 transport、DMA、queue 和同步
Ethernet frame API：

- `virtio_net_is_ready()` / `virtio_net_get_mac()` 查询可用性与设备 MAC
- `virtio_net_send_frame()` 同步发送一帧并等待 TX used completion
- `virtio_net_poll_receive()` 有界轮询 RX used completion，返回去掉
  `virtio_net_hdr` 的 Ethernet frame
- 调用者 buffer 永远不会直接交给设备；TX/RX 均经过驱动私有静态 DMA bounce
  buffer，RX completion 消费后会重新发布 buffer

驱动只协商 `VIRTIO_NET_F_MAC`。queue 0 是单 buffer receiveq，queue 1 是串行
transmitq；两条 legacy split queue 使用独立、4 KiB 对齐、低于 1 GiB 的静态
storage。驱动保留 frame 长度、指针范围、descriptor、used id/length、device
`FAILED` 和 polling timeout 检查。

不支持 modern PCI capabilities、packed queue、IRQ/MSI/MSI-X、多队列、并发请求、
mergeable RX buffers 或 checksum/GSO offload。

## 固定测试网络层

`kernel/net.c` 在公开 Ethernet API 上执行启动 self-test，使用 Makefile 固定的
QEMU user networking 拓扑：

```text
guest MAC:    52:54:00:12:34:56
guest IPv4:   10.0.2.15
gateway IPv4: 10.0.2.2
```

这些值是 QEMU 测试假设，不是 DHCP 或运行时配置。ARP cache 容量只有一个 entry
（IPv4、MAC、valid），没有 TTL、eviction、重试计时器、gratuitous ARP 或并发
resolve。cache miss 会发送真实广播 request；只有完整校验真实 RX reply 后才提交
gateway MAC。ICMP 路径只能使用这条已学习 entry，不能硬编码 gateway MAC。

接收路径先检查 14-byte Ethernet header 和目的 MAC，再按 EtherType 分派 ARP
`0x0806` 或 IPv4 `0x0800`；未知类型在固定 packet 上限内忽略。IPv4 只接受 version
4、IHL 5、有效 header checksum、无 fragmentation、protocol ICMP 和匹配地址的包。
ICMP reply 必须有有效 checksum，并完整匹配 type/code、identifier、sequence 和
payload。所有等待均为 polling 且有固定 spin 和 packet 上限。

请求使用 20-byte IPv4 header、TTL 64、DF、固定非零 identification，以及 ICMP
identifier `0xa905`、sequence 1 和 payload `AukOS ICMP echo`。IPv4 `total_length`
不含 Ethernet header/padding；ICMP checksum 也不覆盖 padding。网络字节序和
one's-complement checksum helper 位于 `kernel/net_packets.c`，可直接用于 host test；
奇数长度最后一个 byte 按高 8 bit 求和。

只有 ARP cache 学习、ICMP TX completion 和完整的真实 RX echo reply 校验都成功后，
才会输出：

```text
net: gateway ARP entry learned
net: ICMP echo reply received from 10.0.2.2
net: gateway ARP/IPv4/ICMP self-test passed
```

此时 `ethernet_ready`、`arp_ready`、`ipv4_ready`、`icmp_ready` 和 `udp_ready` 为 1；
`tcp_ready` 仍为 0。失败不会把未完成层标记为 ready。

## 最小 UDP datagram socket

`kernel/net_packets.c` 在同一 IPv4/checksum 边界实现 Ethernet + IPv4 + UDP builder
和 parser。UDP pseudo-header checksum 覆盖奇数 payload；计算值为零时发送
`0xffff`。接收端按 IPv4 语义接受 checksum 字段 0，非零字段必须严格通过 pseudo-
header checksum。Ethernet padding 不计入 IP/UDP length 或 checksum。

进程 fd slot 指向 tagged、引用计数的 descriptor object，明确区分 VFS file 和 UDP
socket。`dup/dup2/fork` 共享 open object，`close/process_reap` 释放最后引用；文件类
syscall 会拒绝 socket descriptor。全局固定最多 64 个 descriptor、8 个 UDP socket，
每 socket 最多排队 4 个、每个最大 1472-byte datagram。队列满、socket 表满和端口
冲突均确定性失败。

首轮 ABI 只接受 `AF_INET + SOCK_DGRAM + protocol 0/IPPROTO_UDP`。`bind` 接受
`INADDR_ANY` 或 `10.0.2.15` 和显式非零端口；`sendto` 只路由到已有 gateway entry
对应的 `10.0.2.2`，flags 必须为 0。`recvfrom` bounded polling 并返回一个 datagram；
buffer 太小时复制可容纳的前缀并消费整个 datagram，同时回填 source address、port
和 addrlen。完整 parser 校验后才按 destination port 投递，malformed、checksum
错误和未绑定端口的包会丢弃。

## 尚未实现

这不是通用或异步网络栈。当前没有 DHCP、路由表、多 entry ARP cache、IPv4 options、
fragment reassembly、IPv6、TCP、DNS 或 TLS。`connect/listen/accept` 仍明确失败；没有
非阻塞 socket、readiness API、IRQ wakeup、多 socket 并发收发或用户态 `ping`。

## 验证

Host tests 覆盖 IPv4/ICMP 与 UDP checksum、奇数长度、零/最大 payload、Ethernet
padding、checksum 0 接收策略、错误 version/IHL/length/checksum/fragment/address/
port/protocol 和各层截断；另覆盖 descriptor 引用生命周期、固定池耗尽、UDP queue
容量/FIFO 和小 buffer 截断语义：

```sh
make test
```

BIOS 与 UEFI smoke 自动启动仓库内 UDP echo helper，ring3 `/bin/udp_test` 通过正式
libc API 完成真实 TX/RX DMA 往返，并保留 virtio-blk、ext4、Signal 和用户态文件
读取回归：

```sh
make smoke
make smoke-uefi
```

smoke 会保留 `build/bios-udp.pcap` 和 `build/uefi-udp.pcap`，并自动严格校验 UDP
请求/响应 IP、MAC、ports、length、checksum 和 15-byte payload。完整启动日志应查看
`build/*-smoke.log`，因为 aush 启动前会清屏。

## Queue PFN 注意事项

设备全局 reset 后，每条 queue 的 PFN 应为零。初始化 queue 1 时不能先写
`QUEUE_PFN=0` 再写目标 PFN：QEMU 11 将前一次写入视为 queue reset，并把
`queue_sel` 重置为 0，导致 queue 1 的 PFN 错写到 queue 0。驱动会确认所选 queue
的初始 PFN 为零，然后直接写入目标 PFN 并回读校验。
