# 文件系统正确性审计与交接记录

## 文档状态

- 审计日期：2026-07-19。
- 审计对象：当前 `refine/code_arch` 工作树中的文件系统正确性修改。
- 本文区分“已确认的问题”“已经合入工作树的修复”“实际运行验证”和“仍未提供的架构能力”。
- 当前活动文件系统路径中本轮发现的 correctness 缺陷均已处理；生产 clean build、项目 CI、boot-smoke 和三阶段磁盘 QEMU 已通过。详细结果见验证矩阵。

## 范围

本轮审计覆盖当前实际参与构建和运行的文件系统路径：

- `core/fs/vfs/`：路径解析、dentry 生命周期、挂载、创建和删除、通用文件操作。
- `core/fs/tmpfs/`：作为根文件系统使用的内存目录层。
- `core/fs/devfs/`：设备节点命名空间、字符和块设备节点发布。
- `core/fs/fd.c`、`core/fs/syscall_fs.c`：进程 fd 表、共享 open-file description 和文件系统 syscall 入口。
- `core/fs/frogfs/` 中由其 `Makefile` 实际编译的 `frogfs.c`、`inode.c`、`ffs_utils.c`、`ffs_super_block.c`。
- 为保证上述契约完整而必须检查的共享代码：块 I/O 和 MBR/EBR 分区发现、`core/lib/bitmap.c`、fork/thread 的 fd 继承与初始化、系统调用表和 x86 syscall 分发边界。
- `scripts/qemu-test.sh`、`core/fs/fs_regression.c` 和递归 Makefile：确保测试使用本次源码构建的镜像，并能跨 QEMU 冷启动验证持久化。

以下代码明确不在本轮修复范围：

- `core/fs/packagefs/`：当前在 `core/fs/Makefile` 中禁用，且项目规则把它定义为 legacy 设计参考。它没有迁移到当前 VFS、fd 和挂载契约。
- `core/fs/frogfs/fs.c`、`core/fs/frogfs/file.c`：不在当前 FrogFS `Makefile` 的对象列表中，是旧的单体文件系统实现，其中仍存在旧 `g_file_table` ABI 和保留 0/1/2 的假设，只作历史参考。
- `core/fs/pipe.c`：当前在 `core/fs/Makefile` 中禁用，并直接依赖旧的全局 fd 表模型。启用前需要作为独立任务迁移到当前 `struct file *` 和引用计数契约。
- 目录流 syscall 当前未接通，统一 syscall 缺省处理应明确返回 `-ENOSYS`，本轮不扩展 `opendir/readdir/closedir` 功能。

## 总体结论

原实现的问题不是单个 API 的局部错误，而是三个契约没有被同时维护：VFS 对 dentry/inode 的所有权、fd 对 `struct file` 的共享生命周期、FrogFS 对“磁盘数据已经提交到哪一步”的持久化顺序。冷缓存、fork、I/O 失败或损坏镜像会绕开原有 happy path，暴露释放后使用、越界 I/O、位图与 inode 不一致以及破坏性自动格式化。

当前工作树已经针对这些根因做了系统性修改，包括路径和对象边界检查、fd 引用计数、显式格式化、分区范围校验、磁盘结构一致性扫描以及失败回滚。clean build、共享临时磁盘的三次冷启动、一个确定性 metadata I/O 失败回滚 case 和损坏挂载不写盘证明均已通过。尚未提供的 journal、用户指针隔离、完整故障注入矩阵等能力列在“残余限制与风险”，不计作已经具备。

## 发现、根因与处理

### 1. VFS 路径和 dentry 生命周期

**问题。** 冷缓存 lookup 曾复用临时 dentry。后端 lookup 可能把该对象挂入父目录缓存，调用方随后释放或复用它，形成 UAF、链表损坏和错误路径命中。路径分量也缺少完整边界，长度为 255 或超长路径可能越过固定缓冲区；重复 `/`、尾随 `/`、`.` 和 `..` 的语义没有一致定义。

**根因。** VFS 没有规定 lookup 候选对象的所有权转移，也把“字符串可读”“路径合法”“分量可放入目标缓冲区”混为一件事。

**工作树中的修法。** 每个 cache miss 分配独立候选 dentry，只有明确由后端接管的对象才进入缓存；失败路径只释放仍由 VFS 持有的对象。入口统一验证绝对路径、总长度和分量长度，并拒绝当前不支持的空分量、尾随斜线和点路径。创建和 mkdir 通过统一的 parent lookup 构造子 dentry，不再在 syscall 层手工拼装半初始化对象。

### 2. 挂载顺序和对象所有权

**问题。** 原挂载路径可能在验证挂载点之前调用后端 mount。对 FrogFS 而言，这意味着一个错误路径也可能触发磁盘格式化。挂载完成时还曾覆盖或释放下层 inode，使挂载点和被挂载根目录的所有权混在一起。

**根因。** `mount_entry` 没有分别表达下层挂载点和上层根，且副作用发生在所有前置条件确认之前。

**工作树中的修法。** 先验证文件系统类型、挂载点存在、目录类型、非根和未被占用，再调用后端。`mount_entry` 分别保存 `mount_point`、`mounted_root` 和 `sb`，下层 dentry 保持原所有权。后端失败或上层根分配失败时调用 `put_super` 并释放 VFS 自己分配的对象。FrogFS 的格式化必须由显式 `FROGFS_MOUNT_FORMAT` 授权，普通 mount 遇到坏 magic 或非法几何只失败，不写盘。

### 3. 创建、删除和目录语义

**问题。** `O_EXCL` 被忽略，open flag 曾被窄化；mkdir 的 syscall 层会创建未完整初始化的 inode/dentry；unlink/rmdir 可能调用空回调、释放仍被打开的 inode、保留已删除 dentry，或者把文件和目录操作混用。失败后再次创建同名对象会命中陈旧缓存。

**根因。** 类型检查、命名空间缓存更新和后端持久化分别由不同层临时处理，没有“后端成功后才发布/移除”的统一提交点。

**工作树中的修法。** open flag 使用完整宽度，校验访问模式及 `O_CREAT/O_EXCL/O_TRUNC/O_DIRECTORY` 的组合。VFS 对 mkdir、unlink 和 rmdir 做父目录、类型、根、挂载、打开引用和非空检查，确认后端操作成功后才修改 dentry 树。打开中的对象返回 `-EBUSY`，关闭后才能 unlink，再次创建同名对象必须得到全新的有效 dentry/inode。

### 4. open-file description、fd 和 fork

**问题。** fork 曾让父子 fd 指向同一个 `struct file`，但没有共享引用计数。任一进程 close 都会释放对象，另一个进程随后 read/write/close 会 UAF 或 double free。原实现还把 0/1/2 当成隐含存在的标准流，但没有实际 `struct file` 或设备后端。

**根因。** 局部 fd、全局表槽位和 open-file description 三种生命周期没有分开。

**工作树中的修法。** `struct file` 增加 `f_count`；安装新 fd 时初始为 1，fork 对继承的每个有效 fd retain，close 先原子地移除本进程表项并递减引用，只有最后一个引用才调用后端 close 和释放对象。fd 分配、获取、retain/release 的全局表操作受 IRQ 临界区保护，错误统一为负 errno。

**当前明确限制。** 0、1、2 没有预装标准流。新线程的全部 fd 槽位初始化为 `-1`，分配遵循 lowest-free 规则，因此首次成功 open 可以返回 0；访问尚未安装的槽位返回 `-EBADF`。在 console/tty 通过 devfs 正式安装前，不能恢复指向不存在全局文件对象的假标准流映射。

### 5. 文件系统 syscall 分发

**问题。** x86 syscall handler 曾用用户可控编号直接索引并调用函数指针，越界或未注册项会跳到任意/空地址。部分文件 syscall 没有注册，错误还会被粗化。文件路径和缓冲区仍由内核直接解引用。

**根因。** syscall 表没有总数契约和默认实现，同时把 syscall ABI 校验与 VFS 操作混在一起。

**工作树中的修法。** 以 `SYS_NR_COUNT` 定义表边界，所有槽位先填入返回 `-ENOSYS` 的默认处理，再覆盖已实现项；汇编入口在间接调用前检查编号和空指针。`rmdir`、`ioctl` 接入当前 VFS，未实现目录流及其他 syscall 明确走 `-ENOSYS`。文件 syscall 保留具体的 VFS/fd 负 errno。

**不能在本轮诚实解决的安全边界。** `sys_open/read/write/ioctl` 等仍会直接使用 ring 3 传入的指针。仅在文件系统模块里添加一个名为 `copy_from_user` 的包装并不能形成安全边界：当前 ring 3/页表架构让用户态执行高地址内核链接代码，内核映射和递归页表也没有形成可靠的 supervisor-only 隔离，且 page fault 路径不能安全地把任意坏用户地址转换为 `-EFAULT`。正确修复必须先由 MM/进程/syscall 层完成用户与内核地址空间权限、范围验证和可恢复 fault 机制，再让 FS syscall 使用统一 `copy_from_user/copy_to_user`。在此之前，QEMU 文件系统测试只能证明可信测试指针下的功能正确，不能声称抵御恶意用户指针。

### 6. rootfs/tmpfs 和 devfs

**问题。** root inode、superblock、mount entry 和设备节点曾存在未清零字段；rootfs lookup/mkdir 没有稳定地建立目录 inode；devfs 可能在 inode/fops/devno 完成前发布节点，初始化和挂载错误也会被忽略。多级设备路径创建失败时可能留下半棵目录树。

**根因。** 初始化代码依赖分配器碰巧返回零内存，并在对象完成前把它加入全局可见链表。

**工作树中的修法。** 对 superblock、inode、dentry、mount entry 做确定性初始化，建立正确的根目录和 lookup/mkdir/rmdir 操作；devfs 验证相对路径、组件、设备类型和 major/minor，在节点 inode/fops/devno 完整后才发布。多级路径记录本次创建的首节点，失败时回滚本次子树。`root_fs_init`、`dev_fs_init`、mkdir 和 mount 的错误向上传递，启动代码不再把失败当成成功继续运行。

### 7. FrogFS 块边界和磁盘格式

**问题。** 原始块读写没有把区号严格限制在目标分区内，损坏的 superblock、inode zone 或目录项可能把 I/O 导向其他分区。普通 mount 在读取失败或 magic 不匹配时会自动格式化，既掩盖错误又可能破坏数据。格式化还曾过早写入有效 magic，失败后可能留下看似可挂载的半成品。

**根因。** 磁盘字段被当作可信内存字段使用，且“识别文件系统”和“创建文件系统”共用隐式失败分支。

**工作树中的修法。** 保持既有 on-disk absolute-zone 格式兼容，但所有读写同时校验磁盘范围、分区起止、zone 对齐、计数溢出和请求末端。mount 验证 superblock 几何、inode/dirent 尺寸、位图和 inode table 布局、最大文件大小及所有引用 zone。格式化先使 superblock 无效，初始化位图、inode table 和根目录，最后一步才写入有效 superblock。普通 mount 绝不自动格式化；测试 prepare 阶段只能对 disposable 镜像显式授权格式化。

### 8. FrogFS 位图、inode 和损坏检测

**问题。** 位图代码混淆 byte、bit 和 zone 单位，全满时可能越界；分配成功后 bitmap flush 失败仍会向上报告成功。inode 跨 zone 读写使用的缓冲区不足，初始化和释放路径也有 allocator 不匹配、泄漏和未初始化字段。磁盘指针、目录项类型/编号、重复 block 所有权和 namespace 引用缺少完整校验。

**根因。** 分配器只维护内存视图，持久化错误没有进入返回值；磁盘反序列化缺少不变量检查。

**工作树中的修法。** 位图按明确 bit limit 扫描并在锁/IRQ 临界区内更新，完整处理 full map，flush 失败回退内存位。inode 读写为跨 zone 情况分配足够缓冲区，统一初始化、缓存链表和释放规则。mount 扫描已分配 inode、direct/indirect table、zone bitmap、目录项、`.`/`..`、父子引用和重复 zone ownership，发现不一致返回 `-EUCLEAN`，而不是继续使用损坏指针。

### 9. FrogFS 数据和元数据提交

**问题。** create/mkdir/write/truncate/unlink/rmdir 由多次磁盘写组成。原实现忽略中间错误或以错误顺序释放 bitmap/inode/dirent，可能产生已分配但不可达的 block、目录项指向已释放 inode、截断后 bitmap 未释放，以及间接块泄漏。`O_TRUNC` 缺失，append 只在 open 时定位，负 lseek 会发生无符号回绕；`sync/statfs/remount` 还可能假成功。

**根因。** 操作没有保存提交前状态，也没有区分“数据写入”“inode 可达”“目录项发布”“位图释放”这些阶段。

**工作树中的修法。** 关键路径保存旧 inode、block map、目录 block 或位图状态，按可回滚顺序提交；后续失败时恢复旧磁盘状态，恢复本身失败则标记 `needs_fsck` 并让 sync 返回 `-EUCLEAN`。create/mkdir 只有 inode 和数据就绪后才发布目录项；unlink/rmdir 在目录、inode、bitmap 和 block 释放之间做错误传播和恢复。实现 `O_TRUNC`、每次 write 重新定位的 `O_APPEND`、有符号范围检查的 lseek 和实际 sync；尚未实现的 `statfs/remount/rename/link/symlink` 明确返回 `-EOPNOTSUPP` 或 `-ENOSYS`，不能假成功。

**必须保留的限制。** 这些回滚能处理被检测到的同步 I/O 失败，但 FrogFS 没有 journal、copy-on-write 事务、写屏障或 fsck 修复器。掉电、宿主崩溃、控制器重排或 torn write 可以发生在任意两次持久化之间，因此不能声称 create/write/truncate/unlink 具备掉电原子性。要提供该保证，必须另行设计日志或 COW 提交协议及恢复流程；本轮最多做到拒绝明显损坏的挂载、传播错误和标记需检查。

### 10. 块 I/O 和 MBR/EBR 分区发现

**问题。** 块请求只检查分区局部范围，未同时证明请求落在物理磁盘内；驱动返回正数成功值会穿透为上层错误语义。MBR 扫描边读取边发布设备，接受 type 0 的非空项、重复 extended container、错误类型的 EBR link、环路和重叠区间，损坏表可能发布别名分区或留下一半设备拓扑。

**根因。** 分区表字段被逐项信任，缺少“完整解析、交叉验证、最后发布”的事务边界；全局设备号也没有在发布前检查唯一性。

**修法。** `bio_read/write` 同时检查分区和物理磁盘的半开区间及加法溢出，并把所有非负驱动返回统一为成功 0。MBR/EBR 先解析到临时描述符，验证签名、type、boot flag、唯一 extended container、protective MBR、主分区和逻辑分区重叠、EBR/data/link 范围、visited EBR 环路及分区表扇区不能暴露为数据，然后才批量分配和发布；`add_partations_bdev` 额外拒绝重复 `dev_t`。坏表不再产生部分的解析拓扑。

### 11. FD 回归暴露的进程生命周期问题

**问题。** idle 已占用 PID 0，main 又被强制改写成 PID 0；main 退出释放该位后，fork 子进程可再次得到 PID 0，父进程把返回值 0 误判为子分支并自行 `exit`。`thread_exit` 还在释放 TCB 后读取 PID，并在 `need_schedule=false` 返回路径遗漏 IRQ restore。

**根因。** PID bitmap、TCB 字段和特殊进程编号由硬编码分别维护，退出资源顺序没有遵守对象释放后的不可访问规则。

**修法。** main 保留分配器给出的唯一 PID，退出前保存并释放 PID、随后才释放 TCB；false 返回路径恢复 IRQ。init 的真实 PID 在其进入 ready list 的同一 IRQ 临界区内登记，孤儿收养不再硬编码 PID 1。该修复让父/子两个相反 close 顺序的 fd 引用测试都能真实执行到 wait/exit。

## 流程问题与处理方案

### Clean build

过去的验证可能消费 `core/build/core.img` 或其他残留对象，导致“源码已坏但旧镜像仍能启动”。每个可提交单元必须从 clean 状态构建，且打包/QEMU 只能使用本次构建生成的 artifact。当前 `scripts/qemu-test.sh` 已在构建前执行 `make -C core clean`；本轮也运行了项目级 `./scripts/CI.sh`，确认递归 Makefile 和直接 kernel build 的依赖图一致。

### 三阶段 QEMU 冷启动

单次 boot 主要命中同一内核生命周期中的 dentry/inode cache，不能证明磁盘格式正确。`disk-smoke` 应在同一个临时目录内复用同一份 disposable `hd80M.img`，分三个独立 QEMU 进程执行：

1. `prepare`：显式允许格式化，使用固定路径执行 create/mkdir/open/read/write/append/truncate/unlink/rmdir；写入一个跨越 direct zone、进入 indirect table 且带非整 zone 尾部的持久化文件。prepare 必须保证目标是 disposable 且确实得到确定的初始格式，不能依赖源镜像碰巧无效。
2. `verify`：不带格式化 flag 冷启动，重新 lookup 并逐字节校验 prepare 数据，证明 superblock、位图、inode、目录项和 indirect block 能从磁盘重建。完成后按测试协议制造受控 superblock 损坏。
3. `corrupt`：再次普通 mount，必须可预期地拒绝损坏文件系统。启动前后对临时数据盘做 hash，证明失败 mount 没有自动格式化或写盘。

成功路径遵循 Unix 约定：runner 以退出状态和紧凑 result artifact 表示成功，不打印每个 PASS，也不保存详细成功日志。guest 只输出失败 case、panic/assert、必要的阶段失败信息；详细 `debugcon.log`、QEMU log 和 build log 默认只保留在失败 artifact 中。该协议已经实现，成功证据为 `build/qemu-test/*-result.json`，只有失败运行才会保留带时间戳的完整目录。

### 确定性故障注入与覆盖缺口

FrogFS 已加入只在 `CONFIG_QEMU_TEST` 下启用的确定性 hook，可按读/写、metadata/data 和调用序号让连续 N 次匹配 I/O 返回 `-EIO`，不依赖随机断电。当前自动回归实际执行一次 create metadata write 失败，断言返回 `-EIO`、命名空间无残留，清除 failpoint 后同名 create/unlink 成功。完整质量门还应扩展到：

- zone/inode bitmap flush 失败时分配回滚；
- 新 inode flush、目录项 publish 和父目录 inode flush 失败；
- `O_TRUNC` 在 inode 清零后、bitmap 释放失败；
- write 新 direct/indirect block 后 inode flush 失败；
- unlink/rmdir 在目录项移除、inode 清除、bitmap 或 block 释放失败；
- rollback 自身失败时 `needs_fsck`、sync 和后续 mount 的行为。

每个新增注入 case 都应在下一次冷 mount 后检查内容、可达 inode、位图占用和目录项一致性。当前机制已实现但矩阵只覆盖一个 metadata create 失败点，不能把这一项写成完整 failure matrix 已通过。

## 验证矩阵

| 层级 | 命令/场景 | 预期 | 当前结果 | 证据 |
| --- | --- | --- | --- | --- |
| 静态检查 | `git diff --check` | 无 whitespace/error | PASS | 命令退出 0 |
| FrogFS 单模块 | 生产和 `QEMU_TEST=1 disk-smoke/prepare` 强制重编 | 两种宏配置均可编译 | PASS | `make -B -C core/fs/frogfs` 对应配置退出 0 |
| kernel clean build | `make -C core clean && make -C core core` | 本次源码完整链接 | PASS | `core/build/core.img` 由 clean graph 生成 |
| 增量依赖 | no-op、touch `vfs.c`、touch `vfs.h` 后分别 build | no-op 不重链；source/header 正确重编并重链 | PASS | object/core.img mtime 依次验证 |
| 项目 clean smoke | `./scripts/CI.sh` | 项目入口 clean build 成功 | PASS | 命令退出 0 |
| QEMU boot | `./scripts/qemu-test.sh boot-smoke` | 进入 ring 3，runner 静默成功 | PASS | `build/qemu-test/boot-smoke-result.json` |
| QEMU prepare | 三阶段 `disk-smoke:prepare` | 显式格式化并完成语义 workload | PASS | `build/qemu-test/disk-smoke-result.json` |
| QEMU cold verify | 三阶段 `disk-smoke:verify` | 普通 mount，精确读回 12 KiB+37 B 跨 indirect-zone 数据 | PASS | 同上 |
| QEMU corrupt | 三阶段 `disk-smoke:corrupt` | mount 失败且临时数据盘 hash 不变 | PASS | 前后 SHA-256 均为 `4416a3d2...48a237d` |
| fd/fork | 父子分别先 close，另一方继续使用继承 fd | 最后引用只关闭一次，wait/exit 正常 | PASS | prepare ring 3 cases，失败会进入 result artifact |
| 错误语义 | path/flag/access/seek/open-unlink/recreate/rmdir cases | 返回具体负 errno，缓存与磁盘一致 | PASS | prepare kernel cases |
| I/O fault | create 的一次 metadata write 失败 | 回滚后无残留且可重试 | PASS（单点） | prepare `frogfs.metadata-rollback` |
| I/O fault matrix | truncate/write/unlink/rmdir/rollback-failure 全矩阵 | 冷启动后仍一致或明确 needs_fsck | 未完成 | 后续质量扩展项，不伪报 PASS |

## 残余限制与风险

- syscall 用户指针不是安全边界，必须等待 MM/页表/fault recovery 前置工作；当前只能测试可信指针。
- FrogFS 无 journal/COW/barrier/fsck，不能保证掉电原子性或自动修复，只能检测一部分不一致并拒绝继续。
- 0/1/2 标准流未预装；普通 open 会从最低空闲槽位（包括 0）分配，需要后续 tty/devfs 安装流程定义启动时的标准流。
- `opendir/readdir/closedir` 及多个扩展 syscall 未实现，统一返回 `-ENOSYS`；FrogFS 的 rename/link/symlink/statfs/remount 也保持明确“不支持”。
- FrogFS 磁盘目录项名称容量小于 VFS 的 255 字节上限，后端仍会对超出其 on-disk 格式上限的名称返回 `-ENAMETOOLONG`。这不是内存越界，但属于当前格式能力限制。
- VFS 尚无完整 umount 生命周期，当前只覆盖启动期挂载和失败清理；运行期卸载、busy 引用和缓存回收需要独立设计。
- 命名空间和 FrogFS 实例已引入串行化，但并发 fd read/write/close、多个进程共享 offset 和锁顺序仍需专门压力测试。
- 确定性 I/O failpoint 已可用，但 truncate/write/unlink/rmdir 和 rollback 自身失败的完整矩阵尚未自动化。
- MBR/EBR 的合法多逻辑分区链已随磁盘 QEMU 覆盖；恶意 type、重叠和环路输入目前依赖边界检查与只读复核，尚无独立的坏分区表 fixture profile。
- fork 创建过程的 OOM 失败清理、用户页复制失败传播和 PID bitmap 耗尽校验仍属于进程子系统遗留问题；本轮只修复了文件描述符继承测试实际触发的 PID、TCB 和 IRQ 生命周期错误。
- IDE 驱动仍有面向当前约 80 MiB 测试盘的硬编码 LBA 上限，`add_disk`/设备注册的部分 OOM 传播也不完整；这些是块驱动和设备生命周期后续任务，当前分区边界修复不能替代它们。

## 接手顺序

1. 从本轮 commit 和本文件开始接手；先看 `build/qemu-test/boot-smoke-result.json` 与 `disk-smoke-result.json`，成功运行按约定没有详细 `debugcon.log`。
2. 修改文件系统后先运行 `./scripts/CI.sh`，再运行适用的 `boot-smoke` 或完整 `disk-smoke`；不要直接复用 `core/build/core.img` 判断结果。
3. 扩展故障注入时保持一个 case 一个明确提交点，并在下一次冷 mount 检查位图、inode、目录项和内容，不要只检查当前 syscall 返回值。
4. 增加恶意 MBR/EBR fixture profile，覆盖 type 0、protective MBR、双 extended、重叠 logical 和 EBR cycle，断言不发布冲突 devfs 节点。
5. 用户指针隔离、journal/fsck、标准流、目录流和 umount 是独立后续里程碑，不要在局部 FS patch 中用空实现伪造成功。
