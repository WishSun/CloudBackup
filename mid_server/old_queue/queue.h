/*************************************************************************
	> File Name: queue.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月07日 星期三 10时20分22秒
 ************************************************************************/

#ifndef _QUEUE_H
#define _QUEUE_H

//队列控制结构
typedef struct __que_t
{
    int     read;  //队列的首结点下标
    int     write; //队列的尾结点下标
    int     block_num;  //结点个数
    char    **block;  //数据区(一块连续的内存区域，用来存放所有队列数据结点的地址)
}que_t;

//初始化一个队列, 队列头指针为head(因为是宏定义，所以只传一级指针即可，如果是函数调用的话，需要传二级指针), T为队列数据区类型, node_count为队列结点的个数, 实际申请结点个数比请求结点个数多1，是为了循环队列计算队列满和队列空方便
//block成员存放所有结点指针
#define ring_init(head, T, node_count) \
    do{ \
        (head) = (que_t*)malloc(sizeof(que_t)); \
        (head)->block_num = node_count + 1; \
        (head)->read = (head)->write = 0; \
        (head)->block = (char**)malloc(sizeof(T *) * (node_count + 1)); \
    }while(0)


//如果head队列不为空，返回真，为空返回假
#define can_read(head) ((head)->read != (head)->write)

//如果head队列已满，返回假, 不为满则返回真
#define can_write(head) ((head)->read != (((head)->write + 1) % (head)->block_num))

//返回head队列队头结点的指针
//#define get_read(head, T) ((T*)(((head)->block)[(head)->read]))
#define get_read(head, T) ((T*)(((head)->block)[(head)->read]))


//返回head队列可插入结点的地址
#define get_write(head, T) (((head)->block)[(head)->write])


//如果能够从head队列的队头读取一个结点，则将队头结点读入V中，并返回1
//如果不能从head队列的队头读取一个结点，则返回0
#define read_push(head, T, V) (can_read(head) ? ((V) = get_read(head, T), 1) : 0)


//修正head队列队头结点下标值
#define read_pop(head) ((head)->read = ((head)->read + 1) % (head)->block_num)


//如果head队列未满可以插入结点，则将V插入head队列中，并返回1
//如果head队列已满，则返回0
#define write_push(head, T, V) (can_write(head) ? (get_write(head, T) = (V), 1) : 0)


//修正head队列队尾结点下标值
#define write_pop(head) ((head)->write = (((head)->write + 1) % (head)->block_num))


//如果head队列不为空，则用V接收队列的队头结点地址，并修正head队列的队头结点下标值，最后返回1
//如果head队列为空，则返回0
#define read_outval(head, T, V) (can_read(head) ? ((V) = get_read(head, T), read_pop(head), 1):0)


//如果head队列未满(即可插入新结点),则获取插入位置指针，并给其赋值为新结点V的地址，然后修正head队列的队尾下标值，最后返回1
//如果head队列已满(即不可插入新结点),则返回0
#define write_inval(head, T, V) (can_write(head) ? (get_write(head, T) = (char*)(V), write_pop(head), 1):0)


#endif
