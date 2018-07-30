/*************************************************************************
	> File Name: scan_dir.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年02月07日 星期三 11时32分28秒
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
#include <sys/types.h>
#include <malloc.h>
#include "./scan_dir.h"


/*检查队列中文件是否被占用, 并设置占用标志*/
int check_used(user_t *p_user)
{
    /*为命令申请内存*/
    char *cmd = malloc(get_queue_node_number(&p_user->trans_que) * 1024);
    memset(cmd, 0x00, get_queue_node_number(&p_user->trans_que) * 1024);

    node_t *temp_node = p_user->trans_que.head;
    file_node_t *p_file = NULL;

    /*构造命令*/
    char *temp_cmd = cmd;
    sprintf(temp_cmd, "fuser -a ");
    temp_cmd += strlen("fuser -a ");


    while(temp_node)
    {
        p_file = (file_node_t *)temp_node->data;
        sprintf(temp_cmd, "%s ", p_file->filename);
        temp_cmd += (strlen(p_file->filename) + 1);
        temp_node = temp_node->next;
    }
    memcpy(temp_cmd, " 2>&1", strlen(" 2>&1"));

    FILE *fp = NULL;
    char resultBuff[1024] = {0};
    char *ptr = NULL;

    fp = popen(cmd, "r");
    if (fp == NULL) 
    {
        perror("popen");
        return -1;
    }
    printf("cmd: %s\n", cmd);

    temp_node = p_user->trans_que.head;
    while(fgets(resultBuff, 1024, fp) != NULL)
    {
        printf("resultBuff:[%s]\n", resultBuff);
        ptr = strchr(resultBuff, ':');
        ptr++;

        /*如果有被占用*/
        if(*ptr != '\n')
        {
            p_file = (file_node_t *)temp_node->data;
            p_file->occupy_flag = TRUE;
        }
        temp_node = temp_node->next;
        memset(resultBuff, 0x00, 1024);
    }

    pclose(fp);
    free(cmd);
    return 0;
}

/*浏览目录*/
int scan_dir(char *path, user_t *p_user)
{
    DIR *p_dir = NULL;
    if (path == NULL) 
    {
        printf("path is null!\n");
        return -1;
    }
    
    /*打开目录*/
    p_dir = opendir(path);
    if (p_dir == NULL)
    {
        printf("opendir error:[%s-%s]\n",path, strerror(errno));
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
        char buff[1024] = {0};

        /*将浏览到的文件名(仅有文件名，没有路径)和路径结合起来，组成全路径名*/
        sprintf(buff, "%s/%s", path, p_file->d_name);

        /*如果文件类型为4（目录），递归浏览目录*/
        if (p_file->d_type == 4) {
            scan_dir(buff, p_user);
            continue;
        }

        /*将文件添加进传输队列*/
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
        p_new_file->occupy_flag = FALSE;   /*文件初始为未占用状态*/

        que_push(&p_user->trans_que, (char *)p_new_file);
    }

    //关闭目录,释放资源
    closedir(p_dir);
    return 0;
}


/*浏览目录线程函数*/
void *run_scan(void *arg)
{
    printf("监控目录线程启动\n");

    /*分离线程*/
    pthread_detach(pthread_self());

    user_t *p_user = (user_t*)arg;

    /*创建接收目录*/
    if(mkdir("./backup_file_dirent", 0777) == -1)
    {
        if(errno != EEXIST)
        {
            perror("mkdir");
            return NULL;
        }
    }

    while(1)
    {
        while(1)
        {
            /*浏览目录开始*/
            scan_dir("./backup_file_dirent", p_user);

            /*如果目录不为空，即有文件需要备份*/
            if(can_pop(&p_user->trans_que))
            {
                break;
            }
            sleep(1);
        }

        /*检查队列中文件是否被占用，并设置占用标志*/
        check_used(p_user);

        /*唤醒上传文件线程*/
        condition_signal(&p_user->ready_upload_file);

        /*等待上传链表中文件上传完毕*/
        condition_wait(&p_user->ready_scan_dir);
    }
}

/*浏览目录线程入口函数*/
int thread_scan(user_t *p_user)
{
    pthread_t tid;

    /*创建浏览目录线程*/
    if(pthread_create(&tid, NULL, run_scan, p_user) < 0)
    {
        printf("create scan thread error:%s", strerror(errno));
        return -1;
    }
    return 0;
}
