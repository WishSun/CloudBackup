/*************************************************************************
	> File Name: send_file.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月07日 星期三 11时21分51秒
 ************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "./send_file_to_backup_server.h"

/*职责: 维持一个备份链表，轮询将每个客户端的备份队列中的一个文件备份至
备份服务器，当备份完成之后，将改文件从客户端的备份队列中删除,并将改文件在中转服务器上删除
 */


/*发送文件到备份服务器线程函数*/
void *run_send_file_to_backup_server(void *arg)
{
    /* 轮询发送每个客户端的备份文件到备份服务器上, 当一个备份文件
     * 发送完毕之后，做两件事:
     * 1. 将文件从中转服务器上删除
     * 2. 将文件结点从备份队列上删除
     */
    printf("发送文件到备份服务器线程启动\n");

    mid_sev_ctrl_t *p_sev_ctrl = (mid_sev_ctrl_t *)arg;

    list_node_t  *p_each_node = NULL;
    mid_sev_client_user_info_t  *p_each_user = NULL;
    file_node_t *p_new_backup_file = NULL;


    char backup_file_path[1024] = {0};
    char buff[SEND_OR_RECV_BUFF + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES + BACKUP_FLAG_BYTES] = {0};
    printf("buff size = %ld\n", sizeof(buff));

    while(1)
    {
        sleep(1);

        /*轮询备份链表*/
        for(p_each_node = p_sev_ctrl->yes_login.head;
            p_each_node != NULL; p_each_node = p_each_node->next)
        {
            p_each_user = (mid_sev_client_user_info_t *)p_each_node->data;
            if(p_each_user == NULL)
            {
                /*如果链表为空*/
                if(p_sev_ctrl->backup_list.head == NULL)
                {
                    printf("链表为空...\n");
                    sleep(1);
                    break;
                }
                continue;
            }

            /*如果该用户当前没有正在备份的文件*/
            if(p_each_user->cli_backup_file_fd == -1)
            {
                /*如果备份队列中有结点，则拿出来一个文件结点，开始备份*/
                if(can_pop(&p_each_user->backup_file_que))
                {
                    p_new_backup_file = (file_node_t *)get_queue_head(&p_each_user->backup_file_que);

                    sprintf(backup_file_path, "./%s/%s", p_each_user->cli_username, p_new_backup_file->filename);
                    printf("备份文件:[%s], 大小为[%ld]\n", backup_file_path, p_new_backup_file->file_size);

                    /*以只读方式打开文件*/
                    if((p_each_user->cli_backup_file_fd = open(backup_file_path, O_RDONLY)) < 0)
                    {
                        perror("open");
                        continue;
                    }

                    p_each_user->cli_backup_file_size = p_new_backup_file->file_size;
                    p_each_user->cli_backup_bytes = 0;
                }
            }
            
            /*说明该用户的备份队列为空，即当前无需要备份的文件*/
            if(p_each_user->cli_backup_file_fd == -1)
            {
                continue;
            }

            /*如果当前备份文件已发送完毕*/
            if(p_each_user->cli_backup_bytes == p_each_user->cli_backup_file_size)
            {
                /*3. 将文件结点在备份队列中弹出，并释放*/
                file_node_t *p_del_file = (file_node_t *)que_pop(&p_each_user->backup_file_que);
                printf("文件[%s]备份完毕\n", backup_file_path);

                /*4. 关闭文件*/
                close(p_each_user->cli_backup_file_fd);

                /*5. 从本地删除该文件*/
                unlink(backup_file_path);
                free(p_del_file);
                p_del_file = NULL;

                /*6. 将当前备份文件描述符置为-1*/
                p_each_user->cli_backup_file_fd = -1;
            }

            /*发送一点文件内容----------------------------*/
            /*文件剩余字节数*/
            uint64_t surplus_len = p_each_user->cli_backup_file_size - p_each_user->cli_backup_bytes;

            /*本次需要发送的文件数据字节数*/
            int send_len = surplus_len >= SEND_OR_RECV_BUFF  ?  SEND_OR_RECV_BUFF : surplus_len;


            /*调整文件指针位置*/
            if(lseek(p_each_user->cli_backup_file_fd, p_each_user->cli_backup_bytes, SEEK_SET) == -1)
            {
                continue;
            }

            /*填充用户名*/
            strncpy(buff, p_each_user->cli_username, USERNAME_BYTES);

            /*填充文件名*/
            strncpy(buff + USERNAME_BYTES, p_new_backup_file->filename, FILENAME_BYTES);

            /*填充文件权限*/
            *(mode_t *)(buff + USERNAME_BYTES + FILENAME_BYTES) = p_new_backup_file->file_mode;

            /*填充文件数据长度*/
            *(int *)(buff + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES) = send_len;


            /*设置文件是否已发送完标志位*/
            if(p_each_user->cli_backup_bytes + send_len == p_each_user->cli_backup_file_size)
            {
                *(char *)(buff + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES) |= FILE_FINISH;
            }
            else
            {
                *(char *)(buff + USERNAME_BYTES +  FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES) &= ~FILE_FINISH;
            }

            p_each_user->cli_backup_bytes += send_len;
            
            /*设置文件是否刚开始发送标志位*/
            if(p_each_user->cli_backup_bytes - send_len == 0)
            {
                *(char *)(buff + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES) |= FILE_FIRST_COME;
            }
            else
            {
                *(char *)(buff + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES) &= ~FILE_FIRST_COME;
            }

            /*读取文件数据*/
            int ret = 0;
            if((ret = recv_data(p_each_user->cli_backup_file_fd, buff + USERNAME_BYTES + SEND_LEN_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + BACKUP_FLAG_BYTES, send_len)) == -1)
            {
                printf("ret = %d", ret);
                continue;
            }

            /*将从文件中读取的内容发送给备份服务器*/
            if(send_data(p_sev_ctrl->backup_server_fd, buff, sizeof(buff)) == -1)
            {
                continue;
            }

            memset(buff, 0x00, sizeof(buff));
        }
    }
}

/*发送文件到备份服务器线程入口*/
int thread_send_file_to_backup_server(mid_sev_ctrl_t *p_sev_ctrl)
{
    pthread_t  tid;

    if(pthread_create(&tid, NULL, run_send_file_to_backup_server, p_sev_ctrl) != 0)
    {
        ERR("create send_file_to_backup_server thread error:%s", strerror(errno));
        return -1;
    }

    return 0;
}
