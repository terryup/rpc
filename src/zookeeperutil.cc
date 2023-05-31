#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include "routeEngine.h"
#include <iostream>
#include <semaphore.h>

//  全局的watcher观察器     zkserver给zkclient的通知
void global_watcher(zhandle_t *zh, int type, 
        int state, const char *path,void *watcherCtx){
    //  回调的消息类型是和会话相关的消息类型
    if(type == ZOO_SESSION_EVENT){
        //  zkclient和zkserver连接成功
        if(state == ZOO_CONNECTED_STATE){
            sem_t *sem = (sem_t*)zoo_get_context(zh);
            sem_post(sem);
        }
    }
}


ZkClient::ZkClient() : m_zhandle(nullptr){

}

ZkClient::~ZkClient(){
    if(m_zhandle != nullptr){
        zookeeper_close(m_zhandle); //  关闭句柄，释放资源
    }
}

//  zkclient连接启动zkserver
void ZkClient::Start(){
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;

    /*
    zookeeper_mt:多线程版本
    zookeeper的API客户端程序提供了三个线程
    API调用线程
    网络I/O线程     pthread_create  poll
    watcher回调线程     pthread_create
    */
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
    if(m_zhandle == nullptr){
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);

    sem_wait(&sem);
    std::cout << "zookeeper_init sucess!" << std::endl;
}

//  在zkserver上根据指定的path创建znode结点
void ZkClient::Create(const char *path, const char *data, int datalen, int state){
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
    //  先判断path表示的znode结点是否存在，如果存在就不再重复创建了
    flag = zoo_exists(m_zhandle, path, 0, nullptr);
    //  表示path的znode结点不存在
    if(flag == ZNONODE){
        //  创建指定path的znode结点
        flag = zoo_create(m_zhandle, path, data, datalen, 
                            &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if(flag == ZOK){
            std::cout << "znode create success... path:" << path << std::endl;
        }
        else{
            std::cout << "flag:" << flag << std::endl;
            std::cout << "znode create error... path:" << path << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

//  根据参数指定的znode结点路径，获取znode结点的值
std::string ZkClient::GetData(const char *path){
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if(flag != ZOK){
        std::cout << "get znode error... path:" << path << std::endl;
        return "";
    }
    else{
        return buffer;
    }
}

void ZkClient::GetChildren(const char *path, std::vector<std::string>& children)
{
    // 定义用于存储子节点列表的变量
    struct String_vector string_vector;

    // 获取子节点列表
    int result = zoo_get_children(m_zhandle, path, 0, &string_vector);
    if (result == ZOK) {
         // 遍历子节点列表
        for (int i = 0; i < string_vector.count; ++i) {
            children.push_back(string_vector.data[i]);
        }   
    }

    // 释放子节点列表的内存
    deallocate_String_vector(&string_vector);
}

std::string ZkClient::GetNode(std::string& path)
{
    std::vector<std::string> children;
    GetChildren(path.c_str(), children);

    //  这个函数只需要告诉我，我要选哪个node临时结点就可以了
    RouteStrategy::ptr strategy = RouteEngine::queryStrategy(Strategy::HashIP);
    LOG_INFO("Hash!");
    std::string node = strategy->select(children);
    //  std::string node = HashIPRouteStrategyImpl::select(children);

    std::string nodePath = path + "/" + node;
    std::string nodeData = GetData(nodePath.c_str());
    return nodeData;
}