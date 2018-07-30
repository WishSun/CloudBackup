/*************************************************************************
	> File Name: queue.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月04日 星期日 09时22分50秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "../../common_include/queue.h"

/*初始化队列*/
void que_init(que_t *p_queue)
{
    p_queue->head = p_queue->tail = NULL;
}

/*判断队列是否能够出队(为空不能出队)*/
int can_pop(que_t *p_queue)
{
    if(p_queue->head)
    {
        return TRUE;
    }
    return FALSE;
}

/*判断队列是否能够入队(队列为空才能入队)*/
int can_push(que_t *p_queue)
{
    if(p_queue->head)
    {
        return FALSE;
    }
    return TRUE;
}

/*获取队头元素指针*/
char* get_queue_head(que_t *p_queue)
{
    return p_queue->head->data;
}

/*获取队列结点个数*/
int get_queue_node_number(que_t *p_queue)
{
    int count = 0;
    node_t *temp = p_queue->head;
    while(temp)
    {
        count++;
        temp = temp->next;
    }
    return count;
}

/*入队*/
void que_push(que_t *p_queue, char *data)
{
    if(p_queue == NULL || data == NULL)
    {
        printf("argument is error!\n");
        return;
    }

    node_t *p_new_node = malloc(sizeof(node_t));
    p_new_node->data = data;
    p_new_node->next = NULL;

    if(p_queue->head == NULL)
    {
        p_queue->head = p_queue->tail = p_new_node;
    }
    else
    {
        p_queue->tail->next = p_new_node;
        p_queue->tail = p_new_node;
    }
}

/*出队, 返回出队的结点指针(结点未释放)*/
char* que_pop(que_t *p_queue)
{
    if(p_queue == NULL)
    {
        printf("argument is error!\n");
        return NULL;
    }

    node_t *del = p_queue->head;
    p_queue->head = del->next;

    char *ret = del->data;
    free(del);
    return ret;
}

