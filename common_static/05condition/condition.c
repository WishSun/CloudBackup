/*************************************************************************
	> File Name: condition.c
	> Author: 
	> Mail: 
	> Created Time: 2018年01月27日 星期六 10时35分53秒
 ************************************************************************/

#include "../../common_include/condition.h"

//同步、互斥量的初始化
void condition_init(condition_t *cond)
{
    pthread_mutex_init(&cond->mutex, NULL);
    pthread_cond_init(&cond->cond, NULL);
}

//条件变量阻塞等待
int  condition_wait(condition_t *cond)
{
    return pthread_cond_wait(&cond->cond, &cond->mutex);
}

//给一个等待中的条件变量发送唤醒信号
void condition_signal(condition_t *cond)
{
    pthread_cond_signal(&cond->cond);
}

//销毁互斥量和条件变量
void condition_destroy(condition_t *cond)
{
    pthread_mutex_destroy(&cond->mutex);
    pthread_cond_destroy(&cond->cond);
}
