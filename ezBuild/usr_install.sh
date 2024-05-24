#!/bin/sh

usr_dir="$1"
pkg_dir="$2"
force_var=""
pkg_name=""

if [ "$3" = "-force_var" ]; then
  force_var="1"
fi

#==============================================
rm_error_exit=""
my_error_exit () {
  echo "$*"
  if [ -n "$rm_error_exit" ]; then
    rm -rf "$usr_dir/install/$pkg_name"
    find "$usr_dir" -xtype l -delete
  fi
  exit 255
}

make_dir ()
{
  local _dir
  _dir="$1"

  if [ ! -d "$_dir" ]; then
	mkdir -p "$_dir"
  fi

  if [ ! -d "$_dir" ]; then
	my_error_exit "mkdir $_dir error"
  fi
}

make_link ()
{
  local _dst
  local _src
  _dst="$1"
  _src="$2"

  if [ -e "$_dst" ]; then
    if [ ! -h "$_dst" ]; then
      my_error_exit "make link to $_src, but $_dst error"
    fi
  else
    ln -sf "$_src" "$_dst"
  fi

  _dst=$(readlink "$_dst")
  if [ "$_dst" != "$_src" ]; then
    my_error_exit "make link to $_src, but error to $_dst"
  fi
}

install_link_dir ()
{
  local _dst
  local _src
  local _bn_
  _dst="$1"
  _src="$2"
for _src_f in $_src/*
do

_bn_=$(basename "$_src_f")
if [ -d "$_src_f" ]; then
  make_dir "$_dst/$_bn_"
  install_link_dir "$_dst/$_bn_" "$_src_f"
else
  make_link "$_dst/$_bn_" "$_src_f"
fi

done
}

#==============================================

abs=$(expr index "$usr_dir" "/")
if [ $abs -ne 1 ] ; then
	my_error_exit "$usr_dir is not abs path"
fi

if [ ! -d "$usr_dir/install/ezBuild" ]; then
  my_error_exit "$usr_dir is not init"
fi

abs=$(expr index "$pkg_dir" "/")
if [ $abs -ne 1 ] ; then
	my_error_exit "$pkg_dir is not abs path"
fi

if [ ! -d "$pkg_dir" ]; then
  my_error_exit "$pkg_dir is not exist"
fi

pkg_name=$(basename $pkg_dir)
abs=$(expr index $pkg_name .)
if [ $abs -gt 1 ] ; then
  pkg_name=`expr substr $pkg_name 1 $(expr $abs - 1)`
fi

make_link "$usr_dir/install/$pkg_name" "$pkg_dir"

#下面出错时, 反向删除
rm_error_exit=1

#所有的符号链接都是指向install/pkg_name, 方便删除
pkg_dir="$usr_dir/install/$pkg_name"

if [ -d "$pkg_dir/lib" ]; then
  make_dir "$usr_dir/lib"
  install_link_dir "$usr_dir/lib" "$pkg_dir/lib"
fi

if [ -d "$pkg_dir/bin" ]; then
  make_dir "$usr_dir/bin"
  install_link_dir "$usr_dir/bin" "$pkg_dir/bin"
fi

if [ -d "$pkg_dir/include" ]; then
  make_dir "$usr_dir/include"
  install_link_dir "$usr_dir/include" "$pkg_dir/include"
fi

#最后处理var, 保留之前的var
if [ -d "$pkg_dir/var" ]; then
  if [ -n "$force_var" ]; then
    rm -rf "$usr_dir/var/pkg_name"
  fi
  if [ ! -d "$usr_dir/var/pkg_name" ]; then
    cp -a "$pkg_dir/var" "$usr_dir/var/pkg_name"
  fi
else
  rm -rf "$usr_dir/var/pkg_name"
fi
