#ifndef _EZ_DICT_H
#define _EZ_DICT_H

typedef struct _ezDict ezDict;
typedef struct ezDictNode_st ezDictNode;

ezDict * ezDict_create(void);
void ezDict_dump(ezDict *dict);
ezDict * ezDict_copy(ezDict *dst, ezDict *src);
ezDict * ezDict_dup(ezDict *src);
int ezDict_get_count(ezDict *dict);
void ezDict_delete_all(ezDict *dict);

ezDictNode * ezDict_get_key_node(ezDict *dict, const char *key, int len, ezBool if_create);
ezDictNode * ezDict_get_index_node(ezDict *dict, int index); /*获取指定序号的node*/
ezDictNode * ezDict_add_nokey_node(ezDict *dict); /*创建一个没有key的node*/

void ezDictNode_delete(ezDictNode *node);

ezFourCC ezDictNode_get_fourcc(ezDictNode *pn);
void ezDictNode_set_fourcc(ezDictNode *pn, ezFourCC four);
/*由用户自己保证ezFourCC中只有字母，数字和下划线*/

/*DictNode新创建时type是none, 之后可以设置成
int整数，float浮点数，str字符串或另一个Dict*/

/*当node类型是none, int或float时, 可以设置int*/
ezResult ezDictNode_set_int(ezDictNode *tnode, intptr_t value);

/*当node类型是none, int或float时, 可以设置float*/
ezResult ezDictNode_set_float(ezDictNode *tnode, double value);

/*当node类型是none或dict时, 可以设置子dict,
该子dict将成为tnode的slave资源,
如果该tnode之前已经设置过其他子dict, 之前的子dict将自动free*/
ezResult ezDictNode_set_dict(ezDictNode *tnode, ezDict *value);

/*当node类型是none, str时, 可以设置str
该tnode之前设置过str将自动free*/
/*如果len==-1, 将通过strlen(value)获取真实长度*/
ezResult ezDictNode_set_str(ezDictNode *tnode, const char *value, intptr_t len);
/*DictNode内部不分配内存保存str, 只记录指针*/
ezResult ezDictNode_set_str_p(ezDictNode *tnode, const char *value, intptr_t len);

typedef enum {
	EZ_DICT_NODE_NONE		=0,
	EZ_DICT_NODE_STR		='S',
	EZ_DICT_NODE_DICT		='D',
	EZ_DICT_NODE_INT		='I',
	EZ_DICT_NODE_FLOAT		='F',
} ezDictNodeTypeEnum;
ezDictNodeTypeEnum ezDictNode_get_type(ezDictNode *tnode);
const char * ezDictNode_get_key(ezDictNode *tnode, intptr_t *rlen);

/*如果node类型是int或float, 返回整数值. 否则一律返回0*/
intptr_t ezDictNode_get_int(ezDictNode *tnode);
/*如果node类型是int或float, 返回浮点数值. 否则一律返回0.0*/
double ezDictNode_get_float(ezDictNode *tnode);
intptr_t ezTableNode_get_scale_int(ezDictNode *pn, intptr_t scale);
/*如果node类型是str, 返回内部字符串指针, 否则一律返回NULL*/
const char * ezDictNode_get_str(ezDictNode *tnode, intptr_t *rlen);
/*如果node类型是Dict, 返回指针, 否则一律返回NULL*/
ezDict * ezDictNode_get_dict(ezDictNode *tnode, ezBool take_out);
/*take_out: 是否把Node中的子Dict拿出, 该子Dict不再是tnode的slave资源, 成为local资源*/

/*得到dict所在父dict_node*/
ezDictNode * ezDict_get_parent_node(ezDict *dict);

/************************************************************************/

static inline ezResult ezDict_add_key_int(ezDict *dict, const char * key, intptr_t value)
{
	return ezDictNode_set_int(ezDict_get_key_node(dict, key, -1, TRUE), value);
}

static inline ezResult ezDict_add_key_str(ezDict *dict, const char * key, const char *value, intptr_t len)
{
	return ezDictNode_set_str(ezDict_get_key_node(dict, key, -1, TRUE), value, len);
}

static inline ezResult ezDict_add_key_float(ezDict *dict, const char * key, double value)
{
	return ezDictNode_set_float(ezDict_get_key_node(dict, key, -1, TRUE), value);
}

static inline ezResult ezDict_add_key_dict(ezDict *dict, const char * key, ezDict *value)
{
	return ezDictNode_set_dict(ezDict_get_key_node(dict, key, -1, TRUE), value);
}

/*------------------------------------------------------*/

static inline intptr_t ezDict_get_key_int(ezDict *dict, const char *key)
{
	return ezDictNode_get_int(ezDict_get_key_node(dict, key, -1, FALSE));
}

static inline const char * ezDict_get_key_str(ezDict *dict, const char *key, intptr_t *rlen)
{
	return ezDictNode_get_str(ezDict_get_key_node(dict, key, -1, FALSE), rlen);
}

static inline double ezDict_get_key_float(ezDict *dict, const char *key)
{
	return ezDictNode_get_float(ezDict_get_key_node(dict, key, -1, FALSE));
}

static inline ezDict * ezDict_get_key_dict(ezDict *dict, const char *key)
{
	return ezDictNode_get_dict(ezDict_get_key_node(dict, key, -1, FALSE), FALSE);
}

static inline ezDict * ezDict_takeout_key_dict(ezDict *dict, const char *key)
{
	return ezDictNode_get_dict(ezDict_get_key_node(dict, key, -1, FALSE), TRUE);
}

/*------------------------------------------------------*/

static inline ezResult ezDict_add_nokey_int(ezDict *dict, intptr_t value)
{
	return ezDictNode_set_int(ezDict_add_nokey_node(dict), value);
}

static inline ezResult ezDict_add_nokey_str(ezDict *dict, const char *value, intptr_t len)
{
	return ezDictNode_set_str(ezDict_add_nokey_node(dict), value, len);
}

static inline ezResult ezDict_add_nokey_float(ezDict *dict, double value)
{
	return ezDictNode_set_float(ezDict_add_nokey_node(dict), value);
}

static inline ezResult ezDict_add_nokey_dict(ezDict *dict, ezDict *value)
{
	return ezDictNode_set_dict(ezDict_add_nokey_node(dict), value);
}

/*------------------------------------------------------*/

#define ezDict_get_key_type(dict, key) \
	ezDictNode_get_type(ezDict_get_key_node(dict, key, -1, FALSE))

#define ezDict_find_key_node(dict, key) \
	ezDict_get_key_node(dict, key, -1, FALSE)

#define ezDict_del_key_node(dict, key) \
	ezDictNode_delete(ezDict_get_key_node(dict, key, -1, FALSE))

/*---------------------------------------------------------------*/

#endif

