/*************************************************************************
	> File Name: mid_server.h
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月06日 星期二 22时06分37秒
 ************************************************************************/

#ifndef _MID_SERVER_H
#define _MID_SERVER_H

#include <stdint.h>
#include <sys/stat.h>
#include "./log.h"
#include "./myList.h"
#include "./queue.h"
#include "./condition.h"

#define TRUE  1
#define FALSE 0

/*最大监听数*/
#define MAX_LISTEN 1024

/*网络传输缓冲区大小*/
#define SEND_OR_RECV_BUFF 4096

/*用户名所占字节数*/
#define USERNAME_BYTES     32

/*路径所占字节数*/
#define FILEPATH_BYTES     1024

/*文件名所占字节数(不包括路径)*/
#define FILENAME_BYTES     256

/*文件权限所占字节数*/
#define FILE_MODE_BYTES    4

/*发送长度所占字节数*/
#define SEND_LEN_BYTES     4

/*备份标志所占字节数*/
#define BACKUP_FLAG_BYTES  1

/*IP地址所占字节数*/
#define IP_ADDR_BYTES      16

/*文件发送完毕*/
#define FILE_FINISH        0x01

/*文件刚刚开始发送*/
#define FILE_FIRST_COME    0x02

/*MD5码的字节数*/
#define MD5_BYTES          32

/*服务器地址信息*/
typedef struct
{
    char mid_sev_IP[IP_ADDR_BYTES];   /*中转服务器IP*/
    char backup_sev_IP[IP_ADDR_BYTES];/*备份服务器IP*/

    int mid_sev_port;        /*中转服务器端口*/
    int backup_sev_port;     /*备份服务器端口*/
}sev_info_t;

/*客户端结构*/
typedef struct 
{
    int   mid_server_fd;     /*与中转服务器通信套接字*/
    int   backup_server_fd;  /*与备份服务器通信套接字*/
    que_t trans_que;         /*备份文件上传队列*/

    condition_t ready_upload_file;  /*是否可以上传文件*/
    condition_t ready_scan_dir;     /*是否可以监控目录*/
}user_t;


/*中转服务器服务控制结构*/
typedef struct 
{
    char cloud_conf_path[FILEPATH_BYTES];  /*服务器配置文件路径*/
    char sev_addr[IP_ADDR_BYTES];          /*中转服务器地址*/
    int  sev_port;              /*中转服务器端口*/
    char backup_sev_addr[IP_ADDR_BYTES];   /*备份服务器地址*/
    int  backup_sev_port;       /*备份服务器端口*/

    char log_conf_path[FILEPATH_BYTES];    /*日志配置文件路径*/
    char log_file_path[FILEPATH_BYTES];    /*日志文件路径*/

    int  backup_server_fd;      /*与备份服务器通信套接字*/

    int  client_login_epfd;     /*监听客户端登录epoll集合*/
    int  client_request_epfd;   /*监听客户端文件上传请求epoll集合*/

    list_t no_login;           /*未登录链表*/
    list_t yes_login;          /*已登录链表(也是接收备份文件上传链表)*/
    list_t backup_list;        /*备份链表*/
}mid_sev_ctrl_t;

/*备份服务器服务控制结构*/
typedef struct 
{
    char cloud_conf_path[FILEPATH_BYTES];  /*服务器配置文件路径*/
    char sev_addr[IP_ADDR_BYTES];          /*服务器地址*/
    int  sev_port;              /*服务器端口*/
    char log_conf_path[FILEPATH_BYTES];    /*日志配置文件路径*/
    char log_file_path[FILEPATH_BYTES];    /*日志文件路径*/

    int  mid_server_fd;         /*与中转服务器通信套接字*/

    int  all_connect_epfd;      /*监听所有连接(包括中转服务器连接和客户端连接)*/
    int  client_request_epfd;           /*监听客户端事件epoll集合(请求恢复事件)*/

    list_t user_list;           /*已连接用户链表(用来向用户发送备份文件)*/
}backup_sev_ctrl_t;

/*中转服务器客户端信息结构*/
typedef struct 
{
    char     cli_username[USERNAME_BYTES];     /*客户端登录用户名*/
    char     cli_user_password[MD5_BYTES];     /*客户端登录密码*/
    char     cli_ip[IP_ADDR_BYTES];           /*客户端IP*/
    int      cli_port;             /*客户端端口*/
    int      cli_socket_fd;        /*与客户端通信套接字描述符*/
    uint64_t cli_connect_time;     /*客户端连接时间*/

    int      cli_upload_file_fd;   /*正在上传的文件描述符*/
    uint64_t cli_upload_bytes;     /*已上传字节数*/
    uint64_t cli_upload_file_size; /*正在上传的文件大小*/

    int      cli_backup_file_fd;   /*正在备份的文件描述符*/
    uint64_t cli_backup_bytes;     /*已备份字节数*/
    uint64_t cli_backup_file_size; /*正在备份的文件大小*/


    que_t   backup_file_que;      /*备份文件上传至备份服务器队列*/
    que_t   upload_file_que;      /*客户端将备份文件上传至中转服务器队列*/
}mid_sev_client_user_info_t;


/*备份服务器客户端信息结构*/
typedef struct 
{
    char     cli_username[USERNAME_BYTES];     /*客户端用户名*/
    char     cli_ip[IP_ADDR_BYTES];           /*客户端IP*/
    int      cli_port;             /*客户端端口*/
    int      cli_socket_fd;        /*与客户端通信套接字描述符*/
    uint64_t cli_connect_time;     /*客户端连接时间*/

    int      cli_restore_file_fd;  /*正在恢复的文件描述符*/
    uint64_t cli_restore_bytes;    /*已恢复字节数*/
    uint64_t cli_restore_file_size;/*正在恢复的文件大小*/

    int     restore_flag;          /*是否恢复文件标记*/
    que_t   restore_file_que;      /*恢复备份文件至客户端用户队列*/
}backup_sev_client_user_info_t;


/*客户端请求类型*/
enum request_t
{
    REQ_UPLOAD = 10, /*请求上传文件*/
    RSP_UPLOAD,     /*对上传文件请求的回复*/

    REQ_LOGIN,      /*请求登录*/
    RSP_LOGIN_SUCCESS,   /*对登录请求的回复(登录成功)*/
    RSP_LOGIN_FAIL,      /*对登录请求的回复(登录失败)*/

    RSP_FINISH,     /*文件接收完毕*/
    RSP_BAD_FILE,   /*文件数据错误*/

    REQ_RESTORE,    /*文件恢复请求*/
    RSP_RESTORE,    /*对文件恢复请求的回复*/
};

/*公共请求头结构*/
typedef struct __req_head_t
{
    uint8_t     type;   /*此次请求的类型*/
    uint64_t    length; /*此次请求携带的数据的长度*/
}req_head_t;

/*文件头结构*/
typedef struct __file_head_t
{
    char        filename[FILENAME_BYTES];  /*文件名(不包含路径)*/
    mode_t      file_mode;      /*文件权限*/
    uint64_t    file_size;      /*文件长度*/
}file_head_t;

/*用户登录信息结构*/
typedef struct __login_info_t
{
    char  username[USERNAME_BYTES];
    char  password[MD5_BYTES];
}login_info_t;

/*文件结点*/
typedef struct __file_node_t
{
    char     filename[FILEPATH_BYTES];  /*文件名(包含路径)*/
    int      occupy_flag;     /*文件是否被占用标志*/
    mode_t   file_mode;       /*文件权限*/
    uint64_t file_size;       /*文件大小*/
}file_node_t;


/*对发送数据或向文件中写入数据进行封装
 * @fd : 套接字描述符或文件描述符 
 * @buff: 将要写入的数据的地址
 * @len: 将要写入的数据的长度
 *
 * 返回值: 成功返回0
 *         失败返回-1
 */
int send_data(int fd, void *buff, int len);

/*对接收数据或从文件中读取数据进行封装
 * @fd : 套接字描述符或文件描述符 
 * @buff: 将要写入的数据的地址
 * @len: 将要写入的数据的长度
 *
 * 返回值: 成功返回0
 *         失败返回-1
 */
int recv_data(int fd, void *buff, int len);

/*设置描述符为非阻塞*/
void setnonblock(int fd);

/*创建套接字并绑定IP地址和端口*/
int create_listen_socket(char *p_sev_addr, int sev_port);

/*与服务器建立连接*/
int connect_server(char *p_sev_addr, int sev_port);
#endif
