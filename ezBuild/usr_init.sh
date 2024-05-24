#!/bin/sh

usr_dir="$1"
pkg_dir="$2"
force_var=""

if [ "$3" = "-force_var" ]; then
  force_var="1"
fi

#==============================================
my_error_exit () {
	echo "$*"
	exit 255
}
#==============================================

abs=$(expr index "$usr_dir" "/")
if [ $abs -ne 1 ] ; then
	my_error_exit "$usr_dir is not abs path"
fi

if [ ! -d "$usr_dir" ]; then
  my_error_exit "$usr_dir is not exist"
fi

abs=$(expr index "$pkg_dir" "/")
if [ $abs -ne 1 ] ; then
	my_error_exit "$pkg_dir is not abs path"
fi

if [ ! -d "$pkg_dir/ezBuild" ]; then
  my_error_exit "$pkg_dir/ezBuild is not exist"
fi

#----------------------------------------------------

mkdir -p $usr_dir/bin ; rm -rf $usr_dir/bin/*
mkdir -p $usr_dir/lib ; rm -rf $usr_dir/lib/*
mkdir -p $usr_dir/include ; rm -rf $usr_dir/include/*
mkdir -p $usr_dir/install ; rm -rf $usr_dir/install/*

#处理var
mkdir -p $usr_dir/var
if [ -n "$force_var" ]; then
  rm -rf "$usr_dir/var/*"
fi

#设置evn.sh
cat > $usr_dir/env.sh <<EOF
export EZ_USR_PATH=$usr_dir
export EZ_PKG_PATH=$pkg_dir
echo "usr=\$EZ_USR_PATH, pkg=\$EZ_PKG_PATH"

export PATH=\$EZ_USR_PATH/bin:\$PATH
export LD_LIBRARY_PATH=\$EZ_USR_PATH/lib
export LC_ALL=zh_CN.UTF-8

TOP=\`dirname \$EZ_USR_PATH\`
TOP=\`basename \$TOP\`
export PS1='\u@\h:\$TOP:\w\$ '
EOF

chmod 755 $usr_dir/env.sh

#初始安装ezBuild
ln -sf $pkg_dir/ezBuild $usr_dir/install/ezBuild
