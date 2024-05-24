#include <ezCore/ezCore.h>

int main(int argc, char *argv[])
{
	ezString *path = ez_get_app_path("example.ezml");

	ezDict *dict;
	dict = ezML_load_file(NULL, ezStr(path));
	ezDict_dump(dict);

	ezDict * d2 = ezDict_dup(dict);
	ezML_store_file(d2, "/tmp/test.ezml");
	dict = ezML_load_file(NULL, "/tmp/test.ezml");
	ezDict_dump(dict);

	ezGC_exit(0); /*自动释放所有local和global资源内存, 并exit()*/
	return 0;
}

