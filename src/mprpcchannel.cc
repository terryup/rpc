#include "mprpcchannel.h"
#include "rpcheader.pb.h"
#include "mprpcapplication.h"
#include "mprpccontroller.h"
#include "zookeeperutil.h"
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>


/*
header_size + service_name + method_name + args_name + args
*/
//  所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据序列化和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                          google::protobuf::Message* response, google::protobuf::Closure* done){

    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();  //  service_name
    std::string method_name = method->name();   //  method_name

    //  获取参数的序列化字符串长度  args_size
    uint32_t args_size = 0;
    std::string args_str;
    if(request->SerializeToString(&args_str)){
        args_size = args_str.size();
    }
    else{
        //  std::cout << "serialize request error!" << std::endl;
        controller->SetFailed("serialize request error!");
        return;
    }

    //  定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if(rpcHeader.SerializeToString(&rpc_header_str)){
        header_size = rpc_header_str.size();
    }
    else{
        //  std::cout << "serialize rpc header error!" << std::endl;
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    //  组织待发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));    //  header_size
    send_rpc_str += rpc_header_str; //  rpcheader
    send_rpc_str += args_str;   //  args

    //  打印调试信息
    std::cout << "==============================================" << std::endl;
    std::cout << "header_size:" << header_size << std::endl;
    std::cout << "rpc_header_str:" << rpc_header_str << std::endl;
    std::cout << "service_name:" << service_name << std::endl;
    std::cout << "method_name:" << method_name << std::endl;
    std::cout << "args_str:" << args_str << std::endl;
    std::cout << "==============================================" << std::endl;

    //  使用TCP编程完成RPC方法的远程调用
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd == -1){
        //  std::cout << "create socket error! error:" << errno << std::endl;
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! error:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  读配置文件rpcserver的信息
    //  std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    //  uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    //  rpc调用方法想调用service_name的method_name服务，需要查询zk上该服务所在的host信息
    ZkClient zkcli;
    zkcli.Start();
    //  /UserServiceRpc/Login   bug=>漏了一个“/”  
    std::string method_path = "/" + service_name + "/" + method_name;
    //  127.0.0.1:8080      bug=>method_path写成了mathod_name
    //  std::string host_data = zkcli.GetData(method_path.c_str()); -- 2023.5.30
    std::string host_data = zkcli.GetNode(method_path);
    if(host_data == ""){
        controller->SetFailed(method_path + "is not exist!");
        return;
    }
    int idx = host_data.find(":");
    if(idx == -1){
        controller->SetFailed(method_path + "address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    //  连接RPC服务结点
    if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        //  std::cout << "connect error! error:" << errno << std::endl;
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! error:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  发送RPC请求         
    //  这里有个bug，要吧strlen改成send_rpc_str.size()
    //  原因strlen在遇到null也就是'\0'就会停下来但是不包括'\0'
    //  那么使用strlen获取的字符串长度可能会小于实际需要发送的字节数，导致数据发送不完整或错误
    //  而size()会返回std::string容器中的真实长度包括'\0'，也就是不以'\0'结尾
    if(send(client_fd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1){
        //  std::cout << "send error! errno:" << errno << std::endl;
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  接收RPC请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if((recv_size = recv(client_fd, recv_buf, 1024, 0)) == -1){
        //  std::cout << "recv error! error:" << errno << std::endl;
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! error:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  反序列化RPC调用的响应数据
    //  有bug出现问题,recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败
    //  std::string response_str(recv_buf, 0, recv_size);  
    //  if(!response->ParseFromString(response_str)){
    if(!response->ParseFromArray(recv_buf, recv_size)){
        //  std::cout << "parse error! response_str:" << recv_buf << std::endl;
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "parse error! response_str%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
    close(client_fd);

}