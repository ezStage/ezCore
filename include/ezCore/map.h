#ifndef _EZ_MAP_H
#define _EZ_MAP_H

typedef struct ezMap_st ezMap;

ezMap *ezMap_create(int node_size, const ezGC_type *node_type, ezBool support_index);

/*检测map的node是否是特定类型*/
void *ezMap_check_type(ezMap *map, const ezGC_type *node_type);

int ezMap_get_count(ezMap *map);
void ezMap_delete_all(ezMap *map);

/*如果len为-1,实际取strlen(key).
如果key==NULL或len==0, 不是正常的key, 返回NULL.
(注意: key为"\0" 且 len==1 是正常的key*/
void * ezMap_get_node_from_string(ezMap *map, const char *key, intptr_t len, ezBool if_create);
/*key==0不是正常的key, 返回NULL*/
void * ezMap_get_node_from_int(ezMap *map, uintptr_t key, ezBool if_create);
void * ezMap_get_node_from_index(ezMap *map, int index);/*该函数只获取, 不创建*/
void * ezMap_add_node_nokey(ezMap *map);/*在尾部创建*/

#define ezMap_find_node_string(map, key) ezMap_get_node_from_string(map, key, -1, FALSE)
#define ezMap_add_node_string(map, key) ezMap_get_node_from_string(map, key, -1, TRUE)
#define ezMap_find_node_int(map, key) ezMap_get_node_from_int(map, key, FALSE)
#define ezMap_add_node_int(map, key) ezMap_get_node_from_int(map, key, TRUE)

#define ezMap_find_node_index(map, index) ezMap_get_node_from_index(map, index)

/*data为NULL表示得到第一个或最后一个, data不为NULL得到下一个或上一个*/
void *ezMap_seek_node_nokey(ezMap *map, void *data, ezBool if_next_or_prev);

/*当support_index==TRUE时, 内部有指针数组, 时间复杂度为O(N).
否则仅为二叉树删除*/
void ezMapNode_delete(void *data);

ezMap * ezMapNode_get_map(const void *data);

/* 返回NULL表示不存在*/
const char * ezMapNode_get_key_string(const void *data, intptr_t *rlen);
uintptr_t ezMapNode_get_key_int(const void *data); /*返回0表示不存在*/

#define ezMap_get_index_key_string(map, i) \
	({ void *d = ezMap_find_node_index(map, i); ezMapNode_get_key_string(d, NULL); })

/*VisitFunc()返回TRUE表示终止遍历*/
typedef ezBool (* ezMapNodeVisitFunc)(void *data, void *arg);
/*返回终止遍历的node. 如果所有节点都遍历完毕, 没有终止遍历就返回NULL*/
void * ezMap_traversal(ezMap *map, ezMapNodeVisitFunc func, void *arg);

void ezMap_qsort_index(ezMap *map, ezSortCompareFunc comp_func, void *arg);

void ** ezMap_get_index_head(ezMap *map);

#endif

