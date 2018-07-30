/*************************************************************************
	> File Name: log.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月07日 星期三 10时42分49秒
 ************************************************************************/

#include <stdio.h>
#include "../../common_include/log.h"

zlog_category_t *log_handle = NULL;

/*以一种日志规则初始化日志, 获取日志操作句柄
 *@ conf: 日志配置文件目录
 *@ mode: 日志打印规则
 *返回值: 成功返回0
 *        失败返回-1
 */
int open_log(char *conf, char *mode)
{
    if (!conf) {
        fprintf(stdout, "log conf file is null\n");
        return -1;
    }
    //zlog初始化
    if (zlog_init(conf)) {
        fprintf(stdout, "zlog_init error\n");
        return -1;
    }
    //获取日志操作句柄
    if ((log_handle = zlog_get_category(mode)) == NULL) {
        fprintf(stdout, "zlog_get_category error\n");
        return -1;
    }
    return 0;
}

/*关闭日志*/
void close_log()
{
    zlog_fini();
}

