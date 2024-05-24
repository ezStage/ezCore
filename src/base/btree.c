#include <ezCore/ezCore.h>

/*left_c是root的左孩子, 右旋, left_c成为新根*/
#define _R_Rotate(root, left_c) \
do {\
	root->left = left_c->right; \
	left_c->right = root; \
} while(0)

/*left_c是parent的左孩子, right_c是left_c的右孩子,
先left_c/right_c左旋, 再右旋, right_c成为新根*/
#define _LR_Rotate(root, left_c, right_c) \
do {\
	root->left = right_c->right; \
	left_c->right = right_c->left; \
	right_c->right = root; \
	right_c->left = left_c; \
} while(0)
/*其实_LR_Rotate(root, left_c, right_c)等价于
	_L_Rotate(left_c, right_c)
	_R_Rotate(root, right_c)
*/

/*right_c是root的右孩子, 左旋, right_c成为新根*/
#define _L_Rotate(root, right_c) \
do {\
	root->right = right_c->left; \
	right_c->left = root; \
} while(0)

/*right_c是root的右孩子, left_c是right_c的左孩子,
先right_c/left_c右旋, 再左旋, left_c成为新根*/
#define _RL_Rotate(root, right_c, left_c) \
do {\
	root->right = left_c->left; \
	right_c->left = left_c->right; \
	left_c->left = root; \
	left_c->right = right_c; \
} while(0)
/*其实_RL_Rotate(root, right_c, left_c)等价于
	_R_Rotate(right_c, left_c)
	_L_Rotate(root, left_c)
*/

/*proot的子树都是平衡的，proot的balance需要矫正，
rotate之后，返回新的root. 注意这里有一个规律：
如果新root的balance==0就表示树的整体高度减少了1, 否则高度不变!*/
static inline ezBTree * _adjust_balance_ (ezBTree *proot)
{
	ezBTree *p1, *p2;
	if(proot->balance > 1)
	{   /*即proot->balance == 2, 左树高*/
		p1 = proot->left;
		switch(p1->balance) {
		case 1:
			_R_Rotate(proot, p1);
			proot->balance = 0;
			p1->balance = 0;
			return p1;
		case 0:
			_R_Rotate(proot, p1);
			proot->balance = 1;
			p1->balance = -1;
			return p1;
		case -1:
			p2 = p1->right;
			_LR_Rotate(proot, p1, p2);
			proot->balance = (p2->balance <= 0)?0:-1;
			p1->balance = (p2->balance >= 0)?0:1;
			p2->balance = 0;
			return p2;
		}
	}
	else if(proot->balance < -1)
	{   /*即proot->balance == -2, 右树高*/
		p1 = proot->right;
		switch(p1->balance) {
			case -1:
			_L_Rotate(proot, p1);
			proot->balance = 0;
			p1->balance = 0;
			return p1;
		case 0:
			_L_Rotate(proot, p1);
			proot->balance = -1;
			p1->balance = 1;
			return p1;
		case 1:
			p2 = p1->left;
			_RL_Rotate(proot, p1, p2);
			proot->balance = (p2->balance >= 0)?0:1;
			p1->balance = (p2->balance <= 0)?0:-1;
			p2->balance = 0;
			return p2;
		}
	}
	return proot;
}

ezBool ezBTree_insert(ezBTree **pproot, ezBTree *pnode)
{
	int i, is_left;
	ezBTree *stack[60];
	ezBTree *proot, *parent;

	if((pproot == NULL)||(pnode == NULL)) return FALSE;

	if((proot = *pproot) == NULL) {
		*pproot = pnode;
		return TRUE;
	}

	stack[i=0] = NULL;
	while(1)
	{
		if(pnode->key < proot->key)
		{
			if(!proot->left) {
				proot->left = pnode;
				proot->balance += 1;
				break;
			}
			stack[++i] = proot;
			proot = proot->left;
		}
		else if(pnode->key > proot->key)
		{
			if(!proot->right) {
				proot->right = pnode;
				proot->balance += -1;
				break;
			}
			stack[++i] = proot;
			proot = proot->right;
		}
		else /* pnode->key == proot->key */
		{
			if(pnode != proot) {
				ezLog_err("key repeat\n");
				return FALSE;
			}
			return TRUE;
		}
	}
	/*这时proot就是pnode的父节点, 肯定是平衡的*/
	/*这时proot->balance表示proot树增加的高度:1:left+1, -1:right+1, 0:不变*/
	while(1)
	{
		parent = stack[i--]; /*parent是proot的父节点*/
		is_left = (parent && (parent->left == proot));
		if((proot->balance > 1)||(proot->balance < -1))
		{
			proot = _adjust_balance_(proot);
			if(parent == NULL)
				*pproot = proot;
			else if(is_left)
				parent->left = proot;
			else
				parent->right = proot;
		}
		/*第一次循环时, proot肯定是平衡的, 并且proot->balance等于0表示
		增加了pnode节点后proot树的高度也是不变的.*/
		/*后面循环时, 如果proot是平衡的, 因为之前是-1,0,1, 加上+/-1
		后还是-1,0,1, 可推理知现在的balance为0表示树的高度没变, 
		否则只能是之前的balance为0, 并且树的高度增加了1
		后面循环时, 如果proot不是平衡的, _adjust_balance_()返回的
		新proot的balance为0正好也表示树的高度减少了1*/
		if((proot->balance == 0)||(parent == NULL)) break;

		parent->balance += is_left?1:-1;
		proot = parent;
	}
	return TRUE;
}

ezBool ezBTree_delete(ezBTree **pproot, ezBTree *pnode)
{
	int i, is_left;
	ezBTree *stack[60];
	ezBTree *proot, *parent;
	ezBTree *prev;

	if((pproot == NULL)||(pnode == NULL)) return FALSE;

	if((proot = *pproot) == NULL) {
		ezLog_err("not on tree");
		return FALSE;
	}

	prev = NULL;
	stack[i=0] = NULL;
	while (1)
	{
		if(pnode->key < proot->key)
		{
			if(!proot->left) {
				ezLog_err("not on tree");
				return FALSE;
			}
			stack[++i] = proot;
			proot = proot->left;
		}
		else if(pnode->key > proot->key)
		{
			if(!proot->right) {
				ezLog_err("not on tree");
				return FALSE;
			}
			stack[++i] = proot;
			proot = proot->right;
		}
		else /* pnode->key == proot->key */
		{
			if(pnode != proot) {
				ezLog_err("not on tree");
				return FALSE;
			}
			if((proot->left == NULL) || (proot->right == NULL))
				break;

			/*交换node和它的直接前驱*/
			prev = proot->left;
			if(prev->right == NULL) {
				EZ_SWAP(*prev, *pnode); //id也需要swap;
				prev->left = pnode;
			} else {
				ezBTree *p2;
				do {p2=prev; prev=prev->right;} while(prev->right);
				EZ_SWAP(*prev, *pnode); //id也需要swap;
				p2->right = pnode;
			}

			parent = stack[i];
			if(parent == NULL)
				*pproot = prev;
			else if(pnode == parent->left)
				parent->left = prev;
			else
				parent->right = prev;

			stack[++i] = prev;
			proot = prev->left;
		}
	}
	/*这时proot等于pnode, pnode没有left或right*/
	proot = pnode->left;
	if(proot == NULL) proot = pnode->right;
	/*proot这里记录pnode下面子树的根*/
	/*pnode可以删除了*/
	if(prev) EZ_SWAP(prev->key, pnode->key); /*id要swap回来*/
	pnode->left = NULL;
	pnode->right = NULL;
	pnode->balance = 0;

	parent = stack[i--];
	is_left = (parent && (parent->left == pnode));
	if(parent == NULL) {
		*pproot = proot;
		return TRUE;
	} else if(is_left) {
		parent->left = proot;
	} else {
		parent->right = proot;
	}
	parent->balance += is_left?-1:1;
	/*这时proot是pnode下面子树的根, 它肯定是平衡的,高度也没变*/
	/*parent下的某个子树高度减少1, parent现在的balance有可能不平衡,
	并且只有现在的balance等于0, parent的树高才减少1，否则树高不变*/
	proot = parent; /*从parent开始调整*/
	while(1)
	{
		parent = stack[i--];
		is_left = (parent && (parent->left == proot));
		if((proot->balance > 1)||(proot->balance < -1))
		{
			proot = _adjust_balance_(proot);
			if(parent == NULL)
				*pproot = proot;
			else if(is_left)
				parent->left = proot;
			else
				parent->right = proot;
		}
		/*进入循环时, proot下某个子树的高度减少了1,
		并且推理可知: 只有proot的balance等于0, 才表示proot树的高度也减少1.
		如果proot是平衡的, proot的balance不为0, 表示proot树的高度没变.
		如果proot是不平衡的(-2/2), proot树的高度也是没变,
		并且_adjust_balance_()返回的	新proot的balance不为0也表示proot树的高度没变!*/
		if((proot->balance != 0)||(parent == NULL)) break;
		/*下面就是proot树的高度减少了1, 调整parent的balance*/
		parent->balance += is_left?-1:1;
		proot = parent;
	}
	return TRUE;
}

/******************************************************************************/

ezBTree *ezBTree_find(ezBTree *proot, uintptr_t key)
{
	while(proot)
	{
		if(key < proot->key)
			proot = proot->left;
		else if(key > proot->key)
			proot = proot->right;
		else break;
	}
	return proot;
}

/*
pb ->   3
      2   4
    1       5
*/
void ezBTree_dump(ezBTree *proot)
{
	ezBTree *parent, *pb, *stack[64];
	int i;

	if(proot == NULL) return;

	stack[i=0] = NULL;
	pb = proot;
	while(1)
	{
		if(pb != NULL)
		{
			stack[++i] = pb;
/*=== 先序遍历pb ===*/
			if(pb->left) {
				pb = pb->left;
			} else { /*right*/
/*--- 中序遍历pb ---*/
ezLog_print("%zd\n", pb->key); //1,4,5
/*---------------*/
				pb = pb->right;
			}
		}
		else
		{
			pb = stack[i--];
/*=== 后序遍历pb ===*/
			parent = stack[i];
			if(parent == NULL) break;
			if(pb == parent->left) {
/*--- 中序遍历parent ---*/
ezLog_print("%zd\n", parent->key);//2,3
/*------------------*/
				pb = parent->right;
			} else { /*right*/
				pb = NULL;
			}
		}
	}
}

void ezBTree_dump2(ezBTree *proot)
{
	ezBTree *pb, *stack[64];
	int i=0;

	pb = proot;
	while((pb != NULL)||(i > 0))
	{
		while(pb != NULL) {
			stack[++i] = pb;
/*=== 先序遍历pb ===*/
			pb = pb->left;
		}
		pb = stack[i--];
/*=== 中序遍历pb ===*/
		pb = pb->right;
	}
}

static int _check(ezBTree *proot)
{
	int right_d, left_d;
	if(proot->left) {
		if(proot->left->key >= proot->key) ezLog_err("left key\n");
		left_d = _check(proot->left);
	} else
		left_d = 0;

	if(proot->right) {
		if(proot->right->key <= proot->key) ezLog_err("right key\n");
		right_d = _check(proot->right);
	} else
		right_d = 0;

	if(proot->balance != left_d - right_d) ezLog_err("balance\n");
	if(EZ_ABS(proot->balance) > 1) ezLog_err("balance abs\n");
	return left_d > right_d? left_d+1 : right_d+1;
}

void ezBTree_check(ezBTree *proot)
{
	if(proot) _check(proot);
}

