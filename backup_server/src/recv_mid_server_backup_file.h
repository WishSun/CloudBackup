/*************************************************************************
	> File Name: recv_mid_server_backup_file.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月11日 星期日 10时06分10秒
 ************************************************************************/

#ifndef _RECV_MID_SERVER_BACKUP_FILE_H
#define _RECV_MID_SERVER_BACKUP_FILE_H

#include "../../common_include/common.h"

/*从中转服务器接收备份文件线程入口函数*/
int thread_recv_mid_server_backup_file(backup_sev_ctrl_t *p_sev_ctrl);
#endif
