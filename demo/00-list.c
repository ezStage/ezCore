#include <ezCore/ezCore.h>

struct student {
	char *name;
	int age;
	ezList list;
};

ezList stu_head;

struct student * stu_add(char *name, int age)
{
	struct student *ret;
	ret = (struct student *)malloc(sizeof(struct student));
	ret->name = strdup(name);
	ret->age = age;
	ezList_init(&(ret->list));
	ezList_add_tail(&(ret->list), &stu_head);
	return ret;
}

struct student * stu_find(char *name)
{
	struct student *stu;
	ezList *plist;
	ezList_for_each(plist, &stu_head)
	{
		stu = EZ_FROM_MEMBER(struct student, list, plist);
		if(strcmp(stu->name, name)) return stu;
	}
	return NULL;
}

void stu_delete(struct student *stu)
{
	ezList_del(&(stu->list));
	free(stu->name);
	free(stu);
}

struct student * stu_print_all(void)
{
	struct student *stu;
	ezList *plist;

	printf("all student:\n");
	ezList_for_each(plist, &stu_head)
	{
		stu = EZ_FROM_MEMBER(struct student, list, plist);
		printf("  name=\"%s\", age=%d\n", stu->name, stu->age);
	}
	printf("\n");
	return NULL;
}

void stu_delete_all(void)
{
	struct student *stu;
	ezList *plist, *tmp;
	ezList_for_each_safe(plist, tmp, &stu_head)
	{
		stu = EZ_FROM_MEMBER(struct student, list, plist);
		ezList_del(plist);
		free(stu->name);
		free(stu);
	}
}

int main(int argc, char *argv[])
{
	struct student *stu;

	ezList_init_head(&stu_head);

	stu_add("zhan1", 10);
	stu_add("wang2", 10);
	stu_add("wang3", 11);
	stu_print_all();

	stu = stu_find("wang2");
	stu_delete(stu);
	stu_print_all();

	stu_delete_all();
	stu_print_all();
	return 0;
}

