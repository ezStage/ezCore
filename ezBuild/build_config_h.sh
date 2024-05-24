#!/bin/sh

in_file="$1"
out_file="$2"
out_name=$(basename "$out_file")

my_error_exit () {
  echo "$*"
  exit 255
}

my_echo () {
  echo "$*" >> "$out_file"
}

if [ ! -f "$in_file" ] ; then
  my_error_exit "$in_file is not exist or error"
fi

mkdir -p $(dirname "$out_file")
echo "/* this file is auto built according to $(basename $1) */" > "$out_file"

if [ ! -f "$out_file" ] ; then
  my_error_exit "$out_file is not exist or error"
fi

my_echo "#ifndef _${out_name%.*}_H_"
my_echo "#define _${out_name%.*}_H_"
my_echo ""

#=============================================
while read line
do

#只匹配 $(eval ...) 或 #$(eval ...) 的行
value=`expr match "$line" '#\?$(eval \(.*\))'`
if [ -z "$value" ] ; then
continue
fi

#获取是否是注释
note=`expr index "$line" "#"`
#去掉前后的空格
value=`echo -n $value`
key=""

#----------------------------------------
#提取 key:= 或 key=
n=`expr match "$value" ".*:="`
if [ $n -gt 2 ] ; then
key=`echo "$value" | cut -b 1-$(expr $n - 2)`
value=`echo "$value" | cut -b $(expr $n + 1)-`
else
n=`expr match "$value" ".*="`
if [ $n -gt 1 ] ; then
key=`echo "$value" | cut -b 1-$(expr $n - 1)`
value=`echo "$value" | cut -b $(expr $n + 1)-`
fi
fi
#----------------------------------------

#去掉前后的空格
key=`echo -n $key`
value=`echo -n $value`

if [ $note -eq 0 ] ; then
  my_echo "#define $key $value"
else
  my_echo "/* #undef $key */"
fi

done < "$in_file"
#=============================================

my_echo ""
my_echo "#endif"
my_echo ""

