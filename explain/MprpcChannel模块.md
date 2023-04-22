# `MprpcChannel`模块

`MprpcChannel`模块继承于`google::protobuf::RpcChannel`他是一个RPC通道的抽象接口，表示一个到服务的通信线路，这个线路用于客户端远程调用服务端的方法。我们需要继承这个类并重写他的`CallMethod`方法。

```c++
class PROTOBUF_EXPORT RpcChannel {
 public:
  inline RpcChannel() {}
  virtual ~RpcChannel();

  virtual void CallMethod(const MethodDescriptor* method,
                          RpcController* controller, const Message* request,
                          Message* response, Closure* done) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(RpcChannel);
};
```

## `MprpcChannel`类

```c++
class MprpcChannel : public google::protobuf::RpcChannel{
public:
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                          google::protobuf::Message* response, google::protobuf::Closure* done);


};
```

## `CallMethod`方法

所有通过stub代理对象调用的RPC方法都走到了这里，统一做RPC方法调用的数据序列化和网络发送

### 获取客户端请求的方法和序列化

1. 从`CallMethod`的参数`method`获取`service_name`和`method_name`;

2. 将获取到的参数序列化为字符串，并获取他的长度。

3. 定义RPC的请求`header`.

4. 组织待发送的RPC请求的字符串

   ```c++
       const google::protobuf::ServiceDescriptor* sd = method->service();
       std::string service_name = sd->name(); 
       std::string method_name = method->name(); 
   
       uint32_t args_size = 0;
       std::string args_str;
       if(request->SerializeToString(&args_str)){
           args_size = args_str.size();
       }
       else{
           controller->SetFailed("serialize request error!");
           return;
       }
   
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
           controller->SetFailed("serialize rpc header error!");
           return;
       }
   
       std::string send_rpc_str;
       send_rpc_str.insert(0, std::string((char*)&header_size, 4)); 
       send_rpc_str += rpc_header_str; 
       send_rpc_str += args_str; 
   ```

### 使用TCP编程完成RPC方法的远程调用

因为`CallMethod`方法用于客户端远程调用服务端的方法，考虑到这里不需要高并发，故没有使用muduo网络库。

1. 通过 socket 函数在内核中创建一个套接字

2. RPC调用方法想要调用`service_name`的`method_name`服务，需要到zookeeper上查询该服务的所在的`host`信息。

3. 查询到了`mathod_name`服务的IP和PORT后，连接RPC服务结点

4. 发送RPC请求

5. 接收RPC请求的响应值

6. 最后反序列化服务器发回来的响应数据

   ```c++
       int client_fd = socket(AF_INET, SOCK_STREAM, 0);
       if(client_fd == -1){
           char errtxt[512] = {0};
           sprintf(errtxt, "create socket error! error:%d", errno);
           controller->SetFailed(errtxt);
           return;
       }
   
       ZkClient zkcli;
       zkcli.Start();
       std::string method_path = "/" + service_name + "/" + method_name;
       //  127.0.0.1:8080  
       std::string host_data = zkcli.GetData(method_path.c_str());
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
   
       if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
           close(client_fd);
           char errtxt[512] = {0};
           sprintf(errtxt, "connect error! error:%d", errno);
           controller->SetFailed(errtxt);
           return;
       }
   
       if(send(client_fd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1){
           close(client_fd);
           char errtxt[512] = {0};
           sprintf(errtxt, "send error! errno:%d", errno);
           controller->SetFailed(errtxt);
           return;
       }
   
       char recv_buf[1024] = {0};
       int recv_size = 0;
       if((recv_size = recv(client_fd, recv_buf, 1024, 0)) == -1){
           close(client_fd);
           char errtxt[512] = {0};
           sprintf(errtxt, "recv error! error:%d", errno);
           controller->SetFailed(errtxt);
           return;
       }
   
       if(!response->ParseFromArray(recv_buf, recv_size)){
           close(client_fd);
           char errtxt[512] = {0};
           sprintf(errtxt, "parse error! response_str%s", recv_buf);
           controller->SetFailed(errtxt);
           return;
       }
       close(client_fd);
   ```

   

