# 下一个 AI 的接力提示

## 项目方向（2026-07-22）

AukOS 采用 **Vix + C17 + x86_64 ASM** 混合开发。长期目标是在 AukOS 内完成
`source -> object -> static ELF -> exec`，随后再移植 NASM、LLD、ar/make 和 Clang/LLVM。

上一轮已经完成可靠 `/work` 文件 API、通用 VFS ELF reader、generated ELF exec 和最小
Vix host-cross 探针。下一主线不是继续增加普通命令，也不是重写稳定内核，而是推进
**Vixc bootstrap**：拆出不依赖 LLVM/LLD/host process 的 freestanding 编译器核心，让
AukOS 内的 Vixc 读取 Vix 源码，执行 lexer/parser/typecheck/MIR/self-codegen，生成标准
x86_64 ELF object，并通过严格受限的静态链接闭环生成可立即 `execve()` 的 ELF。

如果交叉构建暴露 Vixc codegen、ABI、target 或 parser bug，必须在 `user/vixc`
修复 Vixc，并增加回归测试；不能在 AukOS loader、runtime 或 Makefile 中伪造结果。如果现有
`--target` 对这个阶段不够，扩展 Vixc 的正式 target/backend 能力，不能用文本替换产物。

## 上一任务已经完成

### VFS/tmpfs 与文件 API

- VFS 使用 64 槽可回收 `vfs_file` pool；descriptor 仍是 tagged、引用计数 open object
- tmpfs 使用 64 个可复用 node、每目录 64 children、单文件 1 MiB 上限和非连续 4 KiB pages
- linker-owned `/bin` backing 与 `/work` owned pages 分离，静态 backing 拒绝 write/truncate
- unlink/rename replacement 支持 open-fd deferred reclaim，最后 close 后归还 node 和 pages
- `O_EXCL`、access mode、每次 write 的 `O_APPEND`、seek hole、overflow 和失败原子性已冻结
- Linux x86_64 `fsync=74`、`truncate=76`、`ftruncate=77`、`rename=82` 与 libc wrapper 已实现
- libc `mkstemp` 使用 `O_CREAT|O_EXCL|O_RDWR`、0600、64 次有界 collision retry
- ext4 继续只读；tmpfs `fsync` 是经过 descriptor/type 验证的成功 no-op

详细 ownership、错误和生命周期契约见 `docs/file-api.md`。

### 通用 ELF exec

- `kernel/syscall.c` 已删除 tmpfs 私有 struct 和 `inode->private_data` 强转
- ELF loader 使用 `read_at(context, offset, buffer, size) + file size`，不要求连续 image
- exec 经过正常 VFS open/read-at/close，失败不提交新 address space
- memory reader、tmpfs page-backed reader和 short/error reader均有 host tests
- `/bin/file_api_test` 在 ring3 将自身逐块复制到 `/work/generated.elf`，child 真实 exec 后打印：

  ```text
  [generated_exec_test] PASS
  [file_api_test] PASS
  ```

- ring3 同时覆盖 access/open flags、truncate、rename/unlink、mkstemp collision、shared offset、
  128 轮 namespace reuse 和 256 KiB 反复 grow/shrink/page reuse

### 最小 Vix host-cross

- host `user/vixc/build/vixc` 支持：

  ```sh
  vixc --target x86_64-unknown-none -obj input.vix -o output.o
  ```

- AukOS Makefile 已有 `.vix -> ELF64 ET_REL -> static ET_EXEC` 路径
- `/bin/vix_hello` 由 Vix object 与 C syscall shim 链接，无 undefined symbol、`PT_INTERP`、TLS
  或动态库，使用现有 SysV x86_64 `_start(argc, argv, envp)` ABI
- Vix 已在 ring3 检查 argc/argv/envp、调用 C runtime/syscall，并在 BIOS/UEFI 打印：

  ```text
  [vix_hello] PASS
  ```

ABI 见 `docs/vix-abi.md`。当前 `user/hello.vix` 还有用户加入的 `Hello` 循环输出；不要
把它当成编译器 bug或静默删除，只以 marker 与退出码作为 smoke 判定。

### 已通过的最终验收

上一轮从 `make clean` 开始按任务要求完整通过：

```sh
make clean
make toybox-aukos-config
make toybox-aukos-port
make
make test
make check
make iso
make build/aukos_uefi.elf
make iso-uefi
make smoke
make smoke-uefi
```

当前 BIOS/UEFI 日志同时包含：

```text
[signal_test] PASS
[dev_memory_test] PASS
[pipe_test] PASS
[process_env_test] PASS
[generated_exec_test] PASS
[file_api_test] PASS
[vix_hello] PASS
[udp_test] PASS
[toybox_test] true/false exit status PASS
[toybox_test] default command dispatch PASS
[shell_test] pipeline PASS
[shell_test] redirection PASS
```

pipeline 输出仍为 `12`，重定向读取仍为连续 `first`、`second`。BIOS/UEFI UDP pcap 均通过
真实 ARP/IPv4/UDP checksum、virtio TX/RX completion 和 host echo 检查。

## 必须保留的系统边界

- 不改动 `arch/x86_64/syscall_entry.asm` 的独立 resume stack 与 CR3 switch 恢复规则
- scheduler run queue 保持固定容量且无重复 entry；wait/pipe 不能退回 busy-wait
- exec 使用独立 8 页 stack 与 8 页 signal stack，保持 SysV 对齐和现有 argv/envp 上限
- `waitpid` 必须 reap，不能耗尽 16-slot process table
- descriptor 必须保持 tagged/refcount open object；dup/fork 共享 file offset
- VFS/tmpfs 固定 pool、deferred reclaim、page rollback、静态 backing ownership不能退化
- generated toolchain artifacts 必须经过 `/work`、VFS、ELF reader、exec、scheduler 和 descriptor
- 不能让 kernel识别 Vix私有格式、特判生成路径或直接调用嵌入的 Vix函数
- pipe 4096-byte backpressure、EOF/broken-pipe/wakeup语义不能退化
- aush parser/pipeline fd close、ToyBox 12 applet和现有 shell 输出不能退化
- UDP 必须继续走真实 virtio-net/pcap路径，不能合成 reply
- BIOS/UEFI 必须保留 SSE CR0/CR4 配置；VGA font不能依赖系统 consolefont
- 不放宽 host/ring3/smoke断言换取通过

## 仓库与工作树约束

### AukOS

- 顶层 Git repository 是 `https://github.com/vixlang/AukOS.git`
- `toybox`、`user/vixc` 与 `user/nasm` 作为普通 vendored source 由顶层仓库跟踪，
  不是 submodule；本机保留的嵌套 `.git` 只用于追踪来源
- `toybox` 的本地定制必须保留：

  ```text
   M main.c
   M toys/posix/ls.c
  ?? aukos_miniconfig
  ?? toybox-aukos
  ```

- 禁止对 `toybox` 执行 reset、checkout 或 clean
- AukOS Makefile 没有自动 header dependency；最终验收必须从 `make clean` 开始

### Vixc

- AukOS 定制 Vixc 位于 `user/vixc`；不得向 Vix-lang 仓库提交这些定制
- 当前已有用户修改/未跟踪文件，必须保留：

  ```text
   M seed/vixc.ll
   M test.vix
  ?? test
  ?? test.sh
  ```

- 禁止 reset/checkout/clean；尤其 Vix makefile 的 `clean` 会删除用户未跟踪的 `test`
- 修改 compiler source 时不要顺手刷新 `seed/vixc.ll`，除非新实现确实要求、完整 bootstrap
  验证通过且能证明不会覆盖用户修改
- 不得删除本机嵌套 `.git`；顶层 AukOS 仓库仍以普通文件提交当前 Vixc 快照

### 验收证据

- AukOS 日志与 pcap：`build/bios-smoke.log`、`build/uefi-smoke.log`、
  `build/bios-udp.pcap`、`build/uefi-udp.pcap`
- smoke 脚本会复制 virtio fixture，避免交互 QEMU 的 raw image 写锁

## 当前实现的准确缺口

### 1. AukOS Vix runtime 仍是单程序 shim

- `user/entry.c` 与 `user/runtime.c` 提供 AukOS entry 和运行时桥接
- 没有共享的 AukOS Vix runtime object/module和稳定 symbol whitelist
- Vixc 需要 allocator、string、diagnostic、文件读取/写入、seek、remove 与 argv/envp
- `user/libc/stdio.c` 的 `fopen()` 仍直接返回 null
- `user/libc/stubs.c` 的 `fread()` 返回 ENOSYS，`fputs()` 忽略 stream 并写 fd 1
- 因此把 host Vixc object链接进 AukOS即使成功，也不能真实读写 source/artifact

### 2. 当前 Vixc 不是 freestanding compiler binary

- `src/main.vix` 无条件 import `codegen.vix`、LLVM builder、llc、passes 和 linker frontend
- host `build/vixc` 动态依赖 LLVM、LLD、libstdc++、pthread、host libc等
- 当前 host binary约 1.04 MiB，BSS约 5.6 MiB；不能把依赖一起假装成 AukOS runtime
- `user/vixc/src/helper.c` 同时混合通用字符串/diagnostic helper与大量 LLVM C API
- 必须拆分 compiler core / LLVM host driver，不能靠 unused symbol侥幸或 loader忽略 undefined

### 3. self backend 仍依赖 host工具

- `--backend=self` 和 `--backend=self-lir` 目前先写 NASM文本
- `src/main.vix` 随后调用 `system("nasm -f elf64 ...")`
- executable模式继续调用内嵌 LLD API
- AukOS 内没有 `system()` 可启动 host工具，也尚无 NASM/LLD
- 当前 self backend没有标准 ELF64 object writer、relocation writer或 native static link path

### 4. Vixc 尚未在 ring3运行编译器核心

- `/bin/vix_hello` 只证明普通 Vix函数与 C/syscall ABI
- 尚未在 AukOS 运行 lexer/parser/typecheck/MIR/self-codegen
- 尚未由 `/bin/vixc` 读取 `/work/*.vix` 并生成 object
- 尚未由 guest生成标准 ET_EXEC并通过正常 exec运行

### 5. 构建可复现性仍不足

- 尚无 Vix compiler version/capability gate
- 尚无 `readelf/nm` artifact contract检查，不能阻止 host libc/TLS/dynamic依赖混入
- tmpfs动态单文件上限为 1 MiB；如果 Vixc output或runtime object超过上限，必须明确扩容并
  保持 page ownership/rollback测试，不能静默截断或绕开 VFS

## 下一主任务：Vixc freestanding core 到 guest generated exec

最终必须在真实 AukOS ring3 完成：

```text
/work/hello.vix
  -> /bin/vixc (lexer/parser/typecheck/MIR/self backend)
  -> /work/hello.o (standard x86_64 ET_REL)
  -> restricted static link with AukOS Vix runtime
  -> /work/hello.elf (standard static ET_EXEC)
  -> fork/execve/waitpid
  -> generated child validates argv/envp and prints PASS
```

BIOS 与 UEFI smoke最终必须新增：

```text
[vix_runtime_test] PASS
[vixc_frontend_test] PASS
[vixc_object_test] PASS
[vixc_generated_exec_test] PASS
```

不能用 `--version`、预嵌入 host产物或仅写 assembly文本代替闭环。

## Vix + C + ASM 职责

### Vix

- compiler driver、lexer/parser/typecheck、MIR/self backend、ELF object metadata和受限链接策略
- 新增用户态工具业务逻辑
- 所有 compiler失败必须返回非零并打印确定 diagnostic

### C17

- AukOS libc/POSIX、共享 Vix runtime、syscall与文件 I/O veneer
- host-testable byte buffer、checked arithmetic或 ELF primitive可在有明确 ABI时用 C
- 内核/VFS/ELF loader保持 C，不因 Vixc bootstrap重写

### ASM

- 只保留 `_start`/syscall/context等必须精确控制寄存器的边界
- 不在手写 ASM中实现 parser、object writer或linker policy

## 实施要求

### 1. 建立共享 AukOS Vix runtime

- 将 `vix_hello_runtime.c` 拆成共享 runtime与每程序最小 entry；`vix_hello` 改用同一 runtime
- 冻结 Vix/C ABI：整数宽度、pointer/string、argc/argv/envp、错误返回和 allocator ownership
- 至少提供 `write_str`、`exit`、malloc/calloc/realloc/free、strlen/memcpy/memset/strcmp
- 为 compiler提供真实 open/read/write/close/lseek/fsync/unlink/rename文件路径
- 实现 compiler实际使用的 `fopen/fclose/fread/fwrite/fputs/fseek/ftell/remove`，mode和short I/O
  必须真实；不能继续 ENOSYS或无条件成功
- stdio写入必须尊重 `FILE->fd`，读取/seek失败必须传播，close不能重复泄漏 descriptor
- host tests覆盖 mode mapping、partial read/write、EOF、seek、invalid/closed stream和失败清理
- 新增 `/bin/vix_runtime_test`，在 `/work` 做多页读写、seek/reopen和allocator reuse

### 2. 拆分 Vixc freestanding compiler core

- 不直接把当前 LLVM/LLD host `build/vixc`链接进 AukOS
- 建立明确的 freestanding entry，例如独立 `aukos_main.vix`/driver，只 import：
  lexer、parser、semantic/typecheck、desugar、MIR、self x86_64 backend和必要通用 helper
- LLVM builder/codegen、Passes、Llc、Linker C++库不能出现在 AukOS artifact的依赖图
- 将 `helper.c` 中通用 helper与 LLVM helper拆开；共享部分不得 include/link LLVM
- target固定为正式的 `x86_64-unknown-none`/AukOS profile，SysV ABI与现有文档一致
- CLI至少支持 `--lex`、`--parser`、`--check`、`--debug=mir`、`-S`、`-obj`、`-o`
- source/read/import失败、parse/type/codegen失败和output失败都返回不同的确定非零路径
- host构建后必须用 `readelf`、`nm -u`、`objdump`检查无 `PT_INTERP`、TLS、LLVM/LLD、
  pthread、C++ runtime和未批准 host symbol

### 3. self backend 直接生成标准 ELF64 ET_REL

- `-obj` 在 compiler进程内生成 ELF64 little-endian `EM_X86_64 ET_REL`
- 不允许调用 `system()`、`nasm`、`as`、`ld`、LLVM MC或复制预生成 object
- object至少有合法 ELF header、section table、`.text`、`.rodata`、`.data`（按需）、
  `.symtab`、`.strtab`、`.shstrtab`和所需 `.rela.*`
- symbol binding/type/section index、alignment、offset/size和 relocation addend必须 checked
- 初始 relocation可限制为 self backend和runtime实际需要的 x86_64集合，例如 PC32/PLT32/64；
  遇到未支持指令或 relocation必须明确失败，不能生成损坏 object
- 优先让 machine backend输出结构化 instruction/data/symbol，而不是重新解析自由格式 NASM文本
- object writer核心必须 host-testable：golden header/section/symbol/relocation、overflow、duplicate
  symbol、undefined symbol、short write和allocator failure
- host侧使用 `llvm-readelf`/`readelf`解析，并至少用 host `ld.lld`链接后运行多个 Vix样例，证明
  object不是只被自家工具接受
- Vix-lang现有 self tests不能退化；至少运行 `python3 tests/run.py --self`，并对相关 parser/
  macro/codegen bug增加最小回归。若运行完整 `tests/smoke.py`成本可接受，最终必须运行

### 4. 提供严格受限的 guest static link

- 在 AukOS 内将 guest生成的 app object与固定 `/lib/vix/runtime.o`链接为标准 x86_64 `ET_EXEC`
- 可以是 Vixc内置 restricted linker或独立 `/bin/vixld`，但输入输出必须是标准 ELF
- 首轮只支持单个 app object + 单个 runtime object、已定义 relocation集合和现有 user地址布局
- 必须解析/验证 ELF，而不是按固定offset拼接或patch模板 executable
- 输出至少有分离的 RX与RW `PT_LOAD`，保持 NX/W^X，entry指向正常 `_start`
- duplicate/undefined symbol、unsupported relocation、overflow、overlap、short read/write、pool/page
  exhaustion必须失败且不留下可执行的半成品；用 temp + fsync + rename提交最终 ELF
- 不修改 kernel ELF loader来迁就 linker；host `readelf`也必须接受输出

### 5. 做真实 ring3 Vixc 回归

新增静态 orchestrator `/bin/vixc_test`，不能由 parent伪造 child marker：

1. 在 `/work/vixc-test/` 写入多个真实 Vix source，包括成功、lexer error、parser error和type error
2. fork/exec `/bin/vixc --lex/--parser/--check`，检查退出码和确定输出
3. 用 self backend编译含函数调用、branch/loop、string literal、argv/envp和C runtime call的程序
4. 生成 `/work/vixc-test/hello.o`，重新打开并验证 ELF/section/symbol/relocation基本结构
5. guest restricted linker生成临时 ELF，fsync后rename为 `/work/vixc-test/hello.elf`
6. child用真实 argv/envp/cwd执行该 ELF
7. 只有 generated child验证参数、环境、文件读取和runtime syscall后打印
   `[vixc_generated_exec_test] PASS`
8. parent wait/reap并确认零退出，再打印 frontend/object总 marker
9. 至少重复 16 轮 compile/link/exec/unlink，证明 VFS/node/open/page/process资源可复用

marker文本不能存在于 parent orchestrator的输出路径中；generated child source可以包含 marker，
但必须由 guest Vixc本轮生成的 ELF执行后打印。

### 6. 收紧构建和 smoke

- 把 Vix compiler path改成可覆盖且有 capability/version检查的构建输入，避免只有作者机器可用
- Makefile明确表达 compiler-core source -> AukOS object -> `/bin/vixc`
- 加 artifact audit target，检查 `/bin/vixc`与generated fixture的 ELF、undefined whitelist和大小
- `tools/check_qemu_log.sh`要求四个新 marker并禁止对应 FAIL、undefined symbol、unsupported
  relocation、parse/type/codegen error（成功向量）、resource exhaustion、deadlock文本
- BIOS/UEFI都必须运行全部新旧回归；两份 UDP pcap继续检查
- 更新 `docs/vix-abi.md`，新增 `docs/vixc-bootstrap.md`，记录 compiler split、runtime ABI、
  object/relocation/link contract、固定上限和失败原子性
- `TODO.md`只勾选 ring3双路径真正证明的 stage

## 明确禁止的替代方案

- 不把 host LLVM、LLD、glibc、libstdc++或动态 loader整体塞进 AukOS
- 不在 guest通过 `system()`调用 host NASM/ld，不借助 QEMU host共享目录代编译
- 不预先嵌入目标 hello.o/hello.elf后声称是 guest生成
- 不复制模板 ELF后只patch字符串、exit code或marker
- 不添加 Vix私有 executable/object格式，不给 kernel loader加Vix special case
- 不把 compiler marker写进 kernel、aush或parent test程序
- 不为了容纳 artifact取消 tmpfs上限；需要增大时要集中定义、checked并扩充ownership tests
- 不用 Vix重写 boot、syscall/context switch、PMM/VMM、scheduler、VFS、ELF loader或稳定driver
- 不在本轮移植完整 NASM、LLD、GCC、Clang/LLVM、ar/make
- 不扩展 TCP/DNS、GUI、动态链接、TLS、thread/pthread/futex、shared library或JIT

## 建议落地顺序

1. 共享 Vix runtime + 真实 FILE I/O host/ring3 tests
2. Vixc source依赖图拆分，构建无 LLVM符号的 freestanding compiler core
3. 在 host运行 core的 lexer/parser/typecheck/MIR/self-asm regression
4. 结构化 x86_64 emitter +标准 ET_REL writer，先由 host readelf/ld.lld验收
5. host-cross `/bin/vixc`，在 AukOS生成并验证 `/work/*.o`
6. restricted static linker + temp/fsync/rename原子提交
7. `/bin/vixc_test` 完成 guest source -> object -> ELF -> child exec
8. BIOS/UEFI完整 smoke、pcap、文档和TODO

不要把第3步或 `vixc --version`当作完成；最终状态必须达到第7步。

## 最终验收

### Vix-lang

在不删除当前用户文件的前提下，至少执行并报告：

```sh
cd /home/zty/AukOS/user/vixc
python3 tests/run.py --self
python3 tests/smoke.py
```

如果完整 smoke因既有无关问题失败，必须列出准确失败用例；不能用缩小测试集合支持“Vixc无回归”结论。

### AukOS

必须从干净构建按顺序执行：

```sh
cd /home/zty/AukOS
make clean
make toybox-aukos-config
make toybox-aukos-port
make
make test
make check
make iso
make build/aukos_uefi.elf
make iso-uefi
make smoke
make smoke-uefi
```

最终说明必须列出：共享 runtime/stdio failure vectors、compiler dependency audit、lexer/parser/
type/MIR vectors、ET_REL section/symbol/relocation与failure vectors、restricted linker vectors、16轮
guest compile/link/exec reuse、generated child输出、全部旧 regression、BIOS/UEFI日志和两份 UDP pcap。
任何步骤未运行或失败都要明确报告。
