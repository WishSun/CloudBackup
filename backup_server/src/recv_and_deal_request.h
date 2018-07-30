/*************************************************************************
	> File Name: recv_and_deal_request.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月11日 星期日 10时12分47秒
 ************************************************************************/

#ifndef _RECV_AND_DEAL_REQUEST_H
#define _RECV_AND_DEAL_REQUEST_H
#include "../../common_include/common.h"

/*接收并处理中转服务器或客户端的请求线程入口*/
int thread_recv_and_deal_request(backup_sev_ctrl_t *p_sev_ctrl);
#endif
