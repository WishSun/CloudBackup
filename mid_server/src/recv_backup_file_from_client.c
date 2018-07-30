/*************************************************************************
	> File Name: recv_backup_file_from_client.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月07日 星期三 11时37分21秒
 ************************************************************************/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "./recv_backup_file_from_client.h"

/*职责: 维持一个上传链表，轮询从每个客户端的上传队列中接收备份文件, 接收一个备份文件完毕之后
        将该文件结点从该队列中删除，并添加到客户端的备份队列中
 */


/*处理客户端上传文件请求*/
int deal_client_upload_file(mid_sev_client_user_info_t *p_user_info)
{
    /*接收客户端上传的文件的文件头，将其添加到用户的上传队列*/

    /*1.接收文件头信息*/
    file_head_t file_head;
    if(recv_data(p_user_info->cli_socket_fd, (char *)&file_head, sizeof(file_head_t)) == -1)
    {
        DBG("receive file_head error!\n");
        return -1;
    }

    /*2.构建上传文件结点*/
    file_node_t *p_new_upload_file = malloc(sizeof(file_node_t));
    memset(p_new_upload_file, 0x00, sizeof(file_node_t));
    sprintf(p_new_upload_file->filename, "%s", file_head.filename);
    p_new_upload_file->file_mode = file_head.file_mode;
    p_new_upload_file->file_size = file_head.file_size;

    /*3.将上传文件添加到上传队列*/
    que_push(&p_user_info->upload_file_que,(char *)p_new_upload_file);

    return 0;
}

/*接收客户端备份文件上传线程函数*/
void *run_recv_backup_file_from_client(void *arg)
{
    /* 轮询接收客户端备份文件链表, 接收备份文件。当一个备份文件接收完毕之后
     * 做两件事:
     * 1. 将该文件结点从该队列中删除，并添加到客户端的备份队列中, 准备将之备份
     * 到备份服务器上
     * 2. 将该客户端通信套接字添加到监听上传文件请求epoll集合中，准备接收该
     * 客户端用户的下一个备份文件
     */   
    printf("接收客户端备份文件上传线程启动\n");

    mid_sev_ctrl_t *p_sev_ctrl = (mid_sev_ctrl_t *)arg;

    list_node_t  *p_each_node;
    mid_sev_client_user_info_t  *p_each_user;
    while(1)
    {
        /*轮询上传链表*/
        for(p_each_node = p_sev_ctrl->yes_login.head;
            p_each_node != NULL; p_each_node = p_each_node->next)
        {
            p_each_user = (mid_sev_client_user_info_t *)p_each_node->data;
            if(p_each_user == NULL)
            {
                /*如果链表为空*/
                if(p_sev_ctrl->yes_login.head == NULL)
                {
                    sleep(1);
                    break;
                }
                continue;
            }

            /*如果该用户当前没有正在上传的文件*/
            if(p_each_user->cli_upload_file_fd == -1)
            {
                /*如果上传队列中有结点, 则拿出来一个文件结点开始接收上传数据*/
                if(can_pop(&p_each_user->upload_file_que))
                {
                    file_node_t *p_new_upload_file = (file_node_t *)get_queue_head(&p_each_user->upload_file_que);

                    char upload_file_path[1024] = {0};
                    sprintf(upload_file_path, "./%s/%s", p_each_user->cli_username, p_new_upload_file->filename);

                    /*以只写并清空方式打开文件，如果文件不存在则创建*/
                    if((p_each_user->cli_upload_file_fd = 
                        open(upload_file_path, O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0)
                    {
                        perror("open");
                        continue;
                    }

                    p_each_user->cli_upload_file_size = p_new_upload_file->file_size;
                    p_each_user->cli_upload_bytes = 0;
                }
            }

            /*说明该用户的上传队列已空，即无需要上传的文件*/
            if(p_each_user->cli_upload_file_fd == -1)
            {
                continue;
            }

            /*如果当前上传文件已经接受完毕*/
            if(p_each_user->cli_upload_bytes == p_each_user->cli_upload_file_size)
            {
                /* 1. 将该文件结点从该队列中删除*/
                file_node_t *p_finish_file =(file_node_t *)que_pop(&p_each_user->upload_file_que);
                printf("上传的文件[%s]:[%ld字节]已接收完毕\n", p_finish_file->filename, p_finish_file->file_size);

                /* 2.并添加到客户端的备份队列中*/
                que_push(&p_each_user->backup_file_que, (char *)p_finish_file);

                /* 3.关闭文件*/
                close(p_each_user->cli_upload_file_fd);

                /* 4.将当前上传文件描述符置为-1*/
                p_each_user->cli_upload_file_fd = -1;

                /* 5.向客户端发送文件已接收完毕回复*/
                req_head_t rsp_finish;
                rsp_finish.type = RSP_FINISH;
                if(send_data(p_each_user->cli_socket_fd, (char *)&rsp_finish, sizeof(req_head_t)) == -1)
                {
                    continue;
                }

                /* 5.将该用户再次添加到epoll监听集合中，准备接收下一个上传文件头*/
                struct epoll_event ev;
                ev.data.ptr = p_each_user;
                ev.events = EPOLLIN;
                epoll_ctl(p_sev_ctrl->client_request_epfd, EPOLL_CTL_ADD, p_each_user->cli_socket_fd, &ev);

                continue;
            }

            /*剩余字节数*/
            uint64_t surplus_len = p_each_user->cli_upload_file_size - p_each_user->cli_upload_bytes;

            /*本次需要接收的字节数。每个用户的文件只接收一点: 最多4096个字节*/
            int recv_len = surplus_len >= SEND_OR_RECV_BUFF  ?  SEND_OR_RECV_BUFF : surplus_len;

            /*开始从客户端接收文件数据*/
            char buff[SEND_OR_RECV_BUFF] = {0};
            if(recv_data(p_each_user->cli_socket_fd, buff, recv_len) == -1)
            {
                continue;
            }

            /*调整文件指针位置*/
            if(lseek(p_each_user->cli_upload_file_fd, p_each_user->cli_upload_bytes, SEEK_SET) == -1)
            {
                continue;
            }

            /*将接收的文件数据写入文件*/
            if(send_data(p_each_user->cli_upload_file_fd, buff, recv_len) == -1)
            {
                continue;
            }
            p_each_user->cli_upload_bytes += recv_len;
        }
    }
}
void *run_recv_upload_request_from_client(void *arg)
{
    printf("接收文件上传请求线程启动\n");
    mid_sev_ctrl_t *p_sev_ctrl = (mid_sev_ctrl_t *)arg;

    struct epoll_event evs[MAX_LISTEN], ev;
    mid_sev_client_user_info_t *p_user_info;

    while(1)
    {
        /*设置3秒钟超时，即3秒钟检测一次客户端链表*/
        int nfds = epoll_wait(p_sev_ctrl->client_request_epfd, evs, MAX_LISTEN, 3000);
        if(nfds <= 0)
        {
            continue;
        }

        int i = 0;
        for(i = 0; i < nfds; i++)
        {
            /*代表有客户端文件上传请求到来*/
            if(evs[i].events & EPOLLIN)
            {
                p_user_info = evs[i].data.ptr;

                req_head_t cli_upload_request_head;
                memset(&cli_upload_request_head, 0x00, sizeof(req_head_t));
                if(recv_data(p_user_info->cli_socket_fd, (char *)&cli_upload_request_head, sizeof(req_head_t)) == -1)
                {
                    DBG("receive client user request head error!\n");
                    continue;
                }

                /*处理客户端的文件上传请求*/
                if(cli_upload_request_head.type == REQ_UPLOAD)
                {
                    /*将其从监听客户端文件上传请求epoll集合中删除*/
                    epoll_ctl(p_sev_ctrl->client_request_epfd, EPOLL_CTL_DEL, p_user_info->cli_socket_fd, &ev);

                    /*去接收上传文件*/
                    deal_client_upload_file(p_user_info);
                } 
                /*格式错误，将其干掉*/
                else
                {
                    /*将其从监听客户端文件上传请求epoll集合中删除*/
                    epoll_ctl(p_sev_ctrl->client_request_epfd, EPOLL_CTL_DEL, p_user_info->cli_socket_fd, &ev);

                    /*将其从已登陆链表中删除*/
                    list_del(&p_sev_ctrl->yes_login, p_user_info);
                    free(p_user_info);
                    continue;
                }
            }
        }
    }

}

/*接收客户端备份文件上传线程入口*/
int thread_recv_backup_file_from_client(mid_sev_ctrl_t *p_sev_ctrl)
{
    pthread_t tid;

    if(pthread_create(&tid, NULL, run_recv_upload_request_from_client, p_sev_ctrl) != 0)
    {
        ERR("create recv_backup_file_from_client thread error: %s", strerror(errno));
        return -1;
    }

    if(pthread_create(&tid, NULL, run_recv_backup_file_from_client, p_sev_ctrl) != 0)
    {
        ERR("create recv_backup_file_from_client thread error: %s", strerror(errno));
        return -1;
    }

    return 0;
}
