#include <ezCore/ezCore.h>

#define NODE_STATIC_NUM 26 /*静态数组大小*/
#define NODE_DYNAMIC_NUM 64 /*动态数组大小*/

struct ezVector_st
{
	const ezGC_type *type; //为NULL表示不限类型
	int mem_num;
	int num;
	void **pp;
	void *sp[NODE_STATIC_NUM];
};

static void _gc_vector_destroy(void *p)
{
	ezVector *v = (ezVector *)p;
	if(v->mem_num > NODE_STATIC_NUM)
		free(v->pp);
	return;
}

static ezGC_type EZ_VECTOR_TYPE = {
	.destroy_func = _gc_vector_destroy,
};
ezVector * ezVector_create(const ezGC_type *type)
{
	ezVector *v;
	v = ezAlloc(sizeof(ezVector), &EZ_VECTOR_TYPE, TRUE);
	if(v == NULL) return NULL;
	v->mem_num = NODE_STATIC_NUM;
	//v->num = 0;
	v->type = type;
	v->pp = v->sp;
	return v;
}

ezBool ezVector_check_type(ezVector *v, const ezGC_type *type)
{
	if(!v) return FALSE;
	return (v->type == type);
}

int ezVector_get_count(ezVector *v)
{
	if(!v) return 0;
	return v->num;
}

ezResult ezVector_insert(ezVector *v, int i, void *p)
{
	int a, m;
	void **array;
	if((v == NULL)||(p == NULL)) return EZ_FAIL;

	if(v->type && !ezGC_check_type(p, v->type)) {
		ezLog_err("p type error\n");
		return EZ_EINVAL;
	}

	a = v->num;
	if(i == -1) i = a;
	if((i < 0)||(i > a)) return EZ_FAIL;

	array = v->pp;
	m = a+1;
	if(m > v->mem_num)
	{
		if(m > NODE_DYNAMIC_NUM) {
			m <<= 1;
			array = (void **)realloc(array, m*sizeof(void *));
			if(array == NULL) return EZ_ENOMEM;
		} else {
			m = NODE_DYNAMIC_NUM;
			array = (void **)malloc(m*sizeof(void *));
			if(array == NULL) return EZ_ENOMEM;
			memcpy(array, v->sp, NODE_STATIC_NUM*sizeof(void *));
		}
		v->pp = array;
		v->mem_num = m;
	}

	ezResult ret = ezGC_Local2Slave(p, v);
	if(ret != EZ_SUCCESS) return ret;

	v->num += 1;
	array += i;
	if((m=a-i) > 0) memmove(array+1, array, m*sizeof(void *));
	*array = p;
	return EZ_SUCCESS;
}

void * ezVector_get(ezVector *v, int i)
{
	if(v == NULL) return NULL;
	if(i == -1) i = v->num-1;
	if((i < 0)||(i >= v->num)) return NULL;
	return v->pp[i];
}

void * ezVector_take_out(ezVector *v, int i)
{
	int a, m;
	void **array;
	if(v == NULL) return NULL;
	a = v->num;
	if(i == -1) i = a-1;
	if((i < 0)||(i >= a)) return NULL;
	array = v->pp+i;

	void *p = *array;
	ezGC_Slave2Local(p, &EZ_VECTOR_TYPE);

	if((m=a-i-1) > 0) {
		memmove(array, array+1, m*sizeof(void *));
		i += m; //i=a-1;
	}//else m==0 即 i=a-1;
	array[i] = NULL;
	v->num = i;
	m = (v->mem_num>>1);
	if(i <= (m>>1))
	{
		if(m >= NODE_DYNAMIC_NUM)
		{
			array = (void **)realloc(v->pp, m*sizeof(void *));
			if(array != NULL) {
				v->pp = array;
				v->mem_num = m;
			}
		}
	}
	return p;
}

void * ezVector_replace(ezVector *v, int i, void *p)
{
	int a;
	void **array;
	if((v == NULL)||(p == NULL)) return NULL;

	if(v->type && !ezGC_check_type(p, v->type)) {
		ezLog_err("p type error\n");
		return NULL;
	}

	a = v->num;
	if(i == -1) i = a-1;
	if((i < 0)||(i >= a)) {
		ezLog_err("index %d error\n", i);
		return NULL;
	}

	ezResult ret = ezGC_Local2Slave(p, v);
	if(ret != EZ_SUCCESS) return NULL;

	array = v->pp+i;
	void *old = *array;
	*array = p;
	ezGC_Slave2Local(old, &EZ_VECTOR_TYPE);
	return old;	
}

void ezVector_delete(ezVector *v, int i)
{
	void *p = ezVector_take_out(v, i);
	ezDestroy(p);
	return;
}

