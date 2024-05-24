#include <ezCore/ezCore.h>

struct ezDictNode_st
{
	union {
		intptr_t intber; /*int number*/
		double floatber; /*float number*/
		struct {
			const char *str;
			intptr_t str_len;
		};
		ezDict *dict;
	} value;
	ezFourCC four;
	int8_t type; /*value类型:none, int, float, str, dict*/
	ezBool str_malloc; /*str是否单独分配了内存*/
};

#define _str_free(str) free((void *)(str))

static void _dict_node_destroy(void *p)
{
	ezDictNode *pn = (ezDictNode *)p;
	switch(pn->type)
	{
	case EZ_DICT_NODE_STR:
		if(pn->str_malloc)
			_str_free(pn->value.str);
		break;
	}
	return;
}

static ezGC_type EZ_DICT_NODE_TYPE={
	.destroy_func = _dict_node_destroy,
	.name = "ezDict_node",
};

/*=====================================================*/

/*dict是没有int key的map*/
ezDict *ezDict_create(void)
{
	ezMap *ret;
	ret = ezMap_create(sizeof(ezDictNode), &EZ_DICT_NODE_TYPE, TRUE);
	return (ezDict *)ret;	
}

void ezDict_delete_all(ezDict *dict)
{
	ezMap_delete_all((ezMap *)dict);
	return;
}

static void _dict_dump(ezDict *dict, int depth)
{
	ezDictNode *pn;
	const char *key;
	int n, i, d;

	if(dict == NULL) return;

	n = ezMap_get_count((ezMap *)dict);
	for(i=0; i<n; i++)
	{
		pn = ezMap_find_node_index((ezMap *)dict, i);
		key = ezMapNode_get_key_string(pn, NULL);
		for(d=0; d<depth; ++d) ezLog_print("\x20\x20");
		if(key) {
			ezLog_print("%s", key);
			if(pn->type != EZ_DICT_NODE_NONE) ezLog_print("=");
		}
		switch(pn->type)
		{
		case EZ_DICT_NODE_NONE:
			break;
		case EZ_DICT_NODE_INT:
			ezLog_print("%zd", pn->value.intber);
			break;
		case EZ_DICT_NODE_STR:
			ezLog_print("\"%s\"", pn->value.str? pn->value.str : "");
			break;
		case EZ_DICT_NODE_FLOAT:
			ezLog_print("%f", pn->value.floatber);
			break;
		case EZ_DICT_NODE_DICT:
			ezLog_print("{\n");
			_dict_dump(pn->value.dict, depth+1);
			for(d=0; d<depth; ++d) ezLog_print("\x20\x20");
			ezLog_print("}");
			break;
		}
		if(pn->four) ezLog_print("@%.4s", (char *)&(pn->four));
		ezLog_print("\n");
	}
	return;
}
void ezDict_dump(ezDict *dict)
{
	_dict_dump(dict, 0);
	return;
}

ezDict * ezDict_dup(ezDict *src)
{
	if(src == NULL) return NULL;
	ezDict *dst = ezDict_create();
	return ezDict_copy(dst, src);
}

ezDict * ezDict_copy(ezDict *dst, ezDict *src)
{
	if((src == NULL)||(dst == NULL))
		return dst;

	const char *key;
	intptr_t key_len;
	int n, i;
	ezDictNode *pn, *pn_new;

	n = ezMap_get_count((ezMap *)src);
	for(i=0; i<n; i++)
	{
		pn = ezMap_find_node_index((ezMap *)src, i);
		key = ezMapNode_get_key_string(pn, &key_len);
		if(key)
			pn_new = ezDict_get_key_node(dst, key, key_len, TRUE);
		else
			pn_new = ezDict_add_nokey_node(dst);
		if(pn_new == NULL) break;
		if(pn->four) ezDictNode_set_fourcc(pn_new, pn->four);
		switch(pn->type)
		{
		case EZ_DICT_NODE_NONE:
			break;
		case EZ_DICT_NODE_INT:
			ezDictNode_set_int(pn_new, pn->value.intber);
			break;
		case EZ_DICT_NODE_STR:
			ezDictNode_set_str(pn_new, pn->value.str, pn->value.str_len);
			break;
		case EZ_DICT_NODE_FLOAT:
			ezDictNode_set_float(pn_new, pn->value.floatber);
			break;
		case EZ_DICT_NODE_DICT:
			ezDictNode_set_dict(pn_new, ezDict_dup(pn->value.dict));
			break;
		}
	}
	return dst;
}

int ezDict_get_count(ezDict *dict)
{
	return ezMap_get_count((ezMap *)dict);
}

/******************************************************************************************/
ezDictNode * ezDict_get_key_node(ezDict *dict, const char *key, int32_t len, ezBool if_create)
{
	ezDictNode *pn;
	pn = ezMap_get_node_from_string((ezMap *)dict, key, len, if_create);
	return pn;
}

ezDictNode * ezDict_get_index_node(ezDict *dict, int index)
{
	ezDictNode *pn;
	pn = ezMap_find_node_index((ezMap *)dict, index);
	return pn;
}

ezDictNode * ezDict_add_nokey_node(ezDict *dict)
{
	ezDictNode *pn;
	pn = ezMap_add_node_nokey((ezMap *)dict);
	return pn;
}

/*---------------------------------------------------------------------*/

void ezDictNode_set_fourcc(ezDictNode *pn, ezFourCC four)
{
	if(pn) pn->four = four;
	return;
}

ezResult ezDictNode_set_int(ezDictNode *pn, intptr_t value)
{
	if(pn == NULL) return EZ_ENOENT;

	if(pn->type == EZ_DICT_NODE_INT) {
		pn->value.intber = value;
		return EZ_SUCCESS;
	}

	if((pn->type == EZ_DICT_NODE_NONE) ||
		(pn->type == EZ_DICT_NODE_FLOAT))
	{
		pn->type = EZ_DICT_NODE_INT;
		pn->value.intber = value;
		return EZ_SUCCESS;
	}

	return EZ_ECLASH;
}
ezResult ezDictNode_set_float(ezDictNode *pn, double value)
{
	if(pn == NULL) return EZ_ENOENT;

	if(pn->type == EZ_DICT_NODE_FLOAT) {
		pn->value.floatber = value;
		return EZ_SUCCESS;
	}

	if((pn->type == EZ_DICT_NODE_NONE) ||
		(pn->type == EZ_DICT_NODE_INT))
	{
		pn->type = EZ_DICT_NODE_FLOAT;
		pn->value.intber = value;
		return EZ_SUCCESS;
	}

	return EZ_ECLASH;
}

ezResult ezDictNode_set_dict(ezDictNode *pn, ezDict *value)
{
	if(pn != NULL)
	{
		if((pn->type != EZ_DICT_NODE_NONE) &&
			(pn->type != EZ_DICT_NODE_DICT))
		{
			ezLog_err("node type=%d\n", pn->type);
			return EZ_ECLASH;
		}

		if(value != NULL) {
			if(ezGC_Local2Slave(value, pn) != EZ_SUCCESS)
				return EZ_ECLASH;
		}

		if(pn->type == EZ_DICT_NODE_NONE) {
			pn->type = EZ_DICT_NODE_DICT;
		} else if(pn->value.dict != NULL) {
			if(pn->value.dict != value)
				ezGC_SlaveDestroy(pn->value.dict, &EZ_DICT_NODE_TYPE);
		}
		pn->value.dict = value;
		return EZ_SUCCESS;
	}
	return EZ_FAIL;
}

ezResult ezDictNode_set_str(ezDictNode *pn, const char *value, intptr_t len)
{
	if(pn == NULL) return EZ_FAIL;

	if((pn->type != EZ_DICT_NODE_NONE) &&
		(pn->type != EZ_DICT_NODE_STR))
	{
		return EZ_ECLASH;
	}

	if(value && len == -1)
		len = strlen(value);

	if(len < 0) return EZ_FAIL;

	if((value == NULL)||(len == 0))
	{
		if(pn->type == EZ_DICT_NODE_NONE)
			pn->type = EZ_DICT_NODE_STR;

		if(pn->str_malloc) {
			pn->str_malloc = FALSE;
			_str_free(pn->value.str);
		}
		pn->value.str = NULL;
		pn->value.str_len = 0;
		return EZ_SUCCESS;
	}

	char * _new = (char *)malloc(len+1);
	if(_new == NULL) return EZ_ENOMEM;

	memcpy(_new, value, len);
	_new[len] = '\0';

	if(pn->type == EZ_DICT_NODE_NONE)
		pn->type = EZ_DICT_NODE_STR;

	if(pn->str_malloc)
		_str_free(pn->value.str);
	else
		pn->str_malloc = TRUE;
	pn->value.str = _new;
	pn->value.str_len = len;
	return EZ_SUCCESS;
}

ezResult ezDictNode_set_str_p(ezDictNode *pn, const char *value, intptr_t len)
{
	if(pn == NULL) return EZ_FAIL;

	if((pn->type != EZ_DICT_NODE_NONE) &&
		(pn->type != EZ_DICT_NODE_STR))
	{
		return EZ_ECLASH;
	}

	if(value && len == -1)
		len = strlen(value);

	if(len < 0) return EZ_FAIL;

	if((value == NULL)||(len == 0))
	{
		if(pn->type == EZ_DICT_NODE_NONE)
			pn->type = EZ_DICT_NODE_STR;

		if(pn->str_malloc) {
			pn->str_malloc = FALSE;
			_str_free(pn->value.str);
		}
		pn->value.str = NULL;
		pn->value.str_len = 0;
		return EZ_SUCCESS;
	}

	if(pn->type == EZ_DICT_NODE_NONE)
		pn->type = EZ_DICT_NODE_STR;

	if(pn->str_malloc) {
		pn->str_malloc = FALSE;
		_str_free(pn->value.str);
	}

	pn->value.str = value;
	pn->value.str_len = len;
	return EZ_SUCCESS;
}

ezDictNodeTypeEnum ezDictNode_get_type(ezDictNode *pn)
{
	if(pn) return pn->type;
	return EZ_DICT_NODE_NONE;
}

ezFourCC ezDictNode_get_fourcc(ezDictNode *pn)
{
	if(pn) return pn->four;
	return 0;
}

const char * ezDictNode_get_key(ezDictNode *pn, intptr_t *rlen)
{
	return ezMapNode_get_key_string(pn, rlen);
}

intptr_t ezDictNode_get_int(ezDictNode *pn)
{
	if(pn && (pn->type==EZ_DICT_NODE_INT))
		return pn->value.intber;

	if(pn && (pn->type==EZ_DICT_NODE_FLOAT))
		return (intptr_t)(pn->value.floatber);

	return 0;
}

intptr_t ezTableNode_get_scale_int(ezDictNode *pn, intptr_t scale)
{
	if(pn && (pn->type==EZ_DICT_NODE_INT))
		return pn->value.intber * scale;

	if(pn && (pn->type==EZ_DICT_NODE_FLOAT))
		return (intptr_t)(pn->value.floatber * scale);

	return 0;
}

double ezDictNode_get_float(ezDictNode *pn)
{
	if(pn && (pn->type==EZ_DICT_NODE_FLOAT))
		return pn->value.floatber;

	if(pn && (pn->type==EZ_DICT_NODE_INT))
		return (double)(pn->value.intber);

	return 0.0;
}

const char * ezDictNode_get_str(ezDictNode *pn, intptr_t *rlen)
{
	if((pn==NULL)||(pn->type!=EZ_DICT_NODE_STR))
	{
		if(rlen) *rlen = 0;
		return NULL;
	}

	if(rlen) *rlen = pn->value.str_len;
	return pn->value.str;
}

/*takeout: 把dict从dictnode中拿出来*/
ezDict * ezDictNode_get_dict(ezDictNode *pn, ezBool takeout)
{
	if(pn && (pn->type==EZ_DICT_NODE_DICT))
	{
		if(!takeout) return pn->value.dict;
		ezDict *d = pn->value.dict;
		if(d != NULL) {
			pn->value.dict = NULL;
			ezGC_Slave2Local(d, &EZ_DICT_NODE_TYPE);
		}
		return d;
	}
	return NULL;
}

void ezDictNode_delete(ezDictNode *pn)
{
	if(pn) ezMapNode_delete(pn);
	return;
}

void ezDict_qsort(ezDict *dict, ezSortCompareFunc comp_func, void *arg)
{
	ezMap_qsort_index((ezMap *)dict, comp_func, arg);
	return;
}

ezDictNode * ezDict_get_parent_node(ezDict *dict)
{
	return ezGC_get_owner(dict, &EZ_DICT_NODE_TYPE);
}

