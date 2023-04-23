# `RpcProvider`类

`RpcProvider`是一个框架专门为发布rpc服务的网络对象类。在服务端会用此类来注册服务，故`RpcProvider`类需要支持所有服务的发布。因此设计的`NotifyService`方法的参数必须要是这些服务的基类，也就是`google::protobuf::Service`。

因为protobuf只是提供了数据的序列化和反序列化还有RPC接口，并没有提供网络传输的相关代码。所以此项目用了muduo库实现网络传输。（后期可能会自己实现高并发网络传输）

同时还需要将服务注册到zookeeper上。

`RpcProvider`类源码

```c++
class RpcProvider{
public:
    //  这里是框架提供给外部使用的，可以发布RPC方法的接口
    void NotifyService(google::protobuf::Service *service);
    //  启动RPC服务结点，开始提供RPC远程网络调用服务
    void Run();
private:
    //  组合了EventLoop
    muduo::net::EventLoop m_eventLoop;
    //  服务类型信息
    struct ServiceInfo{
        google::protobuf::Service *m_service;    //  保存服务对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;   //  保存服务方法
    };
    //  存储注册成功的服务对象和其他服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;
    //  新的socket连接回调
    void OnConnection(const muduo::net::TcpConnectionPtr&);
    //  已建立连接用户的读写事件回调
    void OnMessage(const muduo::net::TcpConnectionPtr&, muduo::net::Buffer*, muduo::Timestamp);
    //  Closure的回调操作，用于序列化RPC的响应和网络发送
    void SendRpcResponse(const muduo::net::TcpConnectionPtr&, google::protobuf::Message*);

};
```

## `NotifyService`

1. 利用NotifyService发布服务

2. 从`*service`获取服务对象的描述信息，此接口由protobuf提供。

3. 从描述信息中获取到服务名字和服务对象service的方法和数量。

4. 遍历`service`获取服务对象指定虾苗的服务方法描述，并将其注册到`m_methodMap`上，例如`FriendServiceRpc/GetFriendsList`.

5. 最后将其加入服务对象集合`m_serviceMap`中。

   ```c++
   void RpcProvider::NotifyService(google::protobuf::Service *service){
       ServiceInfo service_info;
       const google::protobuf::ServiceDescriptor *perviceDesc = service->GetDescriptor();
       std::string service_name = perviceDesc->name();
       int methodCnt = perviceDesc->method_count();
   
       LOG_INFO("service_name:%s", service_name.c_str());
   
       for (int i = 0; i < methodCnt; ++i){
           const google::protobuf::MethodDescriptor* pmethodDesc =  perviceDesc->method(i);
           std::string method_name = pmethodDesc->name();
           service_info.m_methodMap.insert({method_name, pmethodDesc});
   
           LOG_INFO("method_name:%s", method_name.c_str());
       }
       service_info.m_service = service;
       m_serviceMap.insert({service_name, service_info});
   }
   ```

## 开启远程服务

1. 从RPC的框架中获取到IP和PORT，创建T CPserver对象

2. 设置muduo库的线程数量

3. 吧当前结点要发布的服务注册到zookeeper上，让客户端可以从zookeeper上发现服务

4. 启动网络服务

   ```c++
   void RpcProvider::Run(){
       std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
       uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
       muduo::net::InetAddress address(ip, port);
   
       muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");
   
       server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
       server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
       
       server.setThreadNum(4);
   
       ZkClient zkcli;
       zkcli.Start();
   
       for(auto &sp : m_serviceMap){
           std::string service_path = "/" + sp.first;
           zkcli.Create(service_path.c_str(), nullptr, 0);
           for(auto &mp : sp.second.m_methodMap){
               std::string method_name = service_path + "/" + mp.first;
               char method_path_data[128] = {0};
               sprintf(method_path_data, "%s:%d", ip.c_str(), port);
               zkcli.Create(method_name.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
           }
       }
   
       LOG_INFO("RpcProvider start service at ip::%s port:%d", ip.c_str(), port);
   
       server.start();
       m_eventLoop.loop();
   }
   ```

## 客户端发起请求服务端接收到

当接收到客户端的请求时。`OnMessage`回调函数会被调用，可以从客户端发过来的数据得知他想要调用那一个类的那一个方法以及其参数是什么。

为了防止TCP的粘包问题需要在自定义一个协议，本项目采用了将消息分为消息头和消息体，消息头包含此消息的总长度，每次都需要先读消息头，从而得知我们这次发过来的消息要读到那里。

1. 从网络上接收远程的RPC调用请求的字符串。
2. 从字符串中先读取前四个字节的内容，从而得知此次消息的长度。
3. 根据`header_size`读取数据头的原始字符串，反序列化数据得到RPC请求的详细消息。
4. 获取`service`对象和`method`对象。
5. 生成RPC方法调用请求`request`和相应`response`参数。
6. 在框架上根据远端的RPC请求调用当前的RPC结点方法。

```c++
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn, 
                            muduo::net::Buffer *buffer,
                             muduo::Timestamp){
    std::string recv_buf = buffer->retrieveAllAsString();

    uint32_t header_size = 0;
    recv_buf.copy((char *)&header_size, 4, 0);

    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str)){
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else{
        LOG_ERR("rec_header_str:%sparse error!", rpc_header_str.c_str());
        return;
    }

    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end()){
        LOG_ERR("%sis not exist", service_name.c_str());
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end()){
        LOG_ERR("%s:%sis not exist", service_name.c_str(), method_name.c_str());
        return;
    }

    google::protobuf::Service *service = it->second.m_service;  
    const google::protobuf::MethodDescriptor *method = mit->second; 

    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_str)){
        LOG_ERR("request parse error! content:%s", args_str.c_str());
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    //  给下面的method的方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr&, google::protobuf::Message*>(this,&RpcProvider::SendRpcResponse, conn, response);

    service->CallMethod(method, nullptr, request, response, done);
}
```

## 服务端处理完请求返回数据给客户端

当`service->CallMethod`执行完毕后，调用通过`done`绑定的回调函数。将服务器处理后的结果序列化，然后通过muduo网络库发回给客户端。

```c++
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message *response){
    std::string response_str;
    if(response->SerializeToString(&response_str)){
        conn->send(response_str);
    }
    else{
        LOG_ERR("serialize response_str error!");
    }
    conn->shutdown();  
}
```



