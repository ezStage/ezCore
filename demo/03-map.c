#include <ezCore/ezCore.h>

/*假定每个学生不重名, name作为key*/

struct student {
	const char *name;
	int age;
};

ezMap * stu_map;

struct student * stu_add(char *name, int age)
{
	struct student *ret;
	ret = (struct student *)ezMap_add_node_string(stu_map, name);
	if(ret == NULL) return NULL; //创建map node失败(内存不够或name为NULL)
	if(ret->name != NULL) {
		ezLog_err("already has same name %s\n", name);
		return NULL;
	}
	ret->name = ezMapNode_get_key_string(ret, NULL);
	ret->age = age;
	return ret;
}

struct student * stu_find(char *name)
{
	struct student *stu;
	stu = ezMap_find_node_string(stu_map, name);
	return stu;
}

void stu_delete(struct student *stu)
{
	ezMapNode_delete(stu);
}


ezBool _stu_visit_func(void *data, void *arg)
{
	struct student *stu = (struct student *)data;
	printf("  name=\"%s\", age=%d\n", stu->name, stu->age);
	return FALSE;
}

struct student * stu_print_all(void)
{
	printf("all student:\n");
	ezMap_traversal(stu_map, _stu_visit_func, NULL);
	printf("\n");
	return NULL;
}

void stu_delete_all(void)
{
	ezMap_delete_all(stu_map);
}

int main(int argc, char *argv[])
{
	struct student *stu;

	stu_map = ezMap_create(sizeof(struct student), NULL, FALSE);

	stu_add("zhan1", 10);
	stu_add("wang2", 11);
	stu_add("wang3", 12);
	stu_add("wang2", 13);
	stu_print_all();

	stu = stu_find("wang2");
	stu_delete(stu);
	stu_print_all();

	stu_delete_all();
	stu_print_all();

	ezGC_exit(0); /*自动释放所有local和global资源内存, 并exit()*/
	return 0;
}

