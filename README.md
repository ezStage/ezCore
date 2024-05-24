# ezCore

## 介绍

ezCore 是一个C语言开发的工具库，提供了如下几个功能：
1. 一个基于标准Makefile和shell脚本的编译框架（ezBuild），具有简单的软件包管理功能。详见doc/00-build.md。
2. 一个支持垃圾自动回收的内存管理系统（ezGC）。详见doc/01-runtime.md。
3. 一个基于I/O多路复用接口的事件驱动任务系统（ezTask）。详见doc/01-runtime.md。
4. 一种类似于 yaml 格式和 json 格式的 ezml 格式文件（ezML）
5. 一些常用的数据结构，比如字符串String，链表List，Key/Value键值映射Map等

## 配置

编译前，通过配置命令指定硬件平台

`make config PLAT=x64-linux-gcc` 或 `make config PLAT=x86-linux-gcc`

默认配置命令 `make config` 相当于 x64-linux-gcc

## 编译

运行 `make`

## 安装

首先修改 Makefile 文件中安装路径：

EZ_PKG_PATH :  ezCore 将拷贝到 `$(EZ_PKG_PATH)/ezCore`

EZ_USR_PATH :  `$(EZ_PKG_PATH)` 目录中的多个软件包安装到 `$(EZ_USR_PATH)` 目录。详见 doc/00-build.md

运行 `make install`

