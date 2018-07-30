/*************************************************************************
	> File Name: cloud_client.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月03日 星期六 09时45分11秒
 ************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libconfig.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "../../common_include/common.h"
#include "./scan_dir.h"
#include "./upload_backup_file.h"


/*
 * 解析配置文件获取服务端IP地址和端口
 * @sev_info: 服务端信息结构体
 * 返回值:  成功返回0
 *          失败返回-1
 */
int parse_conf(sev_info_t *p_sev_info)
{
    config_t  cfg;

    //初始化配置文件句柄
    config_init(&cfg);

    //打开配置文件，获取操作配置文件句柄
    if(!config_read_file(&cfg, "../etc/cloud.cfg"))
    {
        printf("read config file error!");
        return -1;
    }

    const char *ptr = NULL;

    //获取中转服务器IP和port
    if(!config_lookup_string(&cfg, "conf.mid_sev_IP", &ptr))
    {
        printf("get mid_sev_IP error!\n");
        return -1;
    }
    memcpy(p_sev_info->mid_sev_IP, ptr, strlen(ptr));

    if(!config_lookup_int(&cfg, "conf.mid_sev_port", &p_sev_info->mid_sev_port))
    {
        printf("get mid_sev_port error!\n");
        return -1;
    }

    //获取备份服务器IP和port
    if(!config_lookup_string(&cfg, "conf.back_sev_IP", &ptr))
    {
        printf("get back_sev_IP error!\n");
        return -1;
    }
    memcpy(p_sev_info->backup_sev_IP, ptr, strlen(ptr));

    if(!config_lookup_int(&cfg, "conf.back_sev_port", &p_sev_info->backup_sev_port))
    {
        printf("get back_sev_port error!\n");
        return -1;
    }

    config_destroy(&cfg);
    return 0;
}

/*
 * 连接中转服务器
 * @sev_info : 服务器信息(包括IP和端口)
 * 返回值:  成功返回通信套接字描述符
 *          失败返回-1
 */
int mid_sev_connect(sev_info_t *p_sev_info)
{
    return connect_server(p_sev_info->mid_sev_IP, p_sev_info->mid_sev_port);
}


/*
 * 连接备份服务器
 * @sev_info : 服务器信息(包括IP和端口)
 * 返回值:  成功返回通信套接字描述符
 *          失败返回-1
 */
int backup_sev_connect(sev_info_t *p_sev_info)
{
    return connect_server(p_sev_info->backup_sev_IP, p_sev_info->backup_sev_port);
}


/*初始化用户信息*/
void init_user(user_t *p_user)
{
    p_user->mid_server_fd = -1;

    /*初始化用户传输队列*/
    que_init(&p_user->trans_que);

    /*初始化条件变量*/
    condition_init(&p_user->ready_upload_file);
    condition_init(&p_user->ready_scan_dir);
}

/*登录中转服务器*/
int login_mid_server(user_t *p_user)
{
    /*向中转服务器发送登录请求头*/   
    req_head_t login_head;
    login_info_t login_info;

    while(1)
    {
        printf("please login server-----------------------------!\n");
        printf("please input username: ");
        scanf("%s", login_info.username);
        printf("please input password: ");
        scanf("%s", login_info.password);
        printf("username :[%s]\npassword :[%s]\n", login_info.username, login_info.password);


        /*发送登录请求头*/
        login_head.type = REQ_LOGIN;
        if(send_data(p_user->mid_server_fd, (char *)&login_head, sizeof(req_head_t)) == -1)
        {
            printf("send login mid_server request fail!\n");
            return -1;
        }

        /*发送登录用户名和密码*/
        if(send_data(p_user->mid_server_fd, &login_info, sizeof(login_info_t)) == -1)
        {
            printf("send login infomation fail!\n");
            return -1;
        }
        memset(&login_head, 0x00, sizeof(req_head_t));
        memset(&login_info, 0x00, sizeof(login_info_t));

        /*接收登录请求回复*/
        if(recv_data(p_user->mid_server_fd, (char *)&login_head, sizeof(req_head_t)) == -1)
        {
            printf("receive login mid_server response fail!\n");
            return -1;
        }
        if(login_head.type == RSP_LOGIN_SUCCESS)
        {
            break;
        }
        printf("username or password error! please login server again--------------\n");
    }
    printf("login server success! Welcome you!\n");
}

/*接收恢复文件数据*/
int recv_restore_file(user_t *p_user, file_head_t *p_file_head, int fd)
{
    char buff[SEND_OR_RECV_BUFF] = {0};
    int  surplus = p_file_head->file_size;
    int  recv_len = surplus > SEND_OR_RECV_BUFF ? SEND_OR_RECV_BUFF : surplus;
    int  ret = 0;

    while(surplus)
    {
        ret = recv_data(p_user->backup_server_fd, buff, recv_len);
        if(ret == 1)
        {
            printf("备份服务器关闭！\n");
        }
        else if(ret == -1)
        {
            return -1;
        }    

        if(send_data(fd, buff, recv_len) == -1)
        {
            return -1;
        }

        surplus -= recv_len;
        recv_len = surplus > SEND_OR_RECV_BUFF ? SEND_OR_RECV_BUFF : surplus;
    }
    return 0;
}

/*恢复备份文件到客户端本地目录*/
int restore_backup_file(user_t *p_user, sev_info_t *p_sev_info)
{
    req_head_t req_head;
    file_head_t file_head;

    /*1. 连接备份服务器*/
    if((p_user->backup_server_fd = backup_sev_connect(p_sev_info)) == -1)
    {
        printf("backup server connect fail!\n");
        return -1;
    }
    printf("backup server connect success!\n");

    #if 0
    printf("发送恢复请求\n");
    /*2. 向备份服务器发送恢复文件请求*/
    req_head.type = REQ_RESTORE;
    if(send_data(p_user->backup_server_fd, &req_head, sizeof(req_head_t)) == -1)
    {
        return -1;
    }

    printf("接收恢复请求的回复\n");
    /*2. 向备份服务器发送恢复文件请求*/
    /*3. 接收备份服务器对恢复文件请求的回复*/
    if(recv_data(p_user->backup_server_fd, &req_head, sizeof(req_head_t)) == -1)
    {
        return -1;
    }
    #endif

    /*4. 创建恢复备份文件目录*/
    if(mkdir("./restore_file_dirent", 0777) == -1)
    {
        if(errno != EEXIST)
        {
            printf("创建目录失败\n");
            return -1;
        }
    }

    int fd;
    char filename[FILENAME_BYTES];
    while(1)
    {
        /*接收文件头*/
        if(recv_data(p_user->backup_server_fd, &file_head, sizeof(file_head_t)) == -1)
        {
            return -1;
        }
        if(file_head.file_size == -1 && file_head.file_mode == -1)
        {   /*所有备份文件恢复完毕*/
            printf("接收到末尾文件\n");
            break;
        }
        /*打开文件*/
        memset(filename, 0x00, sizeof(filename));
        sprintf(filename, "./%s/%s", "restore_file_dirent", file_head.filename);
        printf("恢复文件[%s]...\n", filename);
        if((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, file_head.file_mode)) == -1)
        {
            return -1;
        }

        recv_restore_file(p_user, &file_head, fd);
        printf("备份文件[%s]接收完毕!\n", filename);
        memset(&file_head, 0x00, sizeof(file_head_t));
    }
    printf("备份文件已全部恢复完毕!\n");
    return 0;
}

int main(void)
{
    sev_info_t sev_info;
    user_t  user;

    /*初始化用户信息*/
    init_user(&user);

    //解析配置文件获取服务端IP和端口号
    if(parse_conf(&sev_info) == -1)
    {
        exit(1);
    }

    printf("服务端信息: mid_IP[%s], mid_port[%d]\n\tback_IP[%s], back_port[%d]\n", sev_info.mid_sev_IP, sev_info.mid_sev_port, sev_info.backup_sev_IP, sev_info.backup_sev_port);

    /*连接中转服务器*/
    if((user.mid_server_fd = mid_sev_connect(&sev_info)) == -1)
    {
        printf("connect mid_server fail!\n");
        exit(1);
    }
    printf("connect mid_server success!\n");

    /*登录中转服务器*/
    if(login_mid_server(&user) == -1)
    {
        printf("login mid_server fail!\n");
        exit(1);
    }

    /*启动监控备份目录线程*/
    thread_scan(&user);

    /*启动上传备份文件线程*/
    thread_upload_backup_file(&user);

    /*主线程获取恢复文件命令, 并发送给备份服务器*/
    printf("请输入命令编号: \n\t1. 恢复备份文件\t2. 退出程序\n");
    int cmd_num = -1;

    while(1)
    {
        printf("input cmd-num >> ");
        scanf("%d", &cmd_num);
        switch(cmd_num)
        {
            case 1:
            {
                restore_backup_file(&user, &sev_info);
                break;
            }
            case 2: 
            {
                printf("程序已退出！\n");
                return 0;
            }
        }
    }

    return 0;
}

