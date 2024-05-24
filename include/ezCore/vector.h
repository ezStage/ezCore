#ifndef _EZ_VECTOR_H
#define _EZ_VECTOR_H

typedef struct ezVector_st ezVector;
ezVector * ezVector_create(const ezGC_type *type);

ezBool ezVector_check_type(ezVector *v, const ezGC_type *type);

int ezVector_get_count(ezVector *v);
ezResult ezVector_insert(ezVector *v, int i, void *p);
void * ezVector_get(ezVector *v, int i);
void * ezVector_take_out(ezVector *v, int i);
void ezVector_delete(ezVector *v, int i);

//返回之前的对象, 失败返回NULL
void * ezVector_replace(ezVector *v, int i, void *p);

//在尾部增加
#define ezVector_add(v, p) ezVector_insert(v, -1, p);

#endif

