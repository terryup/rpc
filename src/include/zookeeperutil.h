#pragma once

#include <semaphore.h>
#include <string>
#include <zookeeper/zookeeper.h>

//  封装的客户端类
class ZkClient{
public:
    ZkClient();
    ~ZkClient();
    //  zkclient连接启动zkserver
    void Start();
    //  在zkserver上根据指定的path创建znode结点
    void Create(const char *path, const char *data, int datalen, int state = 0);
    //  根据参数指定的znode结点路径，获取znode结点的值
    std::string GetData(const char *path);

private:
    //  zk的客户端句柄
    zhandle_t *m_zhandle;
};