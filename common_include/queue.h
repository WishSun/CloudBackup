/*************************************************************************
	> File Name: queue.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月03日 星期六 17时46分31秒
 ************************************************************************/

#ifndef _QUEUE_H
#define _QUEUE_H

#include <stdint.h>

/*结点类型*/
typedef struct _node_t
{
    char    *data;
    struct _node_t  *next;
}node_t;

/*队列类型*/
typedef struct __que_t
{
    node_t   *head;     /*队头指针*/
    node_t   *tail;     /*队尾指针*/
}que_t;

#define TRUE 1
#define FALSE 0

/*初始化队列*/
void que_init(que_t *p_queue);

/*判断队列是否能够出队(为空不能出队)*/
int can_pop(que_t *p_queue);

/*判断队列是否能够入队(队列为空才能入队)*/
int can_push(que_t *p_queue);

/*获取队头元素指针*/
char* get_queue_head(que_t *p_queue);

/*获取队列结点个数*/
int get_queue_node_number(que_t *p_queue);

/*入队*/
void que_push(que_t *p_queue, char *data);

/*出队, 返回出队的结点指针(结点未释放)*/
char* que_pop(que_t *p_queue);
#endif
