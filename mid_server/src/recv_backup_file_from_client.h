/*************************************************************************
	> File Name: recv_backup_file_from_client.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月07日 星期三 11时37分06秒
 ************************************************************************/

#ifndef _RECV_BACKUP_FILE_FROM_CLIENT_H
#define _RECV_BACKUP_FILE_FROM_CLIENT_H

#include "../../common_include/common.h"

/*接收客户端备份文件上传线程入口*/
int thread_recv_backup_file_from_client(mid_sev_ctrl_t *p_sev_ctrl);
#endif
