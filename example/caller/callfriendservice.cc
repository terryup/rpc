#include <iostream>
#include "mprpcapplication.h"
#include "friend.pb.h"
#include "mprpcchannel.h"


int main(int argc, char **argv) {
    //  整个程序启动以后，想使用mprpc框架来享用rpc服务调用，一定要先调用框架的初始化函数(只初始化1次)
    MprpcApplication::Init(argc, argv);
    
    //  演示调用远程发布的RPC方法Login
    fixbug::FriendServiceRpc_Stub stub(new MprpcChannel());
    //  rpc方法的请求参数
    fixbug::GetFriendsListRequest request;
    request.set_userid(1000);
    //  rpc方法的响应
    fixbug::GetFriendsListResponse response;
    //  发起rpc方法的调用，同步的rpc调用过程    MprpcChannel::callmethod
    MprpcController controller;
    stub.GetFriendsList(&controller, &request, &response, nullptr);   //  RpcChannel->RpcChannel::callMethod 集中来做所有rpc请求调用的参数序列化和网络发送

    //  一次rpc调用完成，读调用的结果
    //  bug 逻辑错误(这里) RPC方法执行过程中的状态 如果为false就报错，如果为true调用成功
    //  if(!controller.Failed()){
    if(controller.Failed()){
        std::cout << controller.ErrorText() << std::endl;
    }
    else{
        if(response.result().errcode() == 0){
        std::cout << "rpc GetFriendsList sucess!" << std::endl;
        int size = response.friends_size();
            for(int i = 0; i < size; ++i){
              std::cout << "index:" << (i + 1) << "name:" << response.friends(i) << std::endl;
            }
        }
        else{
        std::cout << "rpc GetFriendsList response:" << response.result().errmsg() << std::endl;
        }
    }
    

}