#ifndef _EZ_LIST_H
#define _EZ_LIST_H

/**************************************************
 * Simple doubly linked list implementation.
 * 简单双向循环链表实现,
 * 思路来源于 linux-2.4.32/include/linux/list.h
 * 根据算法原理, ezList结构所在的内存不能 realloc
 **************************************************/

struct list_head {
	struct list_head *next, *prev;
};

typedef struct list_head ezList;
//EZ_CONTAINER_OF(p, type, member)

static inline void ezList_init(ezList *plist)
{
	plist->next = NULL;
	plist->prev = NULL;
}

static inline void ezList_init_head(ezList *head)
{
	head->next = head;
	head->prev = head;
}

static inline ezBool ezList_is_empty(ezList *head)
{
	return head->next == head;
}

static inline void ezList_add(ezList *plist,
					ezList *prev,
					ezList *next)
{
	if(plist->next) ezLog_err("add error list\n");
	next->prev = plist;
	plist->next = next;
	plist->prev = prev;
	prev->next = plist;
}

static inline void ezList_add_front(ezList *plist, ezList *head)
{
	ezList_add(plist, head, head->next);
}

static inline void ezList_add_tail(ezList *plist, ezList *head)
{
	ezList_add(plist, head->prev, head);
}

static inline void __list_del(ezList *prev, ezList *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void ezList_del(ezList *plist)
{
	if(plist->next) {
		__list_del(plist->prev, plist->next);
		plist->next = NULL;
		plist->prev = NULL;
	}
}

static inline void __list_add(ezList *plist,
					ezList *prev,
					ezList *next)
{
	next->prev = plist;
	plist->next = next;
	plist->prev = prev;
	prev->next = plist;
}

static inline void ezList_move(ezList *plist, ezList *head)
{
	if(plist->next) __list_del(plist->prev, plist->next);
	__list_add(plist, head, head->next);
}

static inline void ezList_move_tail(ezList *plist, ezList *head)
{
	if(plist->next) __list_del(plist->prev, plist->next);
	__list_add(plist, head->prev, head);
}

static inline void __list_splice(ezList *head, ezList *prev, ezList *next)
{
	ezList *first = head->next;
	ezList *last = head->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * list_splice - join two lists
 * @head_src: the new list to add.
 * @head_dst: the place to add it in the first list.
 */
static inline void ezList_splice(ezList *head, ezList *dst_head)
{
	if (!ezList_is_empty(head)) {
		__list_splice(head, dst_head, dst_head->next);
		ezList_init_head(head);
	}
}

/*============================================================================*/

/**
 * list_for_each	-	iterate over a list
 * @plist:	the &ezList to use as a loop counter.
 * @head:	the head for your list.
 */
#define ezList_for_each(plist, head) \
	for(plist = (head)->next, EZ_PREFETCH(plist->next); \
		plist != (head); \
		plist = plist->next, EZ_PREFETCH(plist->next))

#define ezList_for_each_prev(plist, head) \
	for(plist = (head)->prev, EZ_PREFETCH(plist->prev); \
		plist != (head); \
		plist = plist->prev, EZ_PREFETCH(plist->prev))

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @plist:	the &ezList to use as a loop counter.
 * @tmp:	another &ezList to use as temporary storage
 * @head:	the head for your list.
 */
#define ezList_for_each_safe(plist, tmp, head) \
	for(plist = (head)->next, tmp = plist->next; \
		plist != (head); \
		plist = tmp, tmp = tmp->next)

#define ezList_for_each_prev_safe(plist, tmp, head) \
	for(plist = (head)->prev, tmp = plist->prev; \
		plist != (head); \
		plist = tmp, tmp = tmp->prev)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @entry:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the type struct.
 */
#define ezList_for_each_entry(entry, head, member) \
	for(entry = EZ_FROM_MEMBER(typeof(*entry), member, (head)->next), \
		EZ_PREFETCH(entry->member.next); \
		&(entry->member) != (head); \
		entry = EZ_FROM_MEMBER(typeof(*entry), member, entry->member.next), \
		EZ_PREFETCH(entry->member.next))

#define ezList_for_each_prev_entry(entry, head, member) \
	for(entry = EZ_FROM_MEMBER(typeof(*entry), member, (head)->prev), \
		EZ_PREFETCH(entry->member.prev); \
		&(entry->member) != (head); \
		entry = EZ_FROM_MEMBER(typeof(*entry), member, entry->member.prev), \
		EZ_PREFETCH(entry->member.prev))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @entry:	the type * to use as a loop counter.
 * @tmp:	another &ezList to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the type struct.
 */
#define ezList_for_each_entry_safe(entry, tmp, head, member) \
	for(entry = EZ_FROM_MEMBER(typeof(*entry), member, (head)->next), \
		tmp = entry->member.next; \
		&(entry->member) != (head); \
		entry = EZ_FROM_MEMBER(typeof(*entry), member, tmp), tmp = tmp->next)

#define ezList_for_each_prev_entry_safe(entry, tmp, head, member) \
	for(entry = EZ_FROM_MEMBER(typeof(*entry), member, (head)->prev), \
		tmp = entry->member.prev; \
		&(entry->member) != (head); \
		entry = EZ_FROM_MEMBER(typeof(*entry), member, tmp), tmp = tmp->prev)

#endif

