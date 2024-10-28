# 搭建 sel4 实验环境

## sel4 简介

sel4 是一个操作系统微内核，seL4 本身不是一个完整的操作系统。它提供很有限的 API，没有提供象传统的操作系统 Linux 那样的内存管理、页内外交换、驱动程序等等。

seL4 是一组基于为内核架构的操作系统内核，澳大利亚研究组织[NICTA](http://nicta.com.au/)创造了一个新的 L4 版本，称为 Secure Embedded L4（简写为 seL4），宣布在世界上率先开发出第一个正规机器检测证明（formal machine-checked proof）通用操作系统。

seL4 是第三代微内核操作系统，它基本是可以说是基于 L4 的，它提供了虚拟地址空间（virtual address spaces）、线程（threads）、IPC（inter-process communication）受 EROS、KeyKOS、CAP 等操作系统设计的影响，seL4 的控制机制是基于 capabilities 的,capabilities 机制提供了访问内核对象（kernel objects）的方法，这种机制使得seL4 与其它 L4 比起来，显示出一定的高效率。

seL4 的设计目标：
    1. 减少内核代码量，减少内核代码的维护成本，减少内核代码的漏洞。
    2. 安全
    3. 高性能微内核
    

## How to start
[GettingStart官方文档](https://docs.sel4.systems/GettingStarted.html)

[安装依赖](https://docs.sel4.systems/projects/buildsystem/host-dependencies.html)

利用repo库，执行以下命令即可获取seL4源码。
``` 
mkdir seL4test
cd seL4test
repo init -u https://github.com/seL4/sel4test-manifest.git
repo sync

```

seL4 编译运行(以 x86_64 为例)
```
mkdir build-x86
cd build-x86
../init-build.sh -DPLATFORM=x86_64 -DSIMULATION=TRUE
ninja
```

使用Qemu模拟：
```
./simulate
```

[seL4-tutorials源码获取](https://docs.sel4.systems/Tutorials/)

seL4-tutorials是帮助初学者学习seL4的教程。
```
mkdir sel4-tutorials-manifest
cd sel4-tutorials-manifest
repo init -u https://github.com/seL4/sel4-tutorials-manifest
repo sync
```

## Hello World
sel4-tutorials里有一些待完成的代码，需要自行填写相应的部分来完成实验。使用以下命令编译实验。
```
# creating a Tutorial directory
mkdir tutorial
cd tutorial
# initialising the build directory with a tutorial exercise
../init  --tut hello-world
# building the tutorial exercise
ninja
```
最后执行 ./simulate 即可运行程序。
