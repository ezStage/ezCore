#include <ezCore/ezCore.h>

struct ezString_st {
	int32_t len; /*>=0*/
	char str[0]; /*[len+1]*/
};

static ezGC_type EZ_STRING_TYPE={
	.name = "ezString",
};
ezString * ezString_create(const char *str, int32_t len)
{
	if(str == NULL) return NULL;
	if(len == -1) len = strlen(str);
	if(len < 0) return NULL;

	ezString *p;
	p = (ezString *)ezAlloc(len+sizeof(ezString)+1, &EZ_STRING_TYPE, FALSE);
	if(p == NULL) return NULL;
	p->len = len;
	if(len > 0) memcpy(p->str, str, len);
	p->str[len] = '\0';
	return p;
}
ezString * ezString_create_trim(const char *str, int32_t len)
{
	if(str == NULL) return NULL;
	if(len == -1) len = strlen(str);
	if(len < 0) return NULL;

	char c;
	while(len > 0) {
		c = *str;
		if(!EZ_CHAR_IS_BLANK(c)) break;
		++str;
		--len;
	};

	while(len > 0) {
		c = str[len-1];
		if(!EZ_CHAR_IS_BLANK(c)) break;
		--len;
	}

	ezString *p;
	p = (ezString *)ezAlloc(sizeof(ezString)+1+len, &EZ_STRING_TYPE, FALSE);
	if(p == NULL) return NULL;
	p->len = len;
	if(len > 0) memcpy(p->str, str, len);
	p->str[len] = '\0';
	return p;
}

ezString * ezString_printf(const char *fmt, ...)
{
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if(len < 0) return NULL;

	ezString *p;
	p = (ezString *)ezAlloc(sizeof(ezString)+1+len, &EZ_STRING_TYPE, FALSE);
	if(p == NULL) return NULL;
	p->len = len;

	va_start(ap, fmt);
	vsnprintf(p->str, len+1, fmt, ap);
	va_end(ap);
	return p;
}

ezString * ezString_vprintf(const char *fmt, va_list ap)
{
	int len;
	va_list ap2;

	va_copy(ap2, ap);
	len = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);

	if(len < 0) return NULL;

	ezString *p;
	p = (ezString *)ezAlloc(sizeof(ezString)+1+len, &EZ_STRING_TYPE, FALSE);
	if(p == NULL) return NULL;
	p->len = len;

	vsnprintf(p->str, len+1, fmt, ap);
	return p;
}

/*=================================================*/

int32_t ezString_index(ezString *es, int32_t start, char c)
{
	if((es == NULL)||(start < 0)||(start >= es->len))
		return EZ_STR_NOT_FOUND;
	char *s, *e=es->str+es->len;
	for(s=es->str+start; s<e; ++s) {
		if(*s == c) return (s - es->str);
	}
	return EZ_STR_NOT_FOUND;
}

int32_t ezString_rindex(ezString *es, int32_t end, char c)
{
	if((es == NULL)||(end <= 0)||(end > es->len))
		return EZ_STR_NOT_FOUND;

	char *s;
	for(s=es->str+end-1; s>=es->str; --s) {
		if(*s == c) return (s - es->str);
	}
	return EZ_STR_NOT_FOUND;
}

ezBool ezString_beginwith(ezString *es, const char *str)
{
	if((es == NULL)||(str == NULL)) return FALSE;
	int n = strncmp(es->str, str, strlen(str)); /*strncmp("XXXX", "\0", 0) 返回0*/
	return (n == 0);
}

ezBool ezString_endwith(ezString *es, const char *str)
{
	if((es == NULL)||(str == NULL)) return FALSE;
	int n = strlen(str);
	if(es->len < n) return FALSE;
	n = strcmp(es->str+es->len-n, str); /*strcmp("\0", "\0") 返回0*/
	return (n == 0);
}

/*获取子串es[start, end) */
ezString * ezString_mid(ezString *es, int32_t start, int32_t end)
{
	/*start和end超出了合法的范围, 即使可以相等也返回NULL*/
	if((es == NULL)||(start > end)||
		(start < 0)||(start >= es->len)||
		(end <= 0)||(end > es->len))
		return NULL;
	return ezString_create(es->str+start, end-start);
}

ezString * ezString_insert(ezString *es, int32_t index, const char *str)
{
	if((es == NULL) ||
		(index < 0) ||
		(index > es->len))
		return NULL;

	int slen = str? strlen(str) : 0;
	if(slen == 0) return ezString_create(es->str, es->len); //dup

	ezString *p;
	p = (ezString *)ezAlloc(es->len+slen+sizeof(ezString)+1, &EZ_STRING_TYPE, FALSE);
	if(p == NULL) return NULL;

	p->len = es->len+slen;

	if(index > 0) memcpy(p->str, es->str, index);
	memcpy(p->str+index, str, slen);

	if(index == es->len)
		p->str[p->len] = '\0';
	else
		memcpy(p->str+index+slen, es->str+index, es->len-index+1); //包括结尾的'\0'
	return p;
}

ezString * ezString_delete(ezString *es, int32_t index, int32_t len)
{
	if((es == NULL) ||
		(index < 0) ||
		(len < 0) ||
		(index+len > es->len))
		return NULL;

	if(len == 0) return ezString_create(es->str, es->len); //dup

	ezString *p;
	p = (ezString *)ezAlloc(es->len-len+sizeof(ezString)+1, &EZ_STRING_TYPE, FALSE);
	if(p == NULL) return NULL;

	p->len = es->len-len;

	if(index > 0) memcpy(p->str, es->str, index);

	len += index;
	if(len == es->len)
		p->str[index] = '\0';
	else
		memcpy(p->str+index, es->str+len, es->len-len+1); //包括结尾的'\0'
	return p;
}

ezString * ezString_replace(ezString *es, int32_t index, int32_t len, const char *str)
{
	if((es == NULL) ||
		(index < 0) ||
		(len < 0) ||
		(index+len > es->len))
		return NULL;

	if(len == 0) return ezString_insert(es, index, str);

	int slen = str? strlen(str) : 0;
	if(slen == 0) return ezString_delete(es, index, len);

	ezString *p;
	p = (ezString *)ezAlloc(es->len-len+slen+sizeof(ezString)+1, &EZ_STRING_TYPE, FALSE);
	if(p == NULL) return NULL;

	p->len = es->len-len+slen;

	if(index > 0) memcpy(p->str, es->str, index);
	memcpy(p->str+index, str, slen);

	len += index;
	if(len == es->len)
		p->str[p->len] = '\0';
	else
		memcpy(p->str+index+slen, es->str+len, es->len-len+1); //包括结尾的'\0'
	return p;
}

/*====================================================================*/

ezVector * ezStringVector_create(void)
{
	return ezVector_create(&EZ_STRING_TYPE);
}

void ezStringVector_dump(ezVector *rv)
{
	if(!ezVector_check_type(rv, &EZ_STRING_TYPE)) {
		ezLog_err("%p is not ezStringVector\n", rv);
		return;
	}
	ezString *s;
	int i, n = ezVector_get_count(rv);
	ezLog_print("ezStringVector <%d>\n", n);
	for(i=0; i<n; ++i) {
		s = ezVector_get(rv, i);
		ezLog_print("\t\"%s\"[%d]\n", ezStr(s), ezStrLen(s));
	}
	return;
}

ezVector * ezString_separate(const char *str, int32_t len, const char sep)
{
	if(str == NULL) return NULL;
	if(len == -1) len = strlen(str);
	if(len <= 0) return NULL;

	ezString *rs;
	ezVector *rv;
	rv = ezStringVector_create();

	const char *s;
	const char *p=str;
	const char *e=str+len;

	if(EZ_CHAR_IS_BLANK(sep))
	{
		s = NULL;
		do {
			if(!EZ_CHAR_IS_BLANK(*p)) {
				if(s == NULL) s = p;
				continue;
			}
			if(s != NULL) {
				rs = ezString_create_trim(s, p-s);
				ezVector_add(rv, rs);
				s = NULL;
			}
		} while(++p < e);

		if(s != NULL) {
			rs = ezString_create_trim(s, p-s);
			ezVector_add(rv, rs);
		}
		return rv;
	}

	s = str;
	do {
		if(*p == sep)
		{
			rs = ezString_create_trim(s, p-s);
			ezVector_add(rv, rs);
			s = p+1;
		}
	} while(++p < e);

	rs = ezString_create_trim(s, p-s);
	ezVector_add(rv, rs);
	return rv;
}

