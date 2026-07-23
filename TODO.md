# AukOS TODO（原生开发平台、图形应用与 Minecraft）

说明：这是一个“可执行的路线图”。每个阶段都应该产出**可运行的镜像**与**最小可验证用例**（QEMU 优先）。项目不只以 Minecraft 为目标，还要让 AukOS 成为能够在原生用户态编写、汇编、编译、链接和运行程序的开发平台。

长期目标分为两条同等重要的主线：

- **原生开发环境**：先运行 Vixc，再移植 NASM、LLD 和 Clang/LLVM，最终能在 AukOS 内完成 `source -> object -> ELF -> exec`。
- **桌面与应用环境**：实现图形、输入、声音和 JVM/兼容层，最终运行 Minecraft 与其他图形应用。

网络暂不作为近期主线。当前 UDP 闭环作为稳定基线保留，TCP/DNS 在下载依赖或联机需求变得实际时再继续。

约定：

- `P0` 必做：没有它就无法推进
- `P1` 强烈建议：能显著提升稳定性/效率
- `P2` 可选：优化或扩展

---

## Phase 1 — 可调试可回归的内核骨架（P0）

启动与基本设施：

- [x] Bootloader（UEFI 或 Limine/GRUB 二选一），进入 long mode
  - [x] 最小 Multiboot2/GRUB 内核 ELF，可进入 x86_64 long mode 并调用 C 内核入口
- [x] 中断/异常：IDT、异常处理、panic 回溯（至少打印 RIP/RSP）
- [x] 串口日志与 log level
  - [x] COM1 串口初始化与基础日志输出
  - [x] VGA text console 输出，避免 QEMU 图形窗口停在 GRUB 文本
  - [x] BIOS/UEFI 统一 VGA 80x25 text mode，上传仓库内 8x16 字体到 plane 2
  - [x] INFO 日志前缀统一为绿色 `[OK]`
- [x] 物理内存探测（UEFI memory map 或 multiboot 信息）

内核核心：

- [x] 内存管理：页分配器 + 简单堆分配
- [x] 虚拟内存：内核高半区映射、用户地址空间框架
  - [x] 记录当前内核 PML4，建立 address_space 框架
  - [x] 创建新内核页表，映射低地址 identity 区与 higher-half direct-map 窗口
  - [x] 用户地址空间创建、4 KiB 用户页映射与查询自检
- [x] 任务模型：线程/进程基本结构
- [x] 调度器：单核抢占式（P0）
  - [x] 单核 PIT IRQ0 tick 入口与调度器 tick 自检
  - [ ] SMP（P1）

交付标准：

- 能在 QEMU 里触发/捕获异常并稳定回到 shell 输出
- [x] 基础单元测试（至少在 host 侧跑的算法/数据结构测试）

## Phase 2 — 系统调用与用户态最小闭环（P0）

- [x] syscall 入口（`syscall/sysenter` 方案二选一）
  - [x] x86_64 `syscall` MSR 初始化、汇编入口与 C 分发器自检
- [x] 用户态程序装载：最小 ELF64 装载（先静态链接）
  - [x] 构建并内嵌静态 `/bin/hello` ELF，解析 `PT_LOAD` 并映射到用户地址空间
- [x] 用户态 ABI：进程退出、写串口/console、获取时间/随机数（可先 stub）
  - [x] 内核侧 syscall 编号与 `exit/write/time/random` dispatcher stub
  - [x] ring3 `/bin/hello` 通过 `SYS_WRITE` 输出并通过 `SYS_EXIT` 结束
- [x] 进程模型：`fork/exec`（或先 `spawn`）
  - [x] `spawn` 风格：从 VFS 查找 `/bin/hello`，装载 ELF，映射用户栈并进入 ring3
- [x] 信号/异常到用户态（可先简化）
  - [x] TSS/RSP0 支持用户态异常回到内核 panic 路径

交付标准：

- [x] 启动后进入 `init`，能运行一个简单用户程序（`/bin/hello`）

## Phase 2.5 - Shell 和 ToyBox移植

- [x] Shell实现（aush）
  - [x] 内嵌 `/bin/sh` 和 `/bin/aush`，通过 syscall/VFS 执行 ToyBox 命令
  - [x] 初步交互式 shell：PS/2 键盘轮询 stdin，保留 `help`、`clear`、`fs`、`signal_test`、`exit` 控制 builtin
  - [x] VGA text 硬件光标跟随 shell 输出与输入
  - [x] `aush` 作为 `/bin/aush` 与 `/bin/sh` 提供
  - [x] aush 接入 toybox：`toybox` / `toybox <applet>` 通过 fork+execve 调用 `/bin/toybox`
  - [x] aush 原地 argv parser：space/tab、单双引号、反斜杠、空参数、16 参数边界和错误拒绝均由生产代码 host test 覆盖
  - [x] aush 统一结构化 `argc/argv`，真实多参数通过 `fork/execve/waitpid` 分发到 `/bin/toybox`
  - [x] 非控制命令默认分发到 `/bin/toybox`，移除 aush 的 `ls/cat/echo` 重复实现
  - [x] `cd` 保持 NOFORK shell builtin，支持 HOME/OLDPWD 与 `-L`/`-P`，并由 ToyBox `pwd` 在 BIOS/UEFI 验证持久 cwd
- [x] ToyBox移植（编译通过，链接 libc stubs，生成 158KB ELF）
  - [x] 上游 ToyBox 源码已克隆并可通过 `make toybox-host` 完整 host 构建
  - [x] AukOS 最小 Toybox 配置入口：`toybox/aukos_miniconfig`（231行，显式禁用 default-y）
  - [x] `-nostdinc -D__linux__ -std=gnu17` 交叉编译，freestanding headers 覆盖 portability.h 全部需求
  - [x] 用户态入口 ABI：`argc/argv/envp` 寄存器传参与用户栈布局
  - [x] AukOS libc/POSIX ABI 第一批：`read/write/open/close/lseek/stat/fstat/fstatat/getdents64/isatty/ioctl`
  - [x] libc 第一批：`errno`、`malloc/calloc/realloc/free`、`string/strings`、`ctype`、基础 `stdio`
  - [x] AukOS libc/POSIX ABI 第二批：`opendir/readdir64/closedir`、`getcwd`、`readlinkat` stub
  - [x] AukOS libc/POSIX ABI 第三批：`openat` dirfd stub、`readlinkat` symlink stub、`getcwd` 进程 cwd
  - [x] 进程 ABI 第一层：`getpid`、`dup2` 最小语义和 libc syscall wrappers
  - [x] 进程 ABI 第二层：cooperative runnable/blocked 调度、真实 `fork/execve/waitpid`
  - [x] typed bounded `pipe`：descriptor 引用、EOF/broken pipe、backpressure 和条件唤醒
  - [x] time/uname/signal/locale 第一层：`clock_gettime/gettimeofday/nanosleep/time/localtime/strftime/uname/signal/sigaction/setlocale/nl_langinfo`
  - [x] signal 第二层：真实 signal delivery、mask、用户态 trampoline
  - [x] TTY/termios/poll/select 第一层：`tcgetattr/tcsetattr/ioctl(TIOCGWINSZ)/poll/select`
  - [x] regex/userdb stub 子集：`regcomp/regexec/regerror/regfree`、`getpwnam/getpwuid/getgrnam/getgrgid` stub
  - [x] 将上游 ToyBox 二进制作为 `/bin/toybox` 嵌入并在 AukOS 用户态链接（crt0+libc stubs）
  - [x] ToyBox argv[0] 分发语义：aush 通过 `toybox <applet>` 调用，toybox 内部 NEWTOY 分发
  - [x] 当前 12 个实测 applet：`basename cat dirname echo false head ls printf pwd true uname wc`
  - [x] `true`/`false` 通过 child 原始 exit status 分别验证为零/非零
  - [x] BIOS/UEFI 启动回归验证 quoted argv、选项、字符串和真实 ext4 文件输入
  - [x] ToyBox 源码/object 编译与 AukOS libc 最终链接均可独立传播失败
  - [x] Freestanding headers 全覆盖：9 个新头 + 大量补充声明/宏（errno 30+、unistd 40+、fcntl 20+、signal 30+、math 25+、sys/stat 15+、sys/mount 20+、netdb struct hostent、sys/syscall 300+ syscall 号、locale locale_t、stddef offsetof 等）
  - [x] libc stubs.c：200+ POSIX stub 函数（ENOSYS 或最小实现）

## Phase 3 — 文件系统与设备模型（P0→P1）

- [x] VFS 框架：path 解析、fd 表、权限最小模型
  - [x] 静态 path lookup：`/bin/hello`
- [x] `tmpfs`（P0）：先有内存文件系统便于 bring-up
  - [x] 文件读写（read/write），动态扩容
  - [x] `open(O_CREAT)` 创建新文件
  - [x] `syscall_write` 通过 fd 路由：fd 1/2 → 串口/控制台，fd 3+ → VFS
  - [x] `/bin/write_test` 自检程序验证写入→关闭→打开→读取闭环
- [x] 移除全局 `process_files[]`，全部走 `current_process->files[]`
- [x] 块设备抽象：RAM disk（P0），virtio-blk（P1）
  - [x] RAM disk 块设备抽象与读写自检
  - [x] virtio-blk（P1）
- [x] 磁盘文件系统：ext4 或 FAT（P1）
  - [x] ext4 superblock 只读识别与 RAM disk 自检
  - [x] FAT32 BPB/boot sector 只读识别与 RAM disk 自检
  - [x] 用户态 `fs`/`toybox fs` 显示 tmpfs/ext4/fat32 状态
  - [x] ext4 inode/block group/directory 读取（独立 fixture、extent、只读 `/ext4` VFS 挂载与损坏元数据自检）
  - [x] 受限可写 ext4：独立 `virtio1`、持久 `/work`、bitmap/count、depth-0 extent、目录 mutation 与 FLUSH
  - [ ] FAT32 FAT/root directory 读取

交付标准：

- [x] 能从文件系统加载用户程序与资源文件
- [x] 能写入日志/配置文件（`/bin/write_test` 通过 `open(O_CREAT)+write+read` 验证）

## Phase 4 — 网络基线（已暂停扩展）（P2）

- [x] P4 网络子系统骨架
  - [x] `net_init`、状态结构、自检日志
  - [x] socket libc/syscall API：UDP 可用，TCP 相关调用保留明确失败语义
- [x] 网卡驱动：virtio-net（优先）
  - [x] QEMU transitional legacy transport，双 split virtqueue polling TX/RX
  - [x] 同步单帧 Ethernet 接口与固定 gateway ARP cache
  - [x] 固定测试地址的真实 Ethernet ARP request/reply DMA 自检
  - [ ] 异步收发
- [ ] TCP/IP：ARP、IP、ICMP、UDP、TCP
  - [x] 固定 gateway ARP resolve 与以太网帧收发
  - [x] IPv4 包解析/封装（无 options、无 fragmentation）
  - [x] 内核启动 ICMP gateway echo 自检
  - [x] UDP datagram socket（固定拓扑、bounded polling、ring3 host echo 回归）
  - [ ] TCP 最小连接状态机
- [ ] DNS + 基础 socket API
- [ ] TLS（P2）：可先通过用户态库实现

交付标准：

- [x] 内核启动自检能通过真实 virtio-net DMA 完成 gateway ICMP echo
- [ ] 用户态能 `ping` 网关
- [ ] 恢复网络主线后，能从 HTTP 下载文件（哪怕是一个简单的用户态 wget）

## Phase 4.5 — 原生开发环境基础（当前主任务，P0）

用户虚拟内存与堆：

- [x] 进程 VM ownership：记录用户映射、物理页和权限，支持 exec/reap 完整释放
- [x] `brk`：可增长/收缩的 page-backed process heap
- [x] `mmap(MAP_PRIVATE|MAP_ANONYMOUS)`：页对齐、零填充、范围/重叠/溢出校验
- [x] `munmap` 与最小 `mprotect`，坚持 W^X，拒绝同时 writable + executable
- [x] `fork` 正确复制约定内的 heap/mmap 区，父子写入互不污染
- [x] libc allocator 从固定 64 KiB 静态数组迁移到 `brk`
- [x] 修正 `calloc` 乘法溢出、`realloc` 旧大小追踪、`free` 回收与复用
- [x] `/bin/dev_memory_test` 在 ring3 验证 1 MiB 以上分配、realloc、free reuse、匿名映射和 fork 隔离

Unix 工具组合能力：

- [x] bounded pipe open object，支持 `read/write/close/dup/fork`、EOF、broken pipe 和 backpressure
- [x] aush 支持 `|`、`>`、`>>`、`<`，拒绝未实现的 job control/组合 operator
- [x] 正确传递 `envp`，提供最小 `PATH`、`HOME`、`TMPDIR` 和 `getenv/setenv/unsetenv`
- [x] exec stack 扩展为 8 页，支持 64 argv、64 env 和 32 KiB table/string 数据
- [x] cwd/chdir 和统一 cwd-relative 路径规范化

可写开发工作区与文件 API：

- [x] 建立 bring-up tmpfs `/work` 与 `/tmp`；随后保留 `/tmp` 并用持久 ext4 覆盖 `/work`
- [x] 补齐工具链需要的 `rename/ftruncate/fsync/mkstemp` 和可靠 unlink-open-file 语义
- [x] 增加受限可写持久 ext4；双固件双启动、guest Vixc ELF 重执行与 host `e2fsck -fn` 验证
- [x] 验证从 `/work` 新生成文件执行静态 ELF，并移除 execve 对 tmpfs 私有布局的耦合

交付标准：

- [x] 双路径 smoke 输出 `[dev_memory_test] PASS`
- [x] 双路径 smoke 输出 `[pipe_test] PASS`、`[process_env_test] PASS` 和两项 shell PASS
- [x] shell 能完成大于 pipe buffer 的 `producer | consumer` 和 `/work` 重定向/追加/输入
- [x] ring3 程序能生成 `/work/generated.elf`，随后通过正式 `execve` 运行

## Phase 4.6 — 原生工具链 bootstrap（P0→P1）

按依赖从小到大推进，不能直接从完整 Clang/LLVM 开始：

- [x] 固定 AukOS x86_64 ELF、调用约定、crt0、静态链接和系统调用 ABI 文档
- [x] host Vixc 交叉构建最小 `/bin/vix_hello`，验证 Vix/C/syscall/argv/envp 双固件 ABI
- [x] Vixc stage 1：在 host 交叉构建静态 AukOS 版本并运行自身 lexer/parser/codegen tests
- [x] Vixc stage 2：在 AukOS 内把示例源码编译为 object/ELF，并执行生成程序
- [x] 移植 NASM：在 AukOS 内将 `hello.asm` 汇编为 ELF64 relocatable object
- [x] 提供严格受限的原生 linker，完成 ET_REL -> static ET_EXEC（LLD 仍未移植）
- [ ] 移植 `ar` 与最小 `make`，支持静态库和多文件项目
- [ ] 为 LLVM 补齐需要的 C++ runtime、TLS、atomics、pthread/futex、文件映射和大地址空间
- [ ] 移植 Clang/LLVM：先 `clang -c hello.c`，再 `clang hello.c -o hello`，最后在 AukOS 内运行产物
- [ ] 增加工具链 smoke，不能只检查 `--version`；必须编译、链接并执行有确定输出的程序

交付标准：

- [x] `/bin/vixc` 能在 AukOS 内生成并运行一个程序
- [x] `/bin/nasm` + linker 能在 AukOS 内生成并运行汇编程序
- [ ] `/bin/clang` 能在 AukOS 内完成 C source -> executable -> run 闭环

## Phase 5 — 图形与输入（图形应用与 MC 的硬门槛）（P0→P1）

最小图形：

- [ ] 帧缓冲/console：EFI GOP 或 VESA（P0）
- [ ] 键盘/鼠标：PS/2（P0）或 USB HID（P1）
- [ ] 窗口系统（选择其一）：
  - [ ] 方案 1：极简 compositor + 自有窗口协议（更可控）
  - [ ] 方案 2：Wayland-ish 子集（更贴近生态，但工作量大）
  - 选择方案二
加速与图形 API（可先慢后快）：

- [ ] 软件渲染（P0）：先把 UI 跑起来
- [ ] OpenGL 兼容（P1/P2）：
  - [ ] 先移植 Mesa llvmpipe（CPU）
  - [ ] 再考虑 virtio-gpu + virgl（QEMU 友好）

交付标准：

- 能跑一个简单图形 demo（渲染三角形/贴图）
- 输入事件稳定

## Phase 6 — 声音（MC 体验项）（P2）

- [ ] 音频输出：AC97/HDA 或 virtio-snd（优先 QEMU 友好）
- [ ] 用户态音频库（简单混音即可）

交付标准：

- demo 播放 wav

## Phase 7 — “能跑 Minecraft”的两条路线（选主线，另一条做备选）

### 路线 A：Linux 兼容层（更快见到 MC）（P1）

核心思路：在 AukOS 上实现足够的 Linux ABI，让 Linux 用户态能跑（尤其是 JVM、图形栈）。

- [ ] ELF 兼容：`PT_INTERP`、auxv、vdso（可简化）
- [ ] Linux syscall 覆盖：以 OpenJDK + LWJGL 需要的 syscalls 为目标集
- [ ] 线程语义：futex（关键）、TLS、信号
- [ ] `/proc` 与必要的伪文件系统子集
- [ ] 运行用户态栈（选择其一）：
  - [ ] 直接跑静态 busybox（先验证 exec/fork/pipe）
  - [ ] 再逐步引入动态链接与更完整的用户态

交付标准：

- 能在 AukOS 里运行一批 Linux CLI 程序
- 能启动 JVM 并跑一个简单 AWT/Headless demo

### 路线 B：原生用户态（更干净但更慢）（P2）

- [ ] 移植 libc（建议 musl 路线）
- [ ] POSIX 子集：pthread、mmap、文件/网络 API
- [ ] 移植 JVM 或替代运行时：
  - [ ] 优先尝试 OpenJDK 的最小配置
  - [ ] 或先跑更轻量的 Java 运行时作为过渡
- [ ] 移植 LWJGL/SDL 或等价库

交付标准：

- 原生跑起 Java 程序并能创建图形窗口

## Phase 8 — Minecraft 集成与体验完善（P1/P2）

- [ ] 资源与存储：缓存目录、配置目录、存档目录
- [ ] 输入映射：键鼠灵敏度、按键绑定
- [ ] 图形：分辨率切换、全屏、vsync（可选）
- [ ] 网络：登录/会话保持（与路线 A/B 相关）
- [ ] 崩溃报告：用户态 core dump/日志打包

交付标准：

- Minecraft 能进入主菜单
- 能创建世界并稳定运行一段时间

---

## 横切（任何阶段都应持续推进）

稳定性：

- [ ] 每阶段都有回归用例（QEMU 脚本化启动 + 断言输出）
- [ ] 栈溢出/越界检测：内核 UBSan/ASan（能用就用）

轻量：

- [ ] 可裁剪构建：不需要的子系统不编译/不链接

安全：

- [ ] 安全编译选项：stack protector、CFI（可选）、fortify
- [ ] 内核对象权限模型（句柄/能力）落地

可定制：

- [ ] 配置系统（Kconfig 或等价）+ 默认 profile
