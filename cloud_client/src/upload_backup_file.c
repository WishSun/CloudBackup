/*************************************************************************
	> File Name: upload_backup_file.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月06日 星期二 19时48分02秒
 ************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include "./upload_backup_file.h"

/*从路径中获取文件名*/
void GetFileNameFromPath(char *path, int path_size)
{
    char *temp_path = malloc(strlen(path) + 1);
    memset(temp_path, 0x00, strlen(path) + 1);
    sprintf(temp_path, "%s", path);
    memset(path, 0x00, path_size);
    
    /*从后向前找'/'的位置，'/'之后的字符串就是文件名, 如果没找到，则路径本身就是文件名*/
    char *begin = temp_path + strlen(temp_path) - 1;
    while((begin != temp_path) && (*begin != '/'))
    {
        begin--;
    }
    if(begin == temp_path)
    {
        sprintf(path, "%s", temp_path);
        return;
    }

    begin++;
    sprintf(path, "%s", begin);

    free(temp_path);
    temp_path = NULL;
}


/*向中转服务器发送文件数据*/
int send_file_data_to_mid_server(user_t *p_user, file_node_t *p_trans_file)
{
    int fd;

    /*打开文件*/
    if((fd = open(p_trans_file->filename, O_RDONLY)) == -1)
    {
        perror("open");
        return -1;
    }

    int64_t  upload_len = 0;
    char buff[SEND_OR_RECV_BUFF] = {0};
    printf("上传文件大小为: %ld字节\n", p_trans_file->file_size);

    printf("\n已上传");
    printf("  0%%");
    while(upload_len < p_trans_file->file_size)
    {
        double flen = upload_len;
        printf("\b\b\b\b%3.0f%%", (flen/ p_trans_file->file_size) * 100);

        /*计算出还要上传的字节数len*/
        int len = p_trans_file->file_size - upload_len;
        len = len > SEND_OR_RECV_BUFF ? SEND_OR_RECV_BUFF : len;

        /*从文件中读取len字节数据*/
        if(recv_data(fd, buff, len) == -1)
        {
            printf("read file data error\n");
            return -1;
        }

        /*将从文件中读到的len字节数据上传给服务器*/
        if(send_data(p_user->mid_server_fd, buff, len) == -1)
        {
            printf("send file data to mid_server error\n");
            return -1;
        }

        upload_len += len;
    }

    printf("\b\b\b\b%3d%%", 100);
    printf("\n上传完毕!\n");
    close(fd);
    return 0;
}

/*上传文件*/
int upload_backup_file(user_t *p_user, file_node_t *p_trans_file)
{
    /*构建公共请求头*/
    req_head_t  upload_head;
    upload_head.type = REQ_UPLOAD;
    upload_head.length = sizeof(file_head_t);

    /*发送公共请求头*/
    if(send_data(p_user->mid_server_fd, (char*)&upload_head, sizeof(req_head_t)) == -1)
    {
        printf("send upload_head error!\n");
        return -1;
    }

    file_head_t  file_head;
    char filename[1024] = {0};
    memset(&file_head, 0x00, sizeof(file_head_t));

    /*从路径中获取文件名*/
    sprintf(filename, "%s", p_trans_file->filename);
    GetFileNameFromPath(filename, 1024);
    sprintf(file_head.filename, "%s", filename);
    file_head.file_size = p_trans_file->file_size;
    file_head.file_mode = p_trans_file->file_mode;

    /*发送文件头*/
    if(send_data(p_user->mid_server_fd, (char*)&file_head, sizeof(file_head_t)) == -1)
    {
        printf("send file_head error!\n");
        return -1;
    }

    /*发送文件数据*/
    send_file_data_to_mid_server(p_user, p_trans_file);

    return 0;
}

/*上传备份文件线程函数*/
void *run_upload_backup_file(void *arg)
{
    printf("上传备份文件线程启动\n");
    /*分离线程*/
    pthread_detach(pthread_self());

    user_t *p_user = (user_t *)arg;


    /*正在被上传的文件*/
    file_node_t *p_trans_file;

    /*等待可以上传文件的信号*/
    condition_wait(&p_user->ready_upload_file);

    while(1)
    {
        /*从传输队列中拿出一个未被占用的文件, 并将遇到的被占用的文件从队列中出队*/
        while(can_pop(&p_user->trans_que))
        {
            p_trans_file = (file_node_t *)get_queue_head(&p_user->trans_que);
            if(p_trans_file->occupy_flag == TRUE)
            {
                que_pop(&p_user->trans_que);

                /*如果传输队列为空，则再次开始监控目录*/
                if(can_push(&p_user->trans_que))
                {
                    sleep(1);
                    /*发送开始监控目录信号*/
                    condition_signal(&p_user->ready_scan_dir);
                    /*等待可以上传文件的信号*/
                    condition_wait(&p_user->ready_upload_file);
                }
            }
            else
            {
                break;
            }
        }

        while(1)
        {
            /*上传文件*/
            upload_backup_file(p_user, p_trans_file);

            /*接收服务器发送过来的上传文件接收完毕回复*/
            req_head_t rsp_finish;
            memset(&rsp_finish, 0x00, sizeof(req_head_t));
            printf("正在接受完成回复\n");
            if(recv_data(p_user->mid_server_fd, (char *)&rsp_finish, sizeof(req_head_t)) == -1)
            {
                return NULL;
            }
            printf("接受完成回复完毕\n");

            /*判断回复，如果文件数据出错，则要重传*/
            if(rsp_finish.type == RSP_FINISH)
            {
                break;
            }
            else
            {
                printf("文件数据传输出错, 正在重传...\n");
                continue;
            }
        }
        /*将文件从本地删除*/
        unlink(p_trans_file->filename);

        /*将文件出队*/
        file_node_t *p_pop_file = (file_node_t *)que_pop(&p_user->trans_que);
        free(p_pop_file);

        /*如果传输队列中的文件都传输完毕，则再次开始监控目录*/
        if(can_push(&p_user->trans_que))
        {
            /*发送开始监控目录信号*/
            condition_signal(&p_user->ready_scan_dir);
            /*等待可以上传文件的信号*/
            condition_wait(&p_user->ready_upload_file);
        }
    }
}


/*上传备份文件线程入口函数*/
int thread_upload_backup_file(user_t *p_user)
{
    pthread_t tid;

    /*创建上传备份文件线程*/
    if(pthread_create(&tid, NULL, run_upload_backup_file, p_user) < 0)
    {
        printf("create upload_backup_file thread error:%s", strerror(errno));
        return -1;
    }

    return 0;
}
