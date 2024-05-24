## 1. 编译系统

ezCore自带了一个简单的编译框架ezBuild，既可以用于ezCore自己的编译，也可用于其他项目的源代码编译工作。

### a. 准备工作

我们假定用户下载了ezCore在x86_64硬件平台的二进制包：pkg_x64.tgz

1) 我们可以在任意目录解压，比如 /home/test 目录
   
   ```shell
   cd /home/test
   tar -xzf pkg_x64.tgz
   ls /home/test/pkg_x64/
       ezBuild    ezCore
   ```

ezBuild目录包含了编译框架的相关文件

ezCore目录包含了ezCore的相关文件（包括头文件，动态连接库，文档和demo等）

2) 还需要准备一个usr目录，比如 /home/test/usr 目录
   
   ```shell
   cd /home/test
   ./pkg_x64/make_usr.sh ./usr
   ```

该脚本通过检测自己所在的目录作为pkg目录，第一个参数路径作为usr目录，创建并初始化usr目录，准备工作就做完了。如果以后你改变了pkg_x64的目录路径，需要重新初始化usr目录。

### b. 编译一个 hello_world 程序

ezBuild编译框架基于标准的Makefile规则和shell脚本，在终端环境中运行。在编译程序之前需要先运行如下命令，建立编译环境：

```shell
source /home/test/usr/env.sh
```

env.sh文件就是设置了 EZ_USR_PATH、EZ_PKG_PATH、PATH、LD_LIBRARY_PATH 等环境变量，每个终端只需要设置一次。

我们把要编译的程序源代码按照如下方式组织目录结构：

| top_dir / |           |            | 顶层目录              |
| --------- | --------- | ---------- | ----------------- |
|           | include / |            | 存放对外公开的头文件        |
|           | doc /     |            | 存放对外公开的文档文件       |
|           | local /   |            | 存放自己的私有资源文件，运行时只读 |
|           | var /     |            | 存放运行时产生或会修改的资源文件  |
|           | src /     |            | 源代码目录，目录名和个数没有限制  |
|           |           | hello.c    | 源代码，文件个数没有限制      |
|           |           | libhello.c | 源代码，也可以有子目录，没有限制  |
|           | Makefile  |            | 编译项目配置文件          |

上面的子目录如果没有可以不用设置。Makefile就是项目的编译配置文件，内容如下

```makefile
STATIC_TOP_PATH := .
#==============================================
include $(EZ_USR_PATH)/install/ezBuild/init.mk
#==============================================

LOCAL_NAME := libhello
LOCAL_LIB_VERSION := 1.0
LOCAL_SRC_FILES := src/libhello.c
include $(BUILD_SHARE_LIBRARY)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := hello
LOCAL_CFLAGS :=
LOCAL_LDFLAGS :=
LOCAL_SRC_FILES := src/hello.c
include $(BUILD_EXECUTE)

#==============================================
config: distclean
    mkdir -p $(STATIC_BUILD_PATH)
    cp -a config.mk $(STATIC_BUILD_PATH)/

install: std_install_pkg
    $(USR_INSTALL) $(EZ_USR_PATH) $(EZ_PKG_PATH)/$(EZ_PKG_NAME)

remove: std_remove_pkg
    $(USR_REMOVE) $(EZ_USR_PATH) $(EZ_PKG_PATH)/$(EZ_PKG_NAME)
```

Makefile中必须指定 STATIC_TOP_PATH，这是整个源代码的顶层目录。

LOCAL_SRC_FILES 指定的源代码文件都是相对于该Makefile所在的目录。

LOCAL_LIB_VERSION 版本号不参与soname指定。

一个Makefile可以编译多个bin和lib，`include $(BUILD_EXECUTE)`是编译bin文件，`include $(BUILD_SHARE_LIBRARY)`是编译动态链接库文件，`include $(BUILD_STATIC_LIBRARY)`是编译静态链接库文件。在每两次包含编译文件之前必须有

`include $(CLEAR_LOCAL_VARS)`。

编译过程中生成的所有文件都在"top_dir/_build/"目录中。

直接在 top_dir 目录下运行` make `命令，生成的内容如下：

| top_dir / |          |         |                 | 顶层目录               |
| --------- | -------- | ------- | --------------- | ------------------ |
|           | ... ...  |         |                 | 编译之前已有的目录和文件       |
|           | _build / |         |                 | 编译生成的所有内容          |
|           |          | local / |                 | 直接复制 top_dir/local |
|           |          | var /   |                 | 直接复制 top_dir/var   |
|           |          | src /   |                 |                    |
|           |          |         | hello.o         | 编译过程中的obj文件        |
|           |          |         | hello.o.Po      | 编译过程中的dep依赖文件      |
|           |          |         | libhello.o      | 编译过程中的obj文件        |
|           |          |         | libhello.o.Po   | 编译过程中的dep依赖文件      |
|           |          | bin /   |                 |                    |
|           |          |         | hello           | 生成的bin文件           |
|           |          | lib /   |                 |                    |
|           |          |         | libhello.so.1.0 | 生成的lib文件           |

"_build" 目录名也可以自己指定，通过设置Make变量 STATIC_BUILD_PATH。

在 top_dir 目录下运行`make clean`命令，会自动删除生成的obj，dep，bin，lib文件。

在 "_build" 目录中编译完成后，bin文件应该可以直接运行，所以 "_build" 目录应该是一个完整的运行环境，所以编译过程中会自动把 top_dir 目录中的 local 和 var 复制到 "_build" 目录中。

下表列出了用户**必须在包含 ezBuild/init.mk 之前**可以设置的Make变量：

| 变量                | 说明                                           |
| ----------------- | -------------------------------------------- |
| EZ_USR_PATH       | usr目录的绝对路径，一般在env.sh中定义，必须定义                 |
| EZ_PKG_PATH       | pkg目录的绝对路径，一般在env.sh中定义，不安装不用定义              |
| STATIC_TOP_PATH   | top目录，必须定义，相对于该Makfile的相对路径或绝对路径             |
| STATIC_BUILD_PATH | 编译生成的目录名，相对于该Makfile的相对路径或绝对路径，不定义默认为 _build |

下表列出了用户**在包含 ezBuild/init.mk 之后和之前**可以设置的Make变量：

| 变量                  | 说明                                     |
| ------------------- | -------------------------------------- |
| GLOBAL_CROSS        | 编译器工具链前缀                               |
| GLOBAL_CFLAGS       | 全局编译参数                                 |
| GLOBAL_CXXFLAGS     | 全局C++编译参数                              |
| GLOBAL_LDFLAGS      | 全局链接参数                                 |
| GLOBAL_DEBUG        | 全局是否添加调试信息：1/0                         |
| GLOBAL_EXE_PIE      | 全局是否生成位置无关的可执行程序：1/0                   |
|                     | (以上GLOBAL_选项一般都定义在一个全局配置文件中)           |
| STATIC_CFLAGS       | 该Makefile中有效的编译参数                      |
| STATIC_LDFLAGS      | 该Makefile中有效的链接参数                      |
| STATIC_DEBUG        | 该Makefile中是否添加调试信息：1/0                 |
|                     |                                        |
| LOCAL_NAME          | 生成的程序名（不能含有 . ）<br>如果是静态库或动态库，必须以lib开头 |
| LOCAL_NAME_SUFFIX   | 名字后缀（必须以 . 开头）（参与soname指定）             |
| LOCAL_CFLAGS        | 编译参数                                   |
| LOCAL_LDFLAGS       | 链接参数                                   |
| LOCAL_SRC_FILES     | 源文件，C文件是 .c 后缀，C++文件是 .cc 或 .cpp 后缀    |
| LOCAL_DEBUG         | 是否添加调试信息：1/0                           |
| LOCAL_LIB_VERSION   | 动态库版本号（不参与soname指定）                    |
| LOCAL_LIB_UNDEFINED | 动态库中是否允许有未定义符号：1/0                     |
| LOCAL_EXTRA_OBJS    | 额外依赖的其他obj文件，需要自己编写编译规则                |
| LOCAL_EXTRA_DEPS    | 额外依赖的target                            |

补充说明：LOCAL_LIB_UNDEFINED 没定义或定义为0时，生成的动态库不允许有未定义符号，这是默认行为。但是gcc的默认行为是允许动态库有未定义符号，这是不安全的。为了照顾那些设计不好的动态库，可以定义LOCAL_LIB_UNDEFINED为1。

下表列出了用户**在包含 ezBuild/init.mk 之后**可以获取(只读)的Make变量：

| 变量                | 说明                                                                                  |
| ----------------- | ----------------------------------------------------------------------------------- |
| STATIC_TOP_PATH   | top目录的绝对路径                                                                          |
| STATIC_BUILD_PATH | 编译生成目录的绝对路径                                                                         |
| STATIC_SRC_DIR    | 当前Makefile所在的绝对路径, <br>必须在TOP目录下或等于TOP目录                                            |
| STATIC_DST_DIR    | STATIC_SRC_DIR目录对应在_build目录下的绝对路径<br>比如 top_dir 对应 _build，top_dir/src 对应 _build/src |
| EZ_PKG_NAME       | 安装到pkg中的目录名,<br>来自于config.mk中MY_PKG_NAME和MY_PKG_SUFFIX                              |

`make distclean`默认是删除 STATIC_BUILD_PATH 目录，用户可以添加 distclean 规则补充自己的 distclean 动作。

## 2. 软件包管理

### a. 安装软件包

如果需要把编译的程序安装到pkg目录，额外需要一个config.mk文件，文件内容如下：

```makefile
MY_PKG_NAME := hello
MY_PKG_SUFFIX :=
EZ_PKG_DEP_LIBS += -lhello
```

| 变量              | 说明                         |
| --------------- | -------------------------- |
| MY_PKG_NAME     | 安装到pkg中的软件包名(不能含有 . )，必须定义 |
| MY_PKG_SUFFIX   | 安装到pkg中的软件包名后缀(必须以 . 开始)   |
| EZ_PKG_DEP_LIBS | 依赖该软件包的程序在编译时需要链接的动态库      |

该文件在编译时必须位于 STATIC_BUILD_PATH 目录下，在 `include ezBuild/init.mk`中会隐含包含自己的 `$(STATIC_BUILD_PATH)$/config.mk` (如果存在)。

在Makefile中补充 config 规则（见上面Makefile内容），在编译前先执行 `make config`把 config.mk 拷贝到 STATIC_BUILD_PATH 中。如果 config.mk 发生改变，那么需要重新编译整个项目，所以config 也会先执行 distclean 规则动作，删除 _build 目录。

然后在Makefile中补充 install 规则（见上面Makefile内容），运行` make install `，首先是触发内建的 std_install_pkg 规则，把 top_dir 和 _build 中的特定目录拷贝到`$(EZ_USR_PATH)/$(EZ_PKG_NAME)/`，其中 `EZ_PKG_NAME := $(MY_PKG_NAME)$(MY_PKG_SUFFIX)`，注意 EZ_PKG_NAME 不能自己定义。

拷贝目录如下：

| EZ_TOP_PATH | EZ_BUILD_PATH | 拷贝  | EZ_PKG_PATH / EZ_PKG_NAME |
| ----------- | ------------- | --- | ------------------------- |
| include /   | include /     | ==> | include /                 |
|             | lib /         | ==> | lib /                     |
|             | bin /         | ==> | bin /                     |
|             | local /       | ==> | local /                   |
| doc /       | doc /         | ==> | doc /                     |
| var /       |               | ==> | var /                     |
|             | config.mk     | ==> | config.mk                 |

当然你也可以在install规则中增加拷贝自己的文件。

install 规则的最后执行`$(USR_INSTALL) $(EZ_USR_PATH) $(EZ_PKG_PATH)/(EZ_PKG_NAME)`把pkg中的bin、lib、include文件通过符号链接的方式汇集到 usr/bin、usr/lib、usr/include 目录中，这样其他程序就可以直接使用了。

注意：相同 MY_PKG_NAME 的软件包虽然可以同时安装到 一个 pkg 目录下（具有不同的软件包名后缀 MY_PKG_SUFFIX ），但是不能同时安装到 usr 目录中！并且bin、lib、include目录中的文件名也不能和其他软件包中的冲突！

在 usr/install 目录中可以查看所有已经安装的软件包名（MY_PKG_NAME，符号链接）。

### d. 移除软件包

最后在Makefile中补充 remove 的规则（见上面Makefile内容）运行`make remove`，把安装到pkg和usr中的内容删除。

## 3. 软件包配置

config.mk 中的内容就是该软件包的配置内容。因为该文件必须预先位于 STATIC_BUILD_PATH 目录下，所以源代码编译的第一步为 `make config`，然后再 `make` 或者 `make install`。

config.mk 中的 EZ_PKG_DEP_LIBS 变量非常有用，它定义了依赖该软件包的其他程序需要的链接选项，注意是通过 += 的方式定义的。

查看ezCore的config.mk，它里面定义了 `EZ_PKG_DEP_LIBS += -lezCore`，表示依赖ezCore的其他程序在编译时需要链接 libezCore.so。

如果上面的hello程序依赖ezCore或其他软件包，就可以在包含 ezBuild/init.mk之后，再依次包含它所**直接**依赖的所有软件包的 config.mk，就可以自动完成链接选项：

```makefile
#==============================================
include $(EZ_USR_PATH)/install/ezBuild/init.mk
... ...
include $(EZ_USR_PATH)/install/ezCore/config.mk
#==============================================
```

ezCore应该算是一个系统底层的平台库，它的config.mk在最后包含。

ezCore的config.mk中也定义了 GLOBAL_CROSS、GLOBAL_CFLAGS 等全局硬件平台相关的编译器选项。

用户还可以在config.mk中定义其它的编译配置选项，并且可以把某些选项转换成C语言头文件，具体用法参见 pkg/ezCore/config.mk 。
