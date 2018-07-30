/*************************************************************************
	> File Name: myList.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月06日 星期二 21时03分36秒
 ************************************************************************/

#ifndef _MYLIST_H
#define _MYLIST_H

typedef struct _list_node_t
{
    char *data;             //存放数据结点的地址
    struct _list_node_t *next;   //next指针
}list_node_t;

typedef struct _list_t
{
    list_node_t *head;   /*链表头结点指针*/
    list_node_t *tail;   /*链表尾结点指针*/
}list_t;


/*链表初始化 */
void list_init(list_t *p_list);

/*尾插一个结点数据 */
void list_tail_add(list_t *p_list, void *data);

/*头插一个结点数据 */
void list_head_add(list_t *p_list, void *data);

/*删除一个结点数据, 并返回它的下一个结点指针 */
list_node_t* list_del(list_t *p_list, void *data);

/*获取链表中结点的个数*/
int list_count(list_t *p_list);

/*销毁链表 */
void list_destroy(list_t *p_list);

#endif
