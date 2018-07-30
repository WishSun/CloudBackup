/*************************************************************************
	> File Name: send_file.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月07日 星期三 11时19分41秒
 ************************************************************************/

#ifndef _SEND_FILE_H
#define _SEND_FILE_H

#include "../../common_include/common.h"

/*发送文件到备份服务器线程入口*/
int thread_send_file_to_backup_server(mid_sev_ctrl_t *p_sev_ctrl);
#endif
