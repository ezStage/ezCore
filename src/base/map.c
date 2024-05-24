#include <ezCore/ezCore.h>

typedef struct ezMapNode_st ezMapNode;

struct ezMapNode_st
{
	ezBTree btree; /* ezBTree.id是整数key或字符串key的hash值*/
	ezMapNode *next; /*id冲突的链表*/
	intptr_t key_len; /*>0表示是字符串key*/
	ezGC_sub sub;
	//uintptr_t sig=0x5775;
	//char key[0]; /*也是 uintptr_t 对齐*/
};

#define IN_NOKEY_LIST		0x1
#define IN_BTREE			0x2
#define IN_BTREE_LIST		0x3

#define node_flags(pn) ((pn)->btree.no_use)

#define STATUS_NORMAL		0
#define STATUS_FREE 		1
#define STATUS_TRAVERSAL 	2
#define STATUS_SORT 		3

#define ARRAY_STATIC_MIN 8
#define ARRAY_DYNAMIC_NUM 64
struct ezMap_st
{
	ezBTree *root;
	int32_t node_size; /*offset_of(ezMapNode, sig)*/
	int32_t node_n;
	int32_t array_n; /*0: not support index*/
	uint8_t status;
	ezList no_key_head; //no key节点链表
	void **pp_array; /* 指向 ezMapNode.sub.p[] 的指针数组 */
	const ezGC_type *node_type;
};

static ezGC_type EZ_MAP_TYPE;

/****************************************************************************************/
static void _gc_map_destroy(void *p)
{
	ezMap *map = (ezMap *)p;
	if(map->status != STATUS_NORMAL) {
		ezLog_err("map status %d error\n", map->status);
	}
	if(map->pp_array) {
		free(map->pp_array);
	}
	return;
}

static void _gc_map_node_destroy(void *p)
{
	ezMapNode *pnode = (ezMapNode *)p;
	ezGC_destroy_sub(pnode->sub);
}

static ezGC_type EZ_MAP_NODE_TYPE = {
	.destroy_func = _gc_map_node_destroy,
	.name = "ezMapNode",
};

static ezGC_type EZ_MAP_TYPE = {
	.destroy_func = _gc_map_destroy,
	.name = "ezMap",
};

ezMap * ezMap_create(int node_size, const ezGC_type *node_type, ezBool support_index)
{
	ezMap *ret;
	if(node_size < 0) return NULL;

	void **pp_array = NULL;
	if(support_index) {
		pp_array = (void **)malloc(ARRAY_STATIC_MIN*sizeof(void *));
		if(pp_array == NULL) return NULL;
	}

	ret = (ezMap *)ezAlloc(sizeof(ezMap), &EZ_MAP_TYPE, TRUE);
	if(ret == NULL) {
		if(pp_array) free(pp_array);
		return NULL;
	}

	if(support_index) {
		ret->array_n = ARRAY_STATIC_MIN;
		ret->pp_array = pp_array;
	}

	ezList_init_head(&(ret->no_key_head));
	int32_t n = sizeof(ezMapNode) + node_size;
	ret->node_size = EZ_ROUND_UP(n, 8);
	ret->node_type = node_type;
	//ret->status = STATUS_NORMAL;
	return ret;	
}

void *ezMap_check_type(ezMap *map, const ezGC_type *node_type)
{
	if(node_type == NULL) {
		ezLog_err("node_type is NULL\n");
		return NULL;
	}
	ezMap *p = ezGC_check_type(map, &EZ_MAP_TYPE);
	if((p != NULL)&&(p->node_type != node_type)) {
		ezLog_err("map node_type is not %s\n", node_type->name);
		return NULL;
	}
	return p;
}

static inline ezBool _create_check_array(ezMap *map)
{
	if(map->status != STATUS_NORMAL) {
		ezLog_err("map status %d error\n", map->status);
		return FALSE;
	}
	int array_n = map->array_n;
	if((array_n > 0)&&(map->node_n == array_n))
	{
		void **pp;
		if(array_n < ARRAY_DYNAMIC_NUM)
			array_n = ARRAY_DYNAMIC_NUM;
		else
			array_n <<= 1;
		pp = (void **)realloc(map->pp_array, array_n*sizeof(void *));
		if(pp == NULL) return FALSE;

		map->pp_array = pp;
		map->array_n = array_n;
	}
	return TRUE;
}

void * ezMap_get_node_from_int(ezMap *map, uintptr_t key, ezBool if_create)
{
	ezMapNode *pn, *pnh=NULL;

	if((map == NULL)||(key == 0)) return NULL;

	ezBTree *pb = ezBTree_find(map->root, key);
	if(pb != NULL)
	{
		pnh = EZ_FROM_MEMBER(ezMapNode, btree, pb);
		/*字符串hash key和整数key都在一个btree上,
		相同btree->id的node在一个链表上, pnh是链表头*/
		for(pn=pnh; pn; pn = pn->next)
		{//key_len=0表示是整数key
			if(pn->key_len == 0) {
				//这里就不检查sig了
				return pn->sub.p;
			}
		}
	}

	if(!if_create) return NULL;

	if(!_create_check_array(map)) return NULL;

	pn = (ezMapNode *)ezAlloc(map->node_size, &EZ_MAP_NODE_TYPE, TRUE);
	if(pn == NULL) return NULL;

	ezBTree_init(&(pn->btree), key);
	if(pnh == NULL) {
		node_flags(pn) = IN_BTREE;
		ezBTree_insert(&(map->root), &(pn->btree));
	} else {
		node_flags(pn) = IN_BTREE_LIST;
		pn->next = pnh->next;
		pnh->next = pn;
	}

	int node_n = map->node_n;
	map->node_n = node_n+1;
	if(map->pp_array) {
		map->pp_array[node_n] = pn->sub.p;
	}

	ezGC_set_sub(pn, &(pn->sub), map->node_type);
	ezGC_Local2Slave(pn, map);
	return pn->sub.p;
}

void * ezMap_get_node_from_string(ezMap *map, const char *key, intptr_t len, ezBool if_create)
{
	uintptr_t *sig;
	uintptr_t hash;
	ezMapNode *pn, *pnh=NULL;
	ezBTree *pb;

	if((map == NULL)||(key == NULL)) return NULL;

	if(len == -1) {
		hash = ez_get_hash_rlen(key, &len);
	} else if(len > 0) {
		hash = ez_get_hash_len(key, len);
	}
	if(len <= 0) return NULL;

	pb = ezBTree_find(map->root, hash);
	if(pb != NULL)
	{
		pnh = EZ_FROM_MEMBER(ezMapNode, btree, pb);
		/*字符串hash key和整数key都在一个btree上,
		相同btree->id的node在一个链表上, pnh是链表头*/
		for(pn=pnh; pn; pn = pn->next)
		{//key_len>0表示是字符串hash key
			if(pn->key_len == len)
			{
				sig = (uintptr_t *)((uint8_t *)pn + map->node_size);
				if(*sig == 0x5775) {
					if(memcmp(sig+1, key, len) == 0)
						return pn->sub.p;
				} else {
					ezLog_err("ezMap Node bad sig !\n");
				}
			}
		}
	}

	if(!if_create) return NULL;

	if(!_create_check_array(map)) return NULL;

	int nlen = map->node_size;
	pn = (ezMapNode *)ezAlloc(nlen+sizeof(uintptr_t)+len+1, &EZ_MAP_NODE_TYPE, FALSE);
	if(pn == NULL) {
		ezLog_errno("malloc");
		return NULL;
	}

	memset(pn, 0, nlen);
	ezBTree_init(&(pn->btree), hash);
	if(pnh == NULL) {
		node_flags(pn) = IN_BTREE;
		ezBTree_insert(&(map->root), &(pn->btree));
	} else {
		node_flags(pn) = IN_BTREE_LIST;
		pn->next = pnh->next;
		pnh->next = pn;
	}
	pn->key_len = len;
	sig = (uintptr_t *)((uint8_t *)pn + map->node_size);
	*sig = 0x5775;
	memcpy(sig+1, key, len); // len > 0
	*((char *)(sig+1) + len) = '\0';

	int node_n = map->node_n;
	map->node_n = node_n+1;
	if(map->pp_array) {
		map->pp_array[node_n] = pn->sub.p;
	}

	ezGC_set_sub(pn, &(pn->sub), map->node_type);
	ezGC_Local2Slave(pn, map);
	return pn->sub.p;
}

void * ezMap_get_node_from_index(ezMap *map, int index)
{
	if(map == NULL) return NULL;
	if((index >= 0)&&(index < map->node_n))
	{
		if(map->pp_array == NULL) {
			ezLog_err("map not support index\n");
			return NULL;
		}
		return map->pp_array[index]; //直接就是 pn->sub.p;
	}
	return NULL;
}

void * ezMap_add_node_nokey(ezMap *map)
{
	if(map == NULL) return NULL;

	if(!_create_check_array(map)) return NULL;

	ezMapNode *pn;
	pn = (ezMapNode *)ezAlloc(map->node_size, &EZ_MAP_NODE_TYPE, TRUE);
	if(pn == NULL) return NULL;

	node_flags(pn) = IN_NOKEY_LIST;

	int node_n = map->node_n;
	map->node_n = node_n+1;
	if(map->pp_array) {
		map->pp_array[node_n] = pn->sub.p;
	}
	
	ezList *plist = (ezList *)&(pn->btree.left);
	ezList_add_tail(plist, &(map->no_key_head));
	
	ezGC_set_sub(pn, &(pn->sub), map->node_type);
	ezGC_Local2Slave(pn, map);
	return pn->sub.p;
}

/*=======================================================*/

static void _delete_all_by_array(ezMap *map)
{
	void **pp, *p;
	ezMapNode *pn;
	int n;
	pp = map->pp_array;
	n = map->node_n;
	while(--n >= 0) {
		p = *pp++;
		pn = EZ_FROM_MEMBER(ezMapNode, sub.p, p);
		ezGC_SlaveDestroy(pn, &EZ_MAP_TYPE);
	}
}

static void _delete_all_by_btree(ezMap *map)
{
	ezMapNode *pn, *pn2;
	ezBTree *pb, *stack[64];
	int i=0;

	pb = map->root;
	while((pb != NULL)||(i > 0))
	{
		while(pb != NULL) {
			stack[++i] = pb;
/*=== 先序遍历pb ===*/
			pb = pb->left;
		}
		pb = stack[i--];
/*--- 中序遍历pb ---*/
pn = EZ_FROM_MEMBER(ezMapNode, btree, pb);
/*----------------*/
		pb = pb->right;
/*----------------*/
do {
	pn2 = pn->next;
	ezGC_SlaveDestroy(pn, &EZ_MAP_TYPE);
	pn = pn2;
} while(pn);
/*----------------*/
	}
}

static void _delete_all_by_no_key_list(ezMap *map)
{
	ezList *plist, *tmp;
	ezList_for_each_safe(plist, tmp, &(map->no_key_head))
	{
		ezMapNode *pn;
		pn = EZ_FROM_MEMBER(ezMapNode, btree.left, plist);
		ezList_del(plist);
		ezGC_SlaveDestroy(pn, &EZ_MAP_TYPE);
	}
}

void ezMap_delete_all(ezMap *map)
{
	if(map == NULL) return;
	if(map->status != STATUS_NORMAL) {
		ezLog_err("map status %d error\n", map->status);
		return;
	}
	map->status = STATUS_FREE;
	if(map->pp_array) {
		_delete_all_by_array(map);
		map->array_n = ARRAY_STATIC_MIN;
		map->pp_array = (void **)realloc(map->pp_array, ARRAY_STATIC_MIN*sizeof(void *));
	} else {
		_delete_all_by_btree(map);
		_delete_all_by_no_key_list(map);
	}
	ezList_init_head(&(map->no_key_head));

	map->status = STATUS_NORMAL;
	map->node_n = 0;
	map->root = NULL;
	return;
}

void ezMapNode_delete(void *data)
{
	ezMapNode *pn;
	ezBTree *pb;
	ezMapNode *pn2;
	void **pp;
	int node_n;
	ezMap *map;

	pn = ezGC_check_type(data, &EZ_MAP_NODE_TYPE);
	if(pn == NULL) return;

	map = ezGC_get_owner(pn, &EZ_MAP_TYPE);
	if(map->status != STATUS_NORMAL) {
		ezLog_err("map status %d error\n", map->status);
		return;
	}

	node_n = map->node_n;
	pp = map->pp_array;
	if(pp != NULL)
	{
		int i, n;
		for(i=0; i<node_n; ++i)
		{
			if(pp[i] == data) {
				memmove(pp+i, pp+i+1, (node_n-i-1)*sizeof(void *));
				break;
			}
		}
		if(i == node_n) {
			ezLog_err("not found in array");
			return;
		}

		n = (map->array_n >> 1);
		if(node_n <= (n>>1))
		{
			if(n >= ARRAY_DYNAMIC_NUM) {
				map->array_n = n;
				map->pp_array = (void **)realloc(pp, n*sizeof(void *));
			}
		}
	}

	switch(node_flags(pn))
	{
	case IN_NOKEY_LIST: {
			ezList *plist = (ezList *)&(pn->btree.left);
			ezList_del(plist);
		}
		break;
	case IN_BTREE:
		ezBTree_delete(&(map->root), &(pn->btree));
		if((pn2=pn->next) != NULL) {
			ezBTree_insert(&(map->root), &(pn2->btree));
			node_flags(pn2) = 1;
		}
		break;
	case IN_BTREE_LIST:
		pb = ezBTree_find(map->root, pn->btree.key);
		if(pb == NULL) {
			ezLog_err("not found in btree\n");
			return;
		}
		pn2 = EZ_FROM_MEMBER(ezMapNode, btree, pb);
		while((pn2 != NULL)&&(pn2->next != pn)) pn2 = pn2->next;
		if(pn2 == NULL) {
			ezLog_err("not found in key list\n");
			return;
		}
		pn2->next = pn->next;
		break;
	default:
		ezLog_err("node flags error\n");
		return;
	}

	map->node_n = node_n - 1;
	ezGC_SlaveDestroy(pn, &EZ_MAP_TYPE);
	return;
}

/*=======================================================*/

ezMap * ezMapNode_get_map(const void *data)
{
	ezMapNode *pn;
	pn = ezGC_check_type(data, &EZ_MAP_NODE_TYPE);
	return ezGC_get_owner(pn, &EZ_MAP_TYPE);
}

const char * ezMapNode_get_key_string(const void *data, intptr_t *rlen)
{
	ezMap *map;
	ezMapNode *pn;
	uintptr_t *sig;

	pn = ezGC_check_type(data, &EZ_MAP_NODE_TYPE);
	if(pn == NULL) {
		if(rlen) *rlen = 0;
		return NULL;
	}

	if(rlen) *rlen = pn->key_len;
	if(pn->key_len == 0) return NULL;

	map = ezGC_get_owner(pn, &EZ_MAP_TYPE);

	sig = (uintptr_t *)((uint8_t *)pn + map->node_size);
	if(*sig == 0x5775) {
		return (char *)(sig+1);
	}
	ezLog_err("Map Node bad sig !\n");
	if(rlen) *rlen = 0;
	return NULL;
}

uintptr_t ezMapNode_get_key_int(const void *data)
{
	ezMapNode *pn;
	pn = ezGC_check_type(data, &EZ_MAP_NODE_TYPE);
	if(pn == NULL) return 0;
	if(node_flags(pn) == 0) return 0;
	if(pn->key_len != 0) return 0;
	return pn->btree.key;
}

int ezMap_get_count(ezMap *map)
{
	return map? map->node_n : 0;
}

static void * _traversal_node_by_array(ezMap *map, ezMapNodeVisitFunc func, void *arg)
{
	void **pp, *p;
	int n;
	pp = map->pp_array;
	n = map->node_n;
	while(--n >= 0) {
		p = *pp++;
		if(func(p, arg)) return p;
	}
	return NULL;
}

static void * _traversal_node_by_btree(ezMap *map, ezMapNodeVisitFunc func, void *arg)
{
	ezMapNode *pn;
	ezBTree *pb, *stack[64];
	int i=0;

	pb = map->root;
	while((pb != NULL)||(i > 0))
	{
		while(pb != NULL) {
			stack[++i] = pb;
/*=== 先序遍历pb ===*/
pn = EZ_FROM_MEMBER(ezMapNode, btree, pb);
do {
	if(func(pn->sub.p, arg)) return pn->sub.p;
	pn = pn->next;
} while(pn);
/*=============*/
			pb = pb->left;
		}
		pb = stack[i--];
/*=== 中序遍历pb ===*/
		pb = pb->right;
	}
	return NULL;
}

static void * _traversal_node_by_nokey_list(ezMap *map, ezMapNodeVisitFunc func, void *arg)
{
	ezList *plist;
	ezList_for_each(plist, &(map->no_key_head))
	{
		ezMapNode *pn;
		pn = EZ_FROM_MEMBER(ezMapNode, btree.left, plist);
		if(func(pn->sub.p, arg)) return pn->sub.p;
	}
	return NULL;
}

void * ezMap_traversal(ezMap *map, ezMapNodeVisitFunc func, void *arg)
{
	void *ret;
	if((map == NULL)||(func == NULL)) return NULL;
	if(map->status != STATUS_NORMAL) {
		ezLog_err("map status %d error\n", map->status);
		return NULL;
	}
	map->status = STATUS_TRAVERSAL;
	if(map->pp_array) {
		ret = _traversal_node_by_array(map, func, arg);
	} else {
		ret = _traversal_node_by_btree(map, func,arg);
		if(ret == NULL)
			ret = _traversal_node_by_nokey_list(map, func,arg);
	}
	map->status = STATUS_NORMAL;
	return ret;
}

/*==============================================*/

void * ezMap_seek_node_nokey(ezMap *map, void *data, ezBool if_next_or_prev)
{
	ezMapNode *pnode;
	ezList *plist;

	if(map == NULL) return NULL;

	if(data != NULL) {
		pnode = ezGC_check_type(data, &EZ_MAP_NODE_TYPE);
		if(pnode == NULL) return NULL;
		if(!ezGC_check_owner(pnode, map)) return NULL;
		if(node_flags(pnode) != IN_NOKEY_LIST) {
			ezLog_err("map node is not nokey\n");
			return NULL;
		}
	}

	if(if_next_or_prev)
	{
		if(data != NULL) {
			plist = (ezList *)(pnode->btree.left);
		} else
			plist = map->no_key_head.next;
	}
	else
	{
		if(data != NULL) {
			plist = (ezList *)(pnode->btree.right);
		} else
			plist = map->no_key_head.prev;
	}

	if(plist == &(map->no_key_head)) return NULL;
	pnode = EZ_FROM_MEMBER(ezMapNode, btree.left, plist);
	return pnode->sub.p;
}

/*==============================================*/

struct _map_qsort_st {
	ezSortCompareFunc comp_func;
	void *arg;
};

static int _map_qsort_func(const void *n1, const void *n2, void *arg)
{
	struct _map_qsort_st *tmp = (struct _map_qsort_st *)arg;
	void *p1 = *(void **)n1;
	void *p2 = *(void **)n2;
	return tmp->comp_func(p1, p2, tmp->arg);
}

void ezMap_qsort_index(ezMap *map, ezSortCompareFunc comp_func, void *arg)
{
	struct _map_qsort_st tmp;
	tmp.comp_func = comp_func;
	tmp.arg = arg;
	if(map && (map->node_n > 1))
	{
		if(map->pp_array == NULL) {
			ezLog_err("map not support index\n");
			return;
		}
		if(map->status != STATUS_NORMAL) {
			ezLog_err("map status %d error\n", map->status);
			return;
		}
		map->status = STATUS_SORT;
		qsort_r(map->pp_array, map->node_n, sizeof(void *), _map_qsort_func, &tmp);
		map->status = STATUS_NORMAL;
	}
	return;
}

void ** ezMap_get_index_head(ezMap *map)
{
	void **pp = map->pp_array;
	if(pp == NULL) {
		ezLog_err("map not support index\n");
	}
	return pp;
}

