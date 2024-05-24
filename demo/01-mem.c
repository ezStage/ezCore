#include <ezCore/ezCore.h>

struct mem_obj {
	const char *name;
	int attr1;
	int attr2;
};

static void _mem_obj_destroy(void *p) {
	struct mem_obj *obj = (struct mem_obj *)p;
	printf("destroy mem_obj %s\n", obj->name);
	ez_free(obj->name); //free()的封装
}

static ezGC_type mem_obj_type={
	.destroy_func = _mem_obj_destroy,
	.name = "mem_obj",
};

struct mem_obj * obj_add(char *name, int attr1, int attr2)
{
	struct mem_obj *po;
	po = ezAlloc(sizeof(struct mem_obj), &mem_obj_type, TRUE);
	po->name = ez_strdup(name); //strdup()的封装 
	po->attr1 = attr1;
	po->attr2 = attr2;
	return po;
}

/*********************************************************/

static struct mem_obj *g1, *g2;
static struct mem_obj *local_1, *local_2;

void test1(void)
{
	g2 = obj_add("obj_global_2", 1, 2); //创建时默认local资源
	ezGC_Local2Global(g2); //把gv2转换成global资源

	local_2 = obj_add("obj_local_2", 1, 2);
}

int main(int argc, char *argv[])
{
	g1 = obj_add("obj_global_1", 1, 2);//创建时默认local资源
	ezGC_Local2Global(g1); //把gv1转换成global资源

	local_1 = obj_add("obj_local_1", 1, 2);

	uintptr_t _mem_gc_point = ezGCPoint_insert();
	{
		test1();

		struct mem_obj *sv;
		sv = obj_add("obj_slave_G1", 1, 2);
		ezGC_Local2Slave(sv, g1);

		sv = obj_add("obj_slave_L1", 1, 2);
		ezGC_Local2Slave(sv, local_1);
		
		sv = obj_add("obj_slave_G2", 1, 2);
		ezGC_Local2Slave(sv, g2);

		sv = obj_add("obj_slave_L2", 1, 2);
		ezGC_Local2Slave(sv, local_2);
	}
	ezGCPoint_clean(_mem_gc_point);
	/*自动销毁在_mem_gc_point_insert和clean
	之间创建的local资源*/

	/*自动释放所有local和global资源, 并exit()*/
	ezGC_exit(0);
	return 0;
}

