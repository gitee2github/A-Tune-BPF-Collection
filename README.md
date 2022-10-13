# A-Tune-BPF-Collection

#### 介绍
A-Tune-BPF-Collection是BPF工具集，这些BPF程序用以跟踪内核行为模式以实时细粒度地调整内核参数，达到提升系统性能的目标。

#### 软件架构
以`readahead_tune`为例来介绍A-Tune-BPF-Collection中的BPF程序。

`readahead_tune`包含两部分：

* BPF program(`readahead_tune.bpf`)：加载到内核的BPF program以跟踪ext4/xfs文件系统的文件读操作
* BPF control program(`readahead_tune`)：读取配置文件，配置BPF program的参数，随即加载BPF program到内核

#### 安装教程

通过rpm命令或者yum安装`A-Tune-BPF-Collection` rpm包：

```
# yum install A-Tune-BPF-Collection
or
# rpm -ivh A-Tune-BPF-Collection-{version}.x86_64.rpm
```

#### 使用说明

以`readahead_tune`为例介绍A-Tune-BPF-Collection中的BPF程序程序使用：

1. （可选）默认配置文件会安装在`/etc/sysconfig/readahead_tune.conf`，也可以自己新建配置文件，通过`start_readahead_tune`命令`-c|--config`选项指令配置文件路径。若未指定配置文件，则会使用默认安装的配置文件。

   ```
   注意：仅支持通过完整路径名指定配置文件，相对路径会无法识别；配置文件中若存在不合法的选项配置或者配置选项缺失，都会使用该选项的默认配置值。
   ```

2. 通过`start_readahead_tune`命令启动/加载`readahead_tune.bpf` BPF Program。命令使用方法可以使用`start_readahead_tune -h|--help`帮助命令。
3. 通过`stop_readahead_tune`命令停止/卸载`readahead_tune.bpf` BPF Program。

#### 源码编译
1. 安装编译依赖：
```bash
sudo dnf install make clang llvm libbpf-devel dwarves kernel-debuginfo bpftool -y
```
2. 编译:
```bash
make
```

编译过程中可能遇到`kenrl-debuginfo`rpm包提供vmlinux路径与`uname -r`不一致的情况导致编译失败。这是由于当前运行的内核版本与`kernel-debuginfo`的版本不一致。openEuler yum源的kernel版本较高，可以执行`dnf update kernel`升级当前kernel到yum源的kernel版本，重启系统，kernel版本便与`kernel-debuginfo`版本一致，再重新编译。

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解 Gitee 上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5.  Gitee 官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
