/*************************************************************************
	> File Name: common.c
	> Author: WishSun
	> Mail: WishSun_Cn@163.com
	> Created Time: 2018年03月06日 星期二 20时28分21秒
 ************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../../common_include/common.h"

/*对发送数据或向文件中写入数据进行封装
 * @fd : 套接字描述符或文件描述符 
 * @buff: 将要写入的数据的地址
 * @len: 将要写入的数据的长度
 *
 * 返回值: 成功返回0
 *         失败返回-1
 */
int send_data(int fd, void *buff, int len)
{
    int s_len = 0;
    int ret;

    while(s_len < len)
    {
        //将接受的数据放到已有数据后边: buff + r_len
        //接收长度为总长度len - 已经接收的长度r_len
        ret = write(fd, buff + s_len, len - s_len);
        if(ret <= 0)
        {
            if(errno == EAGAIN || errno == EINTR)
            {
                continue;
            }
            perror("write");
            return -1;
        }
        s_len += ret;
    }
    return 0;
}

/*对接收数据或从文件中读取数据进行封装
 * @fd : 套接字描述符或文件描述符 
 * @buff: 将要写入的数据的地址
 * @len: 将要写入的数据的长度
 *
 * 返回值: 成功返回0
 *         失败返回-1
 */
int recv_data(int fd, void *buff, int len)
{
    int r_len = 0;
    int ret;

    while(r_len < len)
    {
        //将接受的数据放到已有数据后边: buff + r_len
        //接收长度为总长度len - 已经接收的长度r_len
        ret = read(fd, buff + r_len, len - r_len);
        if(ret <= 0)
        {
            if(ret == 0)
            {
                return 1;
            }
            else if(errno == EAGAIN || errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        r_len += ret;
    }
    return 0;
}

/*设置描述符为非阻塞 */
void setnonblock(int fd)
{
    int flag;
    flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


/*创建套接字并绑定IP地址和端口*/
int create_listen_socket(char *p_sev_addr, int sev_port)
{
    int lst_fd, len;
    struct sockaddr_in sev_addr;

    if((lst_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("create");
        return -1;
    }

    /*设置socket地址重用，目的防止程序主动退出后出现time_wait状态，新启动程序无法绑定端口地址信息*/
    int option = 1;
    setsockopt(lst_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int));

    /*配置服务器监听地址和端口*/
    sev_addr.sin_family = AF_INET;
    sev_addr.sin_port = htons(sev_port);
    sev_addr.sin_addr.s_addr = inet_addr(p_sev_addr);

    if(bind(lst_fd, (struct sockaddr *)&sev_addr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind");
        return -1;
    }

    listen(lst_fd, MAX_LISTEN);

    /*将socket设置为非阻塞*/
    setnonblock(lst_fd);

    return lst_fd;
}

/*与服务器建立连接*/
int connect_server(char *p_sev_addr, int sev_port)
{
    int sock_fd;
    struct sockaddr_in sev_addr;
    int len = sizeof(struct sockaddr_in);


    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return -1;
    }

    //配置服务器地址和端口
    sev_addr.sin_family = AF_INET;
    sev_addr.sin_port = htons(sev_port);
    sev_addr.sin_addr.s_addr = inet_addr(p_sev_addr);

    //连接服务器
    if(connect(sock_fd, (struct sockaddr *)&sev_addr, len) == -1)
    {
        perror("connect");
        return -1;
    }

    return sock_fd;
}
