/*************************************************************************
	> File Name: recv_and_deal_request.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月11日 星期日 21时45分05秒
 ************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <dirent.h>
#include "./recv_and_deal_request.h"

#if 0
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
        char buff[1024] = {0};

        /*将浏览到的文件名(仅有文件名，没有路径)和路径结合起来，组成全路径名*/
        sprintf(buff, "./%s/%s", p_user_info->cli_username, p_file->d_name);

        /*如果文件类型为4（目录），则跳过*/
        if (p_file->d_type == 4) 
        {
            continue;
        }

        /*将文件添加进恢复队列*/
        file_node_t  *p_new_file = (file_node_t *)malloc(sizeof(file_node_t));
        memset(p_new_file, 0x00, sizeof(file_node_t));
        memcpy(p_new_file->filename, buff, strlen(buff));

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
#endif

#if 0
/*接收客户端的请求线程函数*/
void *run_recv_request(void *arg)
{
    pthread_detach(pthread_self());

    backup_sev_ctrl_t *p_sev_ctrl = (backup_sev_ctrl_t *)arg;

    int nfds;
    struct epoll_event evs[MAX_LISTEN], ev;
    backup_sev_client_user_info_t *p_user_info = NULL;
    req_head_t req_head, rsp_head;

    while(1)
    {
        nfds = epoll_wait(p_sev_ctrl->client_request_epfd, evs, MAX_LISTEN, 3000);
        if(nfds < 0)
        {
            return NULL;
        }

        int i = 0;
        for(i = 0; i < nfds; i++)
        {
            p_user_info = evs[i].data.ptr;

            if(recv_data(p_user_info->cli_socket_fd, &req_head, sizeof(req_head_t)) == -1)
            {
                return NULL;
            }
            if(req_head.type == REQ_RESTORE)
            {
                printf("有客户端恢复请求到来！，添加备份文件到恢复队列\n");
                /*将该客户端的备份目录下的所有文件名后缀不为.ocp的文件添加到恢复队列*/
                scan_restore_file_to_queue(p_user_info);


                /*向客户端发送请求回复*/
                printf("发送恢复请求的回复\n");
                memset(&rsp_head, 0x00, sizeof(rsp_head));
                rsp_head.type = RSP_RESTORE;
                if(send_data(p_user_info->cli_socket_fd, &rsp_head, sizeof(req_head_t)) == -1)
                {
                    return NULL;
                }

                printf("激活向客户端发送备份文件\n");
                /*激活向客户端发送备份文件*/
                p_user_info->restore_flag = 1;

                /*更新连接时间*/
                p_user_info->cli_connect_time = time(NULL);
            }
            else
            {
                continue;
            }
        }
    }
    return NULL;
}
#endif

/*处理客户端的请求线程函数*/
void *run_deal_request(void *arg)
{
    pthread_detach(pthread_self());

    backup_sev_ctrl_t *p_sev_ctrl = (backup_sev_ctrl_t *)arg;
    file_node_t *p_restore_file = NULL;

    /*轮询整个用户链表，将所有的用户的文件发送给所有请求恢复的客户端*/
    list_node_t *p_node = NULL;
    backup_sev_client_user_info_t *p_user_info = NULL;

    file_head_t file_head;
    char send_buff[SEND_OR_RECV_BUFF] = {0};
    while(1)
    {
        if(p_sev_ctrl->user_list.head == NULL)
        {
            printf("用户队列为空\n");
            sleep(1);
        }
        for(p_node = p_sev_ctrl->user_list.head; p_node != NULL;)
        {
            p_user_info = (backup_sev_client_user_info_t *)p_node->data;

            if(p_user_info->cli_restore_file_fd == -1)
            {
                printf("拿出队头文件进行发送\n");
                if(can_pop(&p_user_info->restore_file_que))
                {
                    p_restore_file = (file_node_t *)get_queue_head(&p_user_info->restore_file_que);
                    p_user_info->cli_restore_bytes = 0;
                    p_user_info->cli_restore_file_size = p_restore_file->file_size;

                    printf("用户名为[%s], 描述符为[%d]\n", p_user_info->cli_username, p_user_info->cli_socket_fd);
                    printf("发送文件[%s]的文件头\n", p_restore_file->filename);
                    /*发送文件头*/
                    memset(&file_head, 0x00, sizeof(file_head_t));
                    sprintf(file_head.filename, "%s", p_restore_file->filename);
                    file_head.file_mode = p_restore_file->file_mode;
                    file_head.file_size = p_restore_file->file_size;
                    if(send_data(p_user_info->cli_socket_fd, &file_head, sizeof(file_head_t)) == -1)
                    {
                        return NULL;
                    }

                    /*打开文件*/
                    if((p_user_info->cli_restore_file_fd = open(p_restore_file->filename, O_RDONLY)) == -1)
                    {
                        return NULL;
                    }
                }   
                else
                {
                    printf("全部恢复完毕, 已发送完毕标志\n");
                    /*向客户端发送文件已恢复完毕*/
                    memset(&file_head, 0x00, sizeof(file_head_t));
                    file_head.file_mode = -1;
                    file_head.file_size = -1;
                    if(send_data(p_user_info->cli_socket_fd, &file_head, sizeof(file_head_t)) == -1)
                    {
                        return NULL;
                    }

                    p_node = p_node->next;
                    /*将客户端结点从用户链表中删除*/
                    list_del(&p_sev_ctrl->user_list, p_user_info);
                    free(p_user_info);
                    p_user_info = NULL;
                    continue;
                }
            }

            lseek(p_user_info->cli_restore_file_fd, p_user_info->cli_restore_bytes, SEEK_SET);

            int send_len = p_user_info->cli_restore_file_size - p_user_info->cli_restore_bytes;
send_len = send_len > SEND_OR_RECV_BUFF ? SEND_OR_RECV_BUFF : send_len;

            /*读取文件内容并发送*/
            if(recv_data(p_user_info->cli_restore_file_fd, send_buff, send_len) == -1)
            {
                return NULL;
            }

            if(send_data(p_user_info->cli_socket_fd, send_buff, send_len) == -1)
            {
                return NULL;
            }

            p_user_info->cli_restore_bytes += send_len;

            /*文件发送完毕*/
            if(p_user_info->cli_restore_bytes == p_user_info->cli_restore_file_size)
            {
                /*将文件出队*/
                que_pop(&p_user_info->restore_file_que);
                /*将文件描述符置为-1*/
                p_user_info->cli_restore_file_fd = -1;
            }

            p_node = p_node->next;
        }
    }
    return NULL;
}

/*接收并处理中转服务器或客户端的请求线程入口*/
int thread_recv_and_deal_request(backup_sev_ctrl_t *p_sev_ctrl)
{
    pthread_t tid;

    #if 0
    if(pthread_create(&tid, NULL, run_recv_request, p_sev_ctrl) != 0)
    {
        ERR("create recv client request thread  error:", strerror(errno));
        return -1;
    }
    #endif
 
    printf("处理客户端恢复文件请求线程启动！\n");
    if(pthread_create(&tid, NULL, run_deal_request, p_sev_ctrl) != 0)
    {
        ERR("create deal client request thread error:", strerror(errno));
        return -1;
    }   
    return 0;
}
