#include <ezCore/ezCore.h>

#define EXE_INFO	"ezCore arg usage demo"

#define ARG_ID_help		1
#define ARG_ID_opt_a	2
#define ARG_ID_opt_b	3
#define ARG_ID_opt_c 	4

static ezARG_option opts[] = {
	{"help", ARG_ID_help, "show help and exit", FALSE},
	{"a", ARG_ID_opt_a, "ARG_ID_opt_a expain", FALSE},
	{"bv", ARG_ID_opt_b, "ARG_ID_opt_bv expain", TRUE}, //-bv后面必须有值
	{"cv", ARG_ID_opt_c, "ARG_ID_opt_c expain", TRUE, TRUE},
	//-cv后面必须有值, -cv和值之间可以没有空格
	{NULL}, //必须以NULL结束
};

/*
./04-arg -help
./04-arg --help
./04-arg -a -bv 123 -bv=456 -cv 789 -cv=abc -cvdef -other value1 value2
*/

int main(int argc, char *argv[])
{
	//创建一个字符串列表, 记录非选项内容
	ezVector *inputs;
	inputs = ezStringVector_create();

	ezARG ma;
	ezARG_init(&ma, 1, argc, argv, opts);
	while(1)
	{
		switch(ezARG_parse(&ma))
		{
		case EZ_ARG_END:
			//所有命令行参数结束
			break;
		case EZ_ARG_NO_MATCH:
			//以 '-' 或 ''--'' 开始, 但没有匹配到选项
			ezLog_print("arg \"%s\" is not option\n", ma.value);
			return -1;
		case EZ_ARG_NO_VALUE:
			//以 '-' 或 ''--'' 开始, 匹配到选项, 但没有值
			ezLog_print("arg \"%s\" has no value\n", ma.value);
			return -1;
		case EZ_ARG_NO_OPT:
			//不以 '-' 开始, 不是选项
			ezVector_add(inputs, ezString_create(ma.value, -1));
			continue;
		case ARG_ID_help:
			ezARG_help(argv[0], EXE_INFO, opts);
			return 0;
		case ARG_ID_opt_a:
			printf("option -a, no value\n");
			continue;
		case ARG_ID_opt_b:
			printf("option -bv, value=\"%s\"\n", ma.value);
			continue;
		case ARG_ID_opt_c:
			printf("option -cv, value=\"%s\"\n", ma.value);
			continue;
		}
		break;
	}

	printf("no option arg:\n");

	ezString *s;
	int i, n=ezVector_get_count(inputs);
	for(i=0; i<n; ++i) {
		s = ezVector_get(inputs, i);
		printf("  \"%s\"\n", ezStr(s));
	}

	ezGC_exit(0);
	return 0;
}

