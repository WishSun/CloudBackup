/*************************************************************************
	> File Name: log.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月07日 星期三 10时40分45秒
 ************************************************************************/

#ifndef _LOG_H
#define _LOG_H

#include <zlog.h>

extern zlog_category_t *log_handle;

#define ALL(...) zlog_fatal(log_handle, __VA_ARGS__)
#define INF(...) zlog_info(log_handle, __VA_ARGS__)
#define WAR(...) zlog_warn(log_handle, __VA_ARGS__)
#define DBG(...) zlog_debug(log_handle, __VA_ARGS__)
#define ERR(...) zlog_error(log_handle, __VA_ARGS__)

/*以一种日志规则初始化日志, 获取日志操作句柄
 *@ conf: 日志配置文件目录
 *@ mode: 日志打印规则
 *返回值: 成功返回0
 *        失败返回-1
 */
int open_log(char *conf, char *mode);

/*关闭日志*/
void close_log();

#endif
