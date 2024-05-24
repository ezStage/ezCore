#!/bin/sh

usr_dir="$1"
pkg_dir="$2"
force_var=""
pkg_name=""

if [ "$3" = "-force_var" ]; then
  force_var="1"
fi

#==============================================

my_error_exit () {
  echo "$*"
  exit 255
}

#----------------------------------------------------

abs=$(expr index "$usr_dir" "/")
if [ $abs -ne 1 ]; then
	my_error_exit "$usr_dir is not abs path"
fi

if [ ! -d "$usr_dir/install/ezBuild" ]; then
  my_error_exit "$usr_dir is not init"
fi

#$pkg_dir可能已经被删除

abs=$(expr index "$pkg_dir" "/")
if [ $abs -ne 1 ]; then
  #不是绝对路径
  if [ $abs -ne 0 ]; then
    #只是pkg_name, 不能有/
    my_error_exit "$pkg_dir is not pkg_name"
  fi
  pkg_name="$pkg_dir"
  pkg_dir=$(readlink "$usr_dir/install/$pkg_name")
else
  #是绝对路径
  pkg_name=$(basename $pkg_dir)
  abs=$(expr index $pkg_name .)
  if [ $abs -gt 1 ]; then
    pkg_name=`expr substr $pkg_name 1 $(expr $abs - 1)`
  fi
fi

#install/pkg_name可能已经断开
if [ -h "$usr_dir/install/$pkg_name" ]; then
  if [ -e "$usr_dir/install/$pkg_name" ]; then
    _dst=$(readlink "$usr_dir/install/$pkg_name")
    if [ "$_dst" != "$pkg_dir" ]; then
      echo "$pkg_name is install different"
      exit 0
    fi
  fi
else
  if [ ! -e "$usr_dir/install/$pkg_name" ]; then
    echo "$pkg_name is not install"
    exit 0
  fi
fi

rm -rf "$usr_dir/install/$pkg_name"
find "$usr_dir" -xtype l -delete

#最后处理var
if [ -n "$force_var" ]; then
  rm -rf "$usr_dir/var/pkg_name"
fi
