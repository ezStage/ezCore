#!/bin/sh

pkg_dir=$(dirname "$0")
if [ ! -d "$pkg_dir/ezBuild" ]; then
	echo "$pkg_dir/ezBuild no exist"
	exit 255
fi

abs=$(expr index "$pkg_dir" "/")
if [ $abs -ne 1 ] ; then
	pkg_dir=$(cd "$pkg_dir" ; pwd)
fi

usr_dir="$1"
mkdir -p "$usr_dir"

abs=$(expr index "$usr_dir" "/")
if [ $abs -ne 1 ] ; then
	pkg_dir=$(cd "$usr_dir" ; pwd)
fi

init_sh="$pkg_dir/ezBuild/usr_init.sh"
install_sh="$pkg_dir/ezBuild/usr_install.sh"

#初始化usr
$init_sh "$usr_dir" "$pkg_dir" -force_var

#默认安装所有已经存在的pkg
for _src_f in $pkg_dir/*
do

if [ -d "$_src_f" ]; then

_name=$(basename "$_src_f")
if [ "$_name" != "ezBuild" ]; then
  $install_sh "$usr_dir" "$_src_f"
fi

fi

done
