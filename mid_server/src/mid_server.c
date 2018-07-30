/*************************************************************************
	> File Name: mid_server.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月06日 星期二 20时41分12秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libconfig.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "../../common_include/common.h"
#include "./send_file_to_backup_server.h"
#include "./recv_backup_file_from_client.h"

/*解析程序参数
 *@argc: 程序参数个数
 *@argv: 程序的参数字符串数组
 *@daemon_flag: 是否进入守护进程标志
 *返回值: 无
 */
void parse_argv(int argc, char *argv[], int *daemon_flag)
{
    char *ptr = "d";
    char ch;

    while((ch = getopt(argc, argv, ptr)) != -1)
    {
        switch(ch)
        {
            case 'd': 
                *daemon_flag = 0;  /*设置为0，即不进入守护进程*/
                break;
            default:
                printf("usage: \n\t-d\t\tinto debug mode!\n");
                break;
        }
    }
}

/*忽略SIGPIPE信号
 *参数: 无
 *返回值: 无
 */
void init_signal()
{
    signal(SIGPIPE, SIG_IGN);
}

/*解析配置文件
 * @p_sev_ctrl : 存放着配置文件路径信息
 */
int parse_conf(mid_sev_ctrl_t *p_sev_ctrl)
{
    config_t cfg;
    
    if(p_sev_ctrl->cloud_conf_path == NULL)
    {
        printf("config path is null!\n");
        return -1;
    }

    config_init(&cfg);

    if(!config_read_file(&cfg, p_sev_ctrl->cloud_conf_path))
    {
        printf("read config file error!\n");
        return -1;
    }
    const char *ptr = NULL;

    /*获取日志配置文件路径*/
    if(!config_lookup_string(&cfg, "conf.log_conf_path", &ptr))
    {
        printf("get log file path error!\n");
        return -1;
    }
    memcpy(p_sev_ctrl->log_conf_path, ptr, strlen(ptr));

    /*获取中转服务端IP地址*/
    if(!config_lookup_string(&cfg, "conf.mid_sev_addr", &ptr))
    {
        printf("get mid_sev_addr error!\n");
        return -1;
    }
    memcpy(p_sev_ctrl->sev_addr, ptr, strlen(ptr));

    /*获取中转服务端端口*/
    if(!config_lookup_int(&cfg, "conf.mid_sev_port", &p_sev_ctrl->sev_port))
    {
        printf("get mid_sev_port error!\n");
        return -1;
    }

    /*获取备份服务端IP地址*/
    if(!config_lookup_string(&cfg, "conf.backup_sev_addr", &ptr))
    {
        printf("get backup_sev_addr error!\n");
        return -1;
    }
    memcpy(p_sev_ctrl->backup_sev_addr, ptr, strlen(ptr));

    /*获取备份服务端端口*/
    if(!config_lookup_int(&cfg, "conf.backup_sev_port", &p_sev_ctrl->backup_sev_port))
    {
        printf("get backup_sev_port error!\n");
        return -1;
    }


    /*释放资源*/
    config_destroy(&cfg);
    return 0;
}


/*检查登录
 */
int check_login(int sockfd)
{
    return 1;
}


/*处理客户端登录*/
int deal_client_login(mid_sev_client_user_info_t *p_user_info, mid_sev_ctrl_t *p_sev_ctrl)
{
    /*接收登录信息*/
    printf("用户WishSun登录成功\n");

    struct epoll_event ev;
    /*将用户从未登录链表中删除*/
    list_del(&p_sev_ctrl->no_login, p_user_info);

    /*将用户从监听登录epoll集合中删除*/
    epoll_ctl(p_sev_ctrl->client_login_epfd, EPOLL_CTL_DEL, p_user_info->cli_socket_fd, &ev);

    ev.data.ptr = p_user_info;
    ev.events = EPOLLIN;
    /*将用户套接字添加到监听文件上传请求epoll集合中*/
    epoll_ctl(p_sev_ctrl->client_request_epfd, EPOLL_CTL_ADD, p_user_info->cli_socket_fd, &ev);

    /*将用户添加到已登录链表*/
    list_head_add(&p_sev_ctrl->yes_login, p_user_info);



    /*向客户端发送登录回复*/
    req_head_t rsp_login;
    rsp_login.type = RSP_LOGIN_SUCCESS;

    if(send_data(p_user_info->cli_socket_fd, (char *)&rsp_login, sizeof(req_head_t)) == -1)
    {
        DBG("send login response error!\n");
        return -1;
    }

    /*验证客户端用户登录，验证成功后为用户创建上传目录*/
    sprintf(p_user_info->cli_username, "%s", "WishSun");
    if(mkdir(p_user_info->cli_username, 0777) == -1)
    {
        if(errno != EEXIST)
        {
            DBG("create user upload dirent error: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

/*主线程监听客户端的连接请求及登录请求*/
int listen_client_connect_and_login(mid_sev_ctrl_t *p_sev_ctrl)
{
    int lst_fd, len, cli_fd;
    int nfds;
    struct sockaddr_in cli_addr;

    struct epoll_event evs[MAX_LISTEN], ev;

    /*创建监听套接字并开始监听*/
    if((lst_fd = create_listen_socket(p_sev_ctrl->sev_addr, p_sev_ctrl->sev_port)) == -1)
    {
        ERR("create listen sock_fd error\n");
        return -1;
    }

    /*将监听套接字描述符添加到epoll监听列表*/
    ev.data.fd = lst_fd;
    ev.events = EPOLLIN;
    epoll_ctl(p_sev_ctrl->client_login_epfd, EPOLL_CTL_ADD, lst_fd, &ev);


    mid_sev_client_user_info_t *p_user_info = NULL;

    list_node_t *p_head;
    uint64_t this_time;
    while(1)
    {
        /*设置3秒钟超时，即3秒钟检测一次客户端链表, 未登录链表，超时时间为5分钟
         *超过5分钟未登录成功，则干掉该客户端*/
        nfds = epoll_wait(p_sev_ctrl->client_login_epfd, evs, MAX_LISTEN, 3000);
        if(nfds < 0)
        {
            DBG("epoll_wait client_login_epfd error: %s\n", strerror(errno));
            exit(1);
        }

        this_time = time(NULL);
        p_head = p_sev_ctrl->no_login.head;
        while(p_head != NULL)
        {
            p_user_info = (mid_sev_client_user_info_t *)p_head->data;

            /*检测到超时, 干掉超时用户*/
            if(this_time - p_user_info->cli_connect_time > 5 * 60)
            {
                p_head = list_del(&p_sev_ctrl->no_login, p_user_info);
                free(p_user_info);
                p_user_info = NULL;
                continue;
            }
            p_head = p_head->next;
        }

        int i = 0;
        for(i = 0; i < nfds; i++)
        {
            /*如果是监听套接字就绪，代表有新客户端连接到来*/
            if(evs[i].data.fd == lst_fd)
            {
                cli_fd = accept(lst_fd, (struct sockaddr *)&cli_addr, (socklen_t *)&len);
                if(cli_fd < 0)
                {
                    ERR("accept error: %s", strerror(errno));
                    continue;
                }

                /*边缘触发必须将socket设置为非阻塞*/
                /*将新客户端socket设置为非阻塞，并为该客户端创建用户信息节点*/
                setnonblock(cli_fd);


                /*为新用户创建用户结点*/
                p_user_info = malloc(sizeof(mid_sev_client_user_info_t));
                memset(p_user_info, 0x00, sizeof(mid_sev_client_user_info_t));
                p_user_info->cli_socket_fd = cli_fd;
                p_user_info->cli_connect_time = time(NULL);
                sprintf(p_user_info->cli_ip, "%s", inet_ntoa(cli_addr.sin_addr));
                p_user_info->cli_port = ntohs(cli_addr.sin_port);
                p_user_info->cli_backup_file_fd = -1;
                p_user_info->cli_upload_file_fd = -1;


                /*将新用户添加到未登录链表----------------------------*/
                list_head_add(&p_sev_ctrl->no_login, p_user_info);
                
                /*将新客户端添加到监听登录epoll集合*/
                ev.data.ptr = p_user_info;
                ev.events = EPOLLIN;
                epoll_ctl(p_sev_ctrl->client_login_epfd, EPOLL_CTL_ADD, cli_fd, &ev);


                DBG("new connect:[%s]-[%d], connect_time:[%ld]\n", p_user_info->cli_ip, p_user_info->cli_port, p_user_info->cli_connect_time);

            }

            /*代表有客户端登录请求到来*/
            else if(evs[i].events & EPOLLIN)
            {
                req_head_t cli_request_head;
                memset(&cli_request_head, 0x00, sizeof(req_head_t));
                p_user_info = evs[i].data.ptr;

                if(recv_data(p_user_info->cli_socket_fd, (char *)&cli_request_head, sizeof(req_head_t)) == -1)
                {
                    DBG("receive client user request head error!\n");
                    continue;
                }

                /*处理登录请求*/
                if(cli_request_head.type == REQ_LOGIN)
                {
                    deal_client_login(p_user_info, p_sev_ctrl);
                }
                /*请求格式错误情况*/
                else
                {
                    continue;
                }
            }
        }
    }
}

/*初始化sev_ctrl*/
void init_sev_ctrl(mid_sev_ctrl_t *p_sev_ctrl)
{
    memset(p_sev_ctrl, 0x00, sizeof(mid_sev_ctrl_t));
    sprintf(p_sev_ctrl->cloud_conf_path, "%s", "../etc/cloud_conf.cfg");
    p_sev_ctrl->backup_server_fd = -1;
    list_init(&p_sev_ctrl->no_login);
    list_init(&p_sev_ctrl->yes_login);
    list_init(&p_sev_ctrl->backup_list);
}

int main(int argc, char *argv[])
{
    int daemon_flag = 0;
    
    mid_sev_ctrl_t  sev_ctrl;

    /*初始化sev_ctrl*/
    init_sev_ctrl(&sev_ctrl);
    
    /*解析程序参数*/
    parse_argv(argc, argv, &daemon_flag);

    if(daemon_flag)
    {
        /*不更改进程工作目录为根目录，但关闭stdin、stdout、stderr三个描述符,进入守护进程模式*/
        daemon(1, 0);
    }

    /*初始化信号*/
    init_signal();

    /*解析配置文件*/
    parse_conf(&sev_ctrl);

    /*根据是否进入守护进程选择不同日志打印规则*/
    if(daemon_flag)
    {
        if(open_log(sev_ctrl.log_file_path, "f_cat") < 0)
        {
            return -1;
        }
    }
    else
    {
        if(open_log(sev_ctrl.log_file_path, "0_cat") < 0)
        {
            return -1;
        }
    }

    /*连接备份服务器*/
    if((sev_ctrl.backup_server_fd = connect_server(sev_ctrl.backup_sev_addr, sev_ctrl.backup_sev_port)) == -1)
    {
        ERR("connect backup_server error\n");
        return -1;
    }

    /*创建监听客户端登录epoll监听列表*/
    sev_ctrl.client_login_epfd = epoll_create(MAX_LISTEN);
    if(sev_ctrl.client_login_epfd < 0)
    {
        ERR("create client_login_epoll error:%s", strerror(errno));
        return -1;
    }

    /*创建监听客户端文件上传请求epoll集合*/
    sev_ctrl.client_request_epfd = epoll_create(MAX_LISTEN); 
    if(sev_ctrl.client_request_epfd < 0)
    {
        ERR("create client_request_epoll error:%s", strerror(errno));
        return -1;
    }

    /*接收备份文件上传线程启动*/
    thread_recv_backup_file_from_client(&sev_ctrl);

    /*发送备份文件至备份服务器线程启动*/
    thread_send_file_to_backup_server(&sev_ctrl);

    /*主线程监听客户端的连接请求及登录请求*/
    listen_client_connect_and_login(&sev_ctrl);

    return 0;
}

