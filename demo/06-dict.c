#include <ezCore/ezCore.h>

int main(int argc, char *argv[])
{
	ezDict *dict = ezDict_create();
	
	ezDict *sub = ezDict_create();
	ezDict_add_nokey_dict(dict, sub); //同时设置dict是sub的owner
	ezDict_add_key_str(sub, "name", "zhan1", -1);
	ezDict_add_key_int(sub, "age", 10);

	sub = ezDict_create();
	ezDict_add_nokey_dict(dict, sub);
	ezDict_add_key_str(sub, "name", "wang2", -1);
	ezDict_add_key_int(sub, "age", 10);

	sub = ezDict_create();
	ezDict_add_nokey_dict(dict, sub);
	ezDict_add_key_str(sub, "name", "wang3", -1);
	ezDict_add_key_int(sub, "age", 13);

	ezDict_dump(dict);
	ezDestroy(dict); //同时释放所有sub dict

	ezGC_exit(0); /*自动释放所有local和global资源内存, 并exit()*/
	return 0;
}

