#include <ezCore/ezCore.h>

/*返回p1和p2前面相同的非'\0'的字节个数*/
static int _str_cmp(const char *p1, const char *p2)
{
	if((p1 == NULL)||(p2 == NULL)) return 0;
	char c;
	const char *p = p1;
	while((c = *p++)) {
		if(c != *p2++) break;
	}
	return (p-p1-1);
}

int16_t ezARG_parse(ezARG *arg)
{
	const ezARG_option *opts = arg->opts;
	const char *p;
	int i, n;

	arg->value = NULL;

	i = arg->next;
	if(i >= arg->argc) return EZ_ARG_END;

	p = arg->argv[i++];
	if((p == NULL)||(*p == '\0')) return EZ_ARG_END;

	if(*p != '-') {
		arg->value = p;
		arg->next = i;
		return EZ_ARG_NO_OPT;
	}

	if(*++p == '-') ++p;

	int16_t ret = EZ_ARG_NO_MATCH;

	for(; opts->key; ++opts)
	{
		n = _str_cmp(p, opts->key);
		/*key必须完全匹配, 否则下一个*/
		if(opts->key[n] != '\0') continue;

		if(!opts->has_value)
		{
			/*p必须完全匹配*/
			if(p[n] == '\0') {
				arg->next = i;
				return opts->id;
			}
			continue;
		}

		/*下面就是has_value的情况*/

		if(p[n] == '\0')
		{/*p完全匹配时*/
			p = arg->argv[i++]; /*取下一个argv*/
			if((i == arg->argc)||(p == NULL)||
				(*p == '-')||(*p == '\0'))
			{/*下一个argv出错, i 保持是下一个argv*/
				--i;
				ret = EZ_ARG_NO_VALUE;
				break;/*出错, 没有value*/
			}
			arg->value = p;
			arg->next = i;
			return opts->id;
		}
		if(p[n] == '=') /*这时也算p是完全匹配*/
		{
			if(p[++n] == '\0') {
				ret = EZ_ARG_NO_VALUE;
				break;/*出错, 没有value*/
			}
			arg->value = p+n;
			arg->next = i;
			return opts->id;
		}
		/*p不完全匹配*/
		if(opts->compact) {
			arg->value = p+n;
			arg->next = i;
			return opts->id;
		}
	}
	arg->value = arg->argv[arg->next]; /*没有匹配的内容*/
	arg->next = i; /*next仍然取下一个*/
	return ret;
}

void ezARG_help(const char *exe_name, const char *describe, const ezARG_option *opts)
{
	ezLog_print("%s: %s\n", exe_name, describe);
	ezLog_print("usage: %s [options] ...\n", exe_name);
	ezLog_print("options:\n");
	for(; opts->key; ++opts)
	{
		if(opts->has_value) {
			ezLog_print("\n");
			if(opts->compact)
				ezLog_print("  -%s<value>\n", opts->key);
			ezLog_print("  -%s=<value>\n", opts->key);
			ezLog_print("  -%s <value>\n%-30s %s\n", opts->key, "", opts->help);
		} else {
			ezLog_print("  -%-30s %s\n", opts->key, opts->help);
		}
	}
	ezLog_print("\n");
	return;
}

