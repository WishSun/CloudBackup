/*************************************************************************
	> File Name: myList.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月06日 星期二 21时23分17秒
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../../common_include/myList.h"


/*链表初始化 */
void list_init(list_t *p_list)
{
    p_list->head = p_list->tail = NULL;
}

/*尾插一个结点数据 */
void list_tail_add(list_t *p_list, void *data)
{
    if(data == NULL || p_list == NULL)
    {
        printf("argument error!\n");
        return;
    }

    list_node_t *p_new_node = malloc(sizeof(list_node_t));
    p_new_node->data = data;
    p_new_node->next = NULL;

    if(p_list->head == p_list->tail && p_list->head == NULL)
    {
        p_list->head = p_list->tail = p_new_node;  
    }

    p_new_node->next = p_list->head;
    p_list->head = p_new_node;
}

/*头插一个结点数据 */
void list_head_add(list_t *p_list, void *data)
{
    if(data == NULL || p_list == NULL)
    {
        printf("argument error!\n");
        return;
    }

    list_node_t *p_new_node = malloc(sizeof(list_node_t));
    p_new_node->data = data;
    p_new_node->next = NULL;

    if(p_list->head == p_list->tail && p_list->head == NULL)
    {
        p_list->head = p_list->tail = p_new_node;  
    }

    p_list->tail->next = p_new_node;
    p_list->tail = p_new_node;
}

/*删除一个结点数据, 并返回它的下一个结点指针 */
list_node_t* list_del(list_t *p_list, void *data)
{
    list_node_t *pre_temp = p_list->head;   
    list_node_t *temp = p_list->head, *ret = NULL;   

    while(temp != NULL)
    {
        if(temp->data == data)
        {
            break;
        }
        pre_temp = temp;
        temp = temp->next;
    }

    if(temp != NULL)
    {
        if(temp == p_list->head) 
        {
            if(temp == p_list->tail)
            {
                p_list->head = p_list->tail = NULL;
                ret = NULL;
            }
            else
            {
                p_list->head = temp->next;
                ret = temp->next;
            }
        }
        else if(temp == p_list->tail)
        {
            p_list->tail = pre_temp;
            ret = NULL;
        }
        else
        {
            pre_temp->next = temp->next;
            ret = temp->next;
        }

        free(temp);
    }
    return ret;
}

int list_count(list_t *p_list)
{
    int count = 0;
    list_node_t *temp = p_list->head;
    while(temp != NULL)
    {
        temp = temp->next;
        count++;
    }
    return count;
}

/*销毁链表 */
void list_destroy(list_t *p_list)
{
    list_node_t *p_del = NULL;
    while(p_list->head)
    {
        p_del = p_list->head;
        p_list->head = p_del->next;
        free(p_del);
        p_del = NULL;
    }
    p_list->tail = NULL;
}


