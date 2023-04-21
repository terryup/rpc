#include <iostream>
#include <string>
#include <vector>
#include "friend.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include "logger.h"

class FriendService : public fixbug::FriendServiceRpc{
public:

    std::vector<std::string> GetFriendsList(uint32_t userid){
        std::cout << "do GetFriendsList serivce! userid:" << userid << std::endl;
        std::vector<std::string> vec;
        vec.push_back("gao yang");
        vec.push_back("gao yi");
        vec.push_back("liu yang");
        return vec;
    }

    //  重写基类方法
    void GetFriendsList(::google::protobuf::RpcController* controller,
                       const ::fixbug::GetFriendsListRequest* request,
                       ::fixbug::GetFriendsListResponse* response,
                       ::google::protobuf::Closure* done){

        uint32_t userid = request->userid();
        
        std::vector<std::string> friendList = GetFriendsList(userid);
        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        for(std::string &name : friendList){
            std::string *p = response->add_friends();
            *p = name;
        }
        done->Run();
    }
};

int main(int argv, char** argc){

    LOG_INFO("first log message!");
    LOG_ERR("%s:%s:%d", __FILE__, __FUNCTION__, __LINE__);

    //  调用框架的初始化操作    provider -i config.conf
    MprpcApplication::Init(argv, argc);

    //  provider是一个PRC网络服务对象。把FriendService对象发布到RPC结点上
    RpcProvider provider;
    provider.NotifyService(new FriendService());

    //  启动一个PRC服务结点     Run以后，进程进入阻塞状态，等待远程的RPC调用请求
    provider.Run();

    return 0;
}