/*************************************************************************
	> File Name: recv_mid_server_backup_file.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月11日 星期日 10时51分04秒
 ************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "./recv_mid_server_backup_file.h"


/*从中转服务器接收备份文件线程函数*/
void *run_recv_mid_server_backup_file(void *arg)
{
    pthread_detach(pthread_self());

    backup_sev_ctrl_t *p_sev_ctrl = (backup_sev_ctrl_t *)arg;
    
    char recv_data_buff[SEND_OR_RECV_BUFF + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES + BACKUP_FLAG_BYTES] = {0};
    char username[USERNAME_BYTES] = {0};
    char filename[FILENAME_BYTES] = {0};
    char filepath[FILEPATH_BYTES] = {0};
    mode_t file_mode = -1;
    int file_data_len = 0;
    char backup_flag = 0;
    int file_fd = -1;

    while(1)
    {
        if(p_sev_ctrl->mid_server_fd == -1)
        {
            sleep(1);
            continue;
        }

        int ret;
        if((ret = recv_data(p_sev_ctrl->mid_server_fd, recv_data_buff, sizeof(recv_data_buff))) == -1)
        {
            continue;
        }
        if(ret == 1)
        {
            printf("中转服务器退出\n");
            p_sev_ctrl->mid_server_fd = -1;
        }

        /*1. 获取用户名*/
        strncpy(username, recv_data_buff, USERNAME_BYTES);
        if(username[0] == '\0')
        {
            continue;
        }   

        /*2. 创建用户目录*/
        mkdir(username, 0777);
       
        /*3. 获取文件名*/
        strncpy(filename, recv_data_buff + USERNAME_BYTES, FILENAME_BYTES);

        /*4. 获取文件属性*/
        file_mode = *(mode_t *)(recv_data_buff + USERNAME_BYTES + FILENAME_BYTES);

        /*5. 获取备份文件数据长度*/
        file_data_len = *(int *)(recv_data_buff + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES);

        /*6. 获取备份标志*/
        backup_flag = *(char *)(recv_data_buff + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES);

        /*7. 根据标志位选择不同的方式打开文件*/
        if(backup_flag & FILE_FIRST_COME)
        {/*文件数据第一次来*/
            sprintf(filepath, "./%s/%s.ocp", username, filename);
            if((file_fd = open(filepath, O_WRONLY | O_TRUNC | O_CREAT, file_mode)) == -1)
            {
                perror("第一次open");
                return NULL;
            }
        }
        else
        {
            sprintf(filepath, "./%s/%s.ocp", username, filename);
            if((file_fd = open(filepath, O_WRONLY | O_APPEND)) == -1)
            {
                perror("第二次open");
                return NULL;
            }
        }

        /*8.将文件数据写入文件 */
        if(send_data(file_fd, recv_data_buff + USERNAME_BYTES + FILENAME_BYTES + FILE_MODE_BYTES + SEND_LEN_BYTES + BACKUP_FLAG_BYTES, file_data_len) == -1)
        {
            return NULL;
        }

        /*9. 关闭文件*/
        close(file_fd);

        /*10. 判断文件是否接收完毕，如果完毕，则更改文件权限为用户拥有可执行权限*/
        if(backup_flag & FILE_FINISH)
        {/*已经接收完毕*/
            
            /*去掉文件的.ocp(占用中状态)后缀*/
            char true_file_path[FILEPATH_BYTES] = {0};
            sprintf(true_file_path, "./%s/%s", username, filename);
            rename(filepath, true_file_path);
            
            printf("文件[%s]接收完毕\n", filepath);
        }

        memset(recv_data_buff, 0x00, sizeof(recv_data_buff));
        memset(username, 0x00, sizeof(username));
        memset(filename, 0x00, sizeof(filename));
        memset(filepath, 0x00, sizeof(filepath));
        file_data_len = 0;
        file_mode = -1;
        backup_flag = 0;
        file_fd = -1;
    }

    return NULL;
}

/*从中转服务器接收备份文件线程入口函数*/
int thread_recv_mid_server_backup_file(backup_sev_ctrl_t *p_sev_ctrl)
{
    pthread_t tid;

    if(pthread_create(&tid, NULL, run_recv_mid_server_backup_file, p_sev_ctrl) != 0)
    {
        ERR("create recv_mid_server_backup_file thread error:", strerror(errno));
        return -1;
    }
    return 0;
}
