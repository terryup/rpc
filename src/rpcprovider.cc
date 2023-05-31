#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"

/*
service_name -> service描述
                 -> service* 记录服务对象
                 method_name  ->  method方法对象

*/
//  这里是框架提供给外部使用的，可以发布RPC方法的接口
void RpcProvider::NotifyService(google::protobuf::Service *service){
    ServiceInfo service_info;

    //  获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor *perviceDesc = service->GetDescriptor();
    //  获取服务的名字
    std::string service_name = perviceDesc->name();
    //  获取服务对象service的方法和数量
    int methodCnt = perviceDesc->method_count();

    //  std::cout << "service_name:" << service_name << std::endl;
    LOG_INFO("service_name:%s", service_name.c_str());

    for (int i = 0; i < methodCnt; ++i){
        //  获取服务对象指定下标的服务方法的描述(抽象描述)  UserService     Login
        const google::protobuf::MethodDescriptor* pmethodDesc =  perviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});

        //  std::cout << "method_name:" << method_name << std::endl;
        LOG_INFO("method_name:%s", method_name.c_str());
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

//  启动RPC服务结点，开始提供RPC远程网络调用服务
void RpcProvider::Run(){
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    //std::vector<std::string> ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    //  创建TPCserver对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    //  绑定连接回调和消息读写回调方法  分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1,
             std::placeholders::_2, std::placeholders::_3));
    
    //设置muduo库的线程数量
    server.setThreadNum(4);

    //  吧当前结点要发布的服务全都注册到zk上，让rpc client可以从zk上发现服务
    //  session timeout  30s     zkclient 网络I/O线程 1/3 * timeout 时间发送ping消息
    ZkClient zkcli;
    zkcli.Start();
    //  service_name 为永久性结点   method_name为临时性结点
    for(auto &sp : m_serviceMap){
        //  /service_name
        std::string service_path = "/" + sp.first;
        zkcli.Create(service_path.c_str(), nullptr, 0);
        for(auto &mp : sp.second.m_methodMap){
            //  /service_name/method_name   /UserServiceRpc/Login  存储当前这个rpc服务结点主机的ip和port
            //  // 创建永久性节点：/UserServiceRpc/Login  --- 2023.5.30
            std::string method_path = service_path + "/" + mp.first;
            zkcli.Create(method_path.c_str(), nullptr, 0);

            //  出现bug最后那里是mp
            //  std::string method_name = service_path + "/" + sp.first;

            //  创建临时节点：/UserServiceRpc/Login/Node1 --- 2023.5.30
            std::string node_path = method_path + "/" + serverName;
            //  std::string method_name = service_path + "/" + mp.first;
            //  char method_path_data[128] = {0};
            char node_path_data[128] = {0};
            //  sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            sprintf(node_path_data, "%s:%d", ip.c_str(), port);
            //  ZOO_EPHEMERAL表示znode是一个临时性结点
            //  zkcli.Create(method_name.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
            zkcli.Create(node_path.c_str(), node_path_data, strlen(node_path_data), ZOO_EPHEMERAL);
        }
    }

    //  std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;
    LOG_INFO("RpcProvider start service at ip::%s port:%d", ip.c_str(), port);

    //启动网络服务
    server.start();
    m_eventLoop.loop();
}

//  新的socket连接回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn){
    if(!conn->connected()){
        //  和rpc  client的连接断开了
        conn->shutdown();
    }
}

/*
在框架内部，RpcProvider和RpcConsumer协商好之间通信的protobuf数据类型
service_name    method_name     args
UserServiceLoginzhang san123456
定义proto的message类型，进行数据头的序列化和反序列化
service_name  method_name  args_size

header_size(4个字节) + header_str + args_str
10 "10"
10000 "10000"
std::string insert和copy方法
*/
//  已建立连接用户的读写事件回调    如果远程有一个rpc服务的调用请求，那么OnMessage方法就会响应
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn, 
                            muduo::net::Buffer *buffer,
                             muduo::Timestamp){
    //  网络上接收远程RPC调用请求的字符流   Login  args
    std::string recv_buf = buffer->retrieveAllAsString();

    //  从字符流中读取前四个字节的内容
    uint32_t header_size = 0;
    recv_buf.copy((char *)&header_size, 4, 0);

    //  根据header_size读取数据头的原始字符流,反序列化数据得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str)){
        //  数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else{
        //  数据头反序列化失败
        //  std::cout << "rec_header_str:" << rpc_header_str << "parse error!" << std::endl;
        LOG_ERR("rec_header_str:%sparse error!", rpc_header_str.c_str());
        return;
    }

    //  获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    //  打印调试信息
    std::cout << "==============================================" << std::endl;
    std::cout << "header_size:" << header_size << std::endl;
    std::cout << "rpc_header_str:" << rpc_header_str << std::endl;
    std::cout << "service_name:" << service_name << std::endl;
    std::cout << "method_name:" << method_name << std::endl;
    std::cout << "args_str:" << args_str << std::endl;
    std::cout << "==============================================" << std::endl;

    //  获取serivce对象和method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end()){
        //  std::cout << service_name << "is not exist" << std::endl;
        LOG_ERR("%sis not exist", service_name.c_str());
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end()){
        //  std::cout << service_name << ":" << method_name << "is not exist" << std::endl;
        LOG_ERR("%s:%sis not exist", service_name.c_str(), method_name.c_str());
        return;
    }

    google::protobuf::Service *service = it->second.m_service;  //  获取serivce对象   new UserSerivce
    const google::protobuf::MethodDescriptor *method = mit->second; //  获取method对象  Login

    //  生成rpc方法调用的请求request和相应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_str)){
        //  std::cout << "request parse error! content:" << args_str << std::endl;
        LOG_ERR("request parse error! content:%s", args_str.c_str());
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    //  给下面的method的方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, 
                                                                    const muduo::net::TcpConnectionPtr&,
                                                                    google::protobuf::Message*>
                                                                    (this, 
                                                                    &RpcProvider::SendRpcResponse, 
                                                                    conn, response);

    //  在框架上根据远端的RPC请求，调用当前RPC结点的方法
    //  new UserService().Login(controller, request, response, done)
    service->CallMethod(method, nullptr, request, response, done);
}

//  Closure的回调操作，用于序列化RPC的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message *response){
    std::string response_str;
    //  response序列化
    if(response->SerializeToString(&response_str)){
        //  序列化成功后，通过网络吧RPC方法执行结果发送回RPC的调用方
        conn->send(response_str);
    }
    else{
        //  std::cout << "serialize response_str error!" << std::endl;
        LOG_ERR("serialize response_str error!");
    }
    conn->shutdown();   //  模拟http的短链接服务，有rpcprovider主动断开链接
}

void RpcProvider::setServerName(std::string name)
{
    serverName = name;
}