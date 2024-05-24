#ifndef _EZ_STRING_H
#define _EZ_STRING_H

static inline const char *ezStr(ezString *es) {
	return es? (const char *)es+sizeof(int32_t) : NULL;
}

static inline int32_t ezStrLen(ezString *es) {
	return es? *((int32_t *)es) : 0;
}

/*那些字符是空白符*/
#define EZ_CHAR_IS_BLANK(c) (((c)==' ')||(((c)>='\t')&&((c)<='\r'))||((c)=='\0'))

/*创建一个ezString, 如果len等于-1, 取len=strlen(str)*/
ezString * ezString_create(const char *str, int32_t len);
ezString * ezString_create_trim(const char *str, int32_t len); /*去掉首尾的空白符*/

ezString * ezString_printf(const char *fmt, ...);
ezString * ezString_vprintf(const char *fmt, va_list ap);

/*上面函数的参数都是字符串(char *), 生成一个ezString.
ezString中保存的内容也都在最后添加了'\0'结束.
把用户经常对字符串做的第一步操作封装成上面的函数*/

/*ezString可以看做是只读的. 下面的函数如果返回一个ezString,
都是新创建一个, 而不是在输入ezString上修改*/

#define EZ_STR_NOT_FOUND	((int32_t)-1)

/*从start开始扫描(包括start位置的字符), 返回c所在es中的位置索引.
返回 EZ_STR_NOT_FOUND 表示没找到*/
int32_t ezString_index(ezString *es, int32_t start, char c);

/*从end开始反向扫描(不包括end位置的字符), 返回c所在es中的位置索引().
返回 EZ_STR_NOT_FOUND 表示没找到. end最大可以等于ezStrLen(es)*/
int32_t ezString_rindex(ezString *es, int32_t end, char c);

ezBool ezString_beginwith(ezString *es, const char *str);
ezBool ezString_endwith(ezString *es, const char *str);

/*获取子串es[start, end), end最大可以等于ezStrLen(es)*/
ezString * ezString_mid(ezString *es, int32_t start, int32_t end);
#define ezString_dup(es) ezString_mid(es, 0, ezStrLen(es))

/*在index位置插入str, index最大可以取到ezStrLen(es)*/
ezString * ezString_insert(ezString *es, int32_t index, const char *str);
#define ezString_append(es, str) ezString_insert(es, ezStrLen(es), str)
#define ezString_prepend(es, str) ezString_insert(es, 0, str)

/*删除es[index, index+len), index最大可以取到ezStrLen(es)*/
ezString * ezString_delete(ezString *es, int32_t index, int32_t len);

/*用str替换es[index, index+len), index最大可以取到ezStrLen(es)*/
ezString * ezString_replace(ezString *es, int32_t index, int32_t len, const char *str);

/*=====================================================*/

ezVector * ezStringVector_create(void);
void ezStringVector_dump(ezVector *rv);

/*sep是分割符号, 分割符前后的空白符自动去掉.
如果sep正好也是一种空白符, 那么就是按空白符分隔.*/
ezVector * ezString_separate(const char *str, int32_t len, const char sep);

#endif

