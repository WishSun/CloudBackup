/*************************************************************************
	> File Name: condition.h
	> Author: 
	> Mail: 
	> Created Time: 2018年01月27日 星期六 10时24分23秒
 ************************************************************************/

#ifndef _CONDITION_H
#define _CONDITION_H
#include <pthread.h>

//封装互斥量和条件变量为一个condition_t
typedef struct 
{
    pthread_mutex_t  mutex;    //互斥量
    pthread_cond_t   cond;     //条件变量
}condition_t;

//同步、互斥量的初始化
void condition_init(condition_t *cond);


//条件变量阻塞等待
int  condition_wait(condition_t *cond);

//给一个等待中的条件变量发送唤醒信号
void condition_signal(condition_t *cond);

//销毁互斥量和条件变量
void condition_destroy(condition_t *cond);
#endif
