/*************************************************************************
	> File Name: backup_server.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月10日 星期六 21时30分18秒
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
#include <dirent.h>
#include "../../common_include/common.h"
#include "./recv_and_deal_request.h"
#include "./recv_mid_server_backup_file.h"


/*将该客户端的备份目录下的所有文件添加到恢复队列*/
int scan_restore_file_to_queue(backup_sev_client_user_info_t *p_user_info)
{
    printf("用户备份目录为:[%s]\n", p_user_info->cli_username);
    /*查看用户备份目录是否存在*/
    if(access(p_user_info->cli_username, F_OK) != 0)
    {
        if(mkdir(p_user_info->cli_username, 0777) == -1)
        {
            return -1;
        }
        return -1;
    }

    DIR *p_dir = NULL;
    
    /*打开目录*/
    p_dir = opendir(p_user_info->cli_username);
    if (p_dir == NULL)
    {
        printf("opendir error:[%s-%s]\n",p_user_info->cli_username, strerror(errno));
        return -1;
    }

    struct dirent *p_file = NULL;

    while((p_file = readdir(p_dir)) != NULL)
    {
        /*如果文件名是.或者..代表这两个目录不能浏览，继续取其他文件*/
        if (!strcmp(p_file->d_name, ".") || !strcmp(p_file->d_name, ".."))
        {
            continue;
        }

        /*如果文件不完整(正在备份)，则跳过*/
        if(!strcmp(p_file->d_name + strlen(p_file->d_name) - 5, ".ocp"))
        {
            continue;
        }

        /*如果文件类型为4（目录），则跳过*/
        if (p_file->d_type == 4) 
        {
            continue;
        }

        /*将文件添加进恢复队列*/
        file_node_t  *p_new_file = (file_node_t *)malloc(sizeof(file_node_t));
        memset(p_new_file, 0x00, sizeof(file_node_t));
        sprintf(p_new_file->filename,"%s", p_file->d_name);

        struct stat st;
        if(stat(p_new_file->filename, &st) == -1)
        {
            perror("stat");
            continue;
        }
        p_new_file->file_mode = st.st_mode;
        p_new_file->file_size = st.st_size;

        que_push(&p_user_info->restore_file_que, (char *)p_new_file);
    }

    //关闭目录,释放资源
    closedir(p_dir);
    return 0;

}


/*初始化服务器结构信息*/
void init_sev_ctrl(backup_sev_ctrl_t *p_sev_ctrl)
{
    memset(p_sev_ctrl, 0x00, sizeof(backup_sev_ctrl_t));
    sprintf(p_sev_ctrl->cloud_conf_path, "%s", "../etc/cloud_conf.cfg");
    p_sev_ctrl->mid_server_fd = -1;
    list_init(&p_sev_ctrl->user_list);
}

/*解析配置文件 */
int parse_conf(backup_sev_ctrl_t *p_sev_ctrl)
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

    /*获取备份服务端IP地址*/
    if(!config_lookup_string(&cfg, "conf.backup_sev_addr", &ptr))
    {
        printf("get backup_sev_addr error!\n");
        return -1;
    }
    memcpy(p_sev_ctrl->sev_addr, ptr, strlen(ptr));

    /*获取服务端端口*/
    if(!config_lookup_int(&cfg, "conf.backup_sev_port", &p_sev_ctrl->sev_port))
    {
        printf("get backup_sev_port error!\n");
        return -1;
    }

    /*释放资源*/
    config_destroy(&cfg);
    return 0;
}

/*监听所有连接*/
int listen_mid_server_and_client_connect(backup_sev_ctrl_t *p_sev_ctrl)
{
    int lst_fd, len, cli_fd;
    int nfds;
    struct sockaddr_in cli_addr;
    struct epoll_event evs[MAX_LISTEN], ev;

    /*创建监听套接字，并开始监听*/
    if((lst_fd = create_listen_socket(p_sev_ctrl->sev_addr, p_sev_ctrl->sev_port)) == -1)
    {
        ERR("create listen scoket error");
        return -1;
    }

    ev.data.fd = lst_fd;
    ev.events = EPOLLIN;
    epoll_ctl(p_sev_ctrl->all_connect_epfd, EPOLL_CTL_ADD, lst_fd, &ev);

    backup_sev_client_user_info_t *p_user_info = NULL;
    while(1)
    {
        nfds = epoll_wait(p_sev_ctrl->all_connect_epfd, evs, MAX_LISTEN, 3000);
        if(nfds < 0)
        {
            return -1;
        }

        int i = 0;
        for(i = 0; i < nfds; i++)
        {
            /*如果是监听套接字就绪，代表有连接到来*/
            if(evs[i].data.fd == lst_fd)
            {
                cli_fd = accept(lst_fd, (struct sockaddr *)&cli_addr, (socklen_t *)&len);
                if(cli_fd < 0)
                {
                    ERR("accept error: %s", strerror(errno));
                    continue;
                }

                setnonblock(cli_fd);
                /*第一次到来的连接肯定是中转服务器连接*/
                if(p_sev_ctrl->mid_server_fd == -1)
                {
                    p_sev_ctrl->mid_server_fd = cli_fd;

                    DBG("mid_server connect comming!\n");
                    continue;
                }

                /*为客户端用户创建用户结点*/
                p_user_info = malloc(sizeof(backup_sev_client_user_info_t));
                memset(p_user_info, 0x00, sizeof(backup_sev_client_user_info_t));
                sprintf(p_user_info->cli_username, "%s", "WishSun");
                p_user_info->cli_socket_fd = cli_fd;
                p_user_info->cli_connect_time = time(NULL);
                sprintf(p_user_info->cli_ip, "%s", inet_ntoa(cli_addr.sin_addr));
                p_user_info->cli_port = ntohs(cli_addr.sin_port);
                p_user_info->cli_restore_file_fd = -1;
                p_user_info->restore_flag = 1;

                /*将备份文件结点添加到恢复队列中*/
                que_init(&p_user_info->restore_file_que);
                scan_restore_file_to_queue(p_user_info);
                file_node_t *p_file = NULL;
                node_t *p_node = p_user_info->restore_file_que.head;

                for(;p_node != NULL; p_node = p_node->next)
                {
                    p_file = (file_node_t *)p_node->data;
                    printf("文件:[%s]:大小:[%ld]\n", p_file->filename, p_file->file_size);
                }

                /*将客户端描述符从该监听集合中删除，并添加到监听请求epoll集合中*/
                epoll_ctl(p_sev_ctrl->all_connect_epfd, EPOLL_CTL_DEL, cli_fd, &ev);

                /*将新用户添加到用户链表----------------------------*/
                list_head_add(&p_sev_ctrl->user_list, p_user_info);

#if 0
                ev.data.ptr = p_user_info;
                ev.events = EPOLLIN;
                epoll_ctl(p_sev_ctrl->client_request_epfd, EPOLL_CTL_ADD, cli_fd, &ev);
#endif

                DBG("new client connect:[%s]-[%d], connect_time:[%ld]\n", p_user_info->cli_ip, p_user_info->cli_port, p_user_info->cli_connect_time);
            }
        }
    }
}


int main(void)
{
    backup_sev_ctrl_t sev_ctrl;

    /*初始化服务器结构信息*/
    init_sev_ctrl(&sev_ctrl);

    /*解析配置文件*/
    parse_conf(&sev_ctrl);

    printf("IP: [%s] port: [%d]", sev_ctrl.sev_addr, sev_ctrl.sev_port);

    if(open_log(sev_ctrl.log_file_path, "0_cat") < 0)
    {
        return -1;
    }

    /*创建监听所有连接的epoll监听集合:
     1. 如果是中转服务器连接，则开启接收中转服务器备份文件线程，开始不断接收数据
     2. 如果是客户端连接，则将其添加到监听客户端请求epoll集合中，并从本集合中删除
     3. 第一次连接肯定是中转服务器连接
    */
    sev_ctrl.all_connect_epfd = epoll_create(MAX_LISTEN);
    if(sev_ctrl.all_connect_epfd < 0)
    {
        ERR("create all_connect_epoll error:%s", strerror(errno));
        return -1;
    }

#if 0
    /*创建监听客户端的恢复文件请求的epoll集合,监听客户端的恢复文件请求，并处理该
      请求(向客户端发送备份文件)*/
    sev_ctrl.client_request_epfd = epoll_create(MAX_LISTEN); 
    if(sev_ctrl.client_request_epfd < 0)
    {
        ERR("create client_request_epoll error:%s", strerror(errno));
        return -1;
    }
#endif

    /*接收中转服务器的文件数据*/
    thread_recv_mid_server_backup_file(&sev_ctrl);

    /*恢复客户端文件*/
    thread_recv_and_deal_request(&sev_ctrl);

    /*监听所有连接*/
    listen_mid_server_and_client_connect(&sev_ctrl);

    return 0;
}
