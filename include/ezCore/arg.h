#ifndef	_EZ_ARG_H
#define	_EZ_ARG_H

/*特殊option id*/
#define EZ_ARG_NO_OPT		0	/*不以 '-' 开始, 不是选项*/
#define EZ_ARG_NO_MATCH		-1	/*以 '-' 开始, 但没有匹配到选项*/
#define EZ_ARG_NO_VALUE		-2	/*以 '-' 开始, 但没有匹配到值*/
#define EZ_ARG_END			-3	/*结束*/

typedef struct {
	const char *key; /*选项名字, 最后一个为NULL*/
	int16_t id; /*用户自己确定的一个整数值, 不能等于上面的特殊id*/
	const char *help;
	ezBool has_value; /*该选项是否必须有值(值都是字符串)*/
	ezBool compact; /*该选项的值和key之间是否可以没有空隔符*/
} ezARG_option;

typedef struct {
	int next; /*下一个arg的序号: [0, argc]*/
	const char *value;
	int argc; /*保存argc*/
	char **argv; /*保存argv*/
	const ezARG_option *opts; /*保存ezARG_option*/
} ezARG;

/*
可以识别以下形式的选项(前缀为 - 或 --):
-Key
-Key=Value
-Key Value
-KeyValue (compact为true)

识别算法如下:
i=arg.next, 解析argv[i]:
1 如果 i >= arg.argc, 返回 EZ_ARG_END, arg.value=NULL
1 如果argv[i]不以 - 开始, 返回 EZ_ARG_NO_OPT, arg.value=argv[i].
2 跳过前缀 - 或 --, 从前往后依次匹配 ezARG_option:
	1 key必须完全匹配, 否则continue.
	2 如果 !has_value, argv[i]必须完全匹配key, 返回id, arg.value=NULL; 否则continue.
	//下面就是has_value的情况
	3 如果argv[i]完全匹配, argv[i+1]就是value, 返回id, arg.value=argv[i+1];
	如果argv[i+1]不合法, 返回 EZ_ARG_NO_MATCH, arg.value=argv[i].
	//下面就是argv[i]不完全匹配的情况:
	4 如果argv[i]未匹配的第一个字符是'=', 即key=XXX, XXX就是arg.value, 返回id.
	5 如果compact为true, argv[i]未匹配的部分就是arg.value, 返回id.
	6 continue.
3 返回EZ_ARG_NO_MATCH, arg.value=argv[i].
*/

static inline void ezARG_init(ezARG *arg, int index, int argc, char *argv[], const ezARG_option *opts)
{
	arg->next = index;
	arg->argc = argc;
	arg->argv = argv;
	arg->opts = opts;
	for(; opts->key; ++opts)
	{
		if((opts->id == EZ_ARG_NO_OPT) ||
			(opts->id == EZ_ARG_END) ||
			(opts->id == EZ_ARG_NO_MATCH))
		{
			ezLog_err("arg option: key=\"%s\", id=%d, error\n", opts->key, opts->id);
		}
	}
}

/*返回option id*/
extern int16_t ezARG_parse(ezARG *arg);

extern void ezARG_help(const char *exe_name, const char *describe, const ezARG_option *opts);

#endif

