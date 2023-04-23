# MPRPC

## 项目说明

本项目是在Linux环境下基于muduo、protobuf、zookeeper实现的RPC框架，可以通过本框架将本地方法调用重构成基于TCP网络通信的RPC远程方法调用。实现了远程调用，令方法在不同机器上运行，以达到分布式集群效果。

## 项目特点

- 通过muduo网络库实现高并发的网络通信模块

- 通过Protobuf实现RPC方法调用和参数的序列化与反序列化

- 通过ZooKeeper实现分布式一致性协调服务，由此提供服务注册和发现功能

- 实现了基于TCP传输的二进制协议、解决了粘包问题，并且能高效的数据传输服务器名、方法名及参数。

## 开发环境

- 操作系统：`Ubuntu 18.04.2`
- 编译器：`g++ 7.5.0`
- 编译器：`vscode`
- 版本控制：`git`
- 项目构建：`cmake 3.10.2`

## RPC通信原理

[]()

## 代码交互

![](/Users/zixuanhuang/Desktop/MPRPC.png)

## 构建项目

运行脚本，其会自动编译项目

```shell
./autobuild.sh
```

## 项目启动

### 启动ZooKeeper

需要在zookeeper上获得注册服务信息，就需要先启动zookeeper。我的zookeeper目录在`/home/zixuanhuang/bao/zookeeper-3.4.10`。

##### 在`bin`目录下有客户端和服务端的启动脚本，先启用zookeeper服务。

```shell
ubuntu% ./zkServer.sh start
```

此时可以在进程中找到zookeeper

```shell
ubuntu% ps -aux | grep zookeeper
zixuanh+ 15650  0.1  1.4 2913660 28456 ?       Sl   Apr20   1:40 java -Dzookeeper.log.dir=. -Dzookeeper.root.logger=INFO,CONSOLE -cp /home/zixuanhuang/bao/zookeeper-3.4.10/bin/../build/classes:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../build/lib/*.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../lib/slf4j-log4j12-1.6.1.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../lib/slf4j-api-1.6.1.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../lib/netty-3.10.5.Final.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../lib/log4j-1.2.16.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../lib/jline-0.9.94.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../zookeeper-3.4.10.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../src/java/lib/*.jar:/home/zixuanhuang/bao/zookeeper-3.4.10/bin/../conf: -Dcom.sun.management.jmxremote -Dcom.sun.management.jmxremote.local.only=false org.apache.zookeeper.server.quorum.QuorumPeerMain /home/zixuanhuang/bao/zookeeper-3.4.10/bin/../conf/zoo.cfg
zixuanh+ 17234  0.0  0.0  14432  1016 pts/25   S+   06:32   0:00 grep zookeeper
```

在启动客户端脚本，启动客户端是为了检测是否能为RPC框架插入新的服务信息

```shell
ubuntu% ./zkCli.sh
```

### MPRPC框架示例

#### 在项目文件的`bin`目录下有提供者和消费者两个文件，分别启动

##### 启动提供者

可以看到启动后zookeeper已经初始化成功

```shell
ubuntu% ./provider -i test.conf
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@726: Client environment:zookeeper.version=zookeeper C client 3.4.10
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@730: Client environment:host.name=ubuntu
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@737: Client environment:os.name=Linux
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@738: Client environment:os.arch=5.4.0-146-generic
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@739: Client environment:os.version=#163~18.04.1-Ubuntu SMP Mon Mar 20 15:02:59 UTC 2023
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@747: Client environment:user.name=zixuanhuang
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@755: Client environment:user.home=/home/zixuanhuang
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@log_env@767: Client environment:user.dir=/home/zixuanhuang/mprpc/bin
2023-04-21 06:41:54,482:17510(0x7f8aceefca00):ZOO_INFO@zookeeper_init@800: Initiating client connection, host=127.0.0.1:2181 sessionTimeout=30000 watcher=0x55fb4cb3ab90 sessionId=0 sessionPasswd=<null> context=(nil) flags=0
2023-04-21 06:41:54,482:17510(0x7f8ac7fff700):ZOO_INFO@check_events@1728: initiated connection to server [127.0.0.1:2181]
2023-04-21 06:41:54,485:17510(0x7f8ac7fff700):ZOO_INFO@check_events@1775: session establishment complete on server [127.0.0.1:2181], sessionId=0x1879d16838c0041, negotiated timeout=30000
zookeeper_init sucess!
```

回到zookeeper查看确认已经注册成功

```shell
[zk: localhost:2181(CONNECTED) 2] ls /FriendServiceRpc/GetFriendsList
[]
[zk: localhost:2181(CONNECTED) 3] 
```

##### 启动消费者

可以看到非常多zookeeper 相关日志信息，这里看到他打印了`rpc GetFriendsList sucess!`。并且已经获取到了信息，说明RPC方法调用成功。

```shell
ubuntu% ./consumer -i test.conf
==============================================
header_size:36
rpc_header_str:
FriendServiceRpcGetFriendsList
service_name:FriendServiceRpc
method_name:GetFriendsList
args_str?
==============================================
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@726: Client environment:zookeeper.version=zookeeper C client 3.4.10
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@730: Client environment:host.name=ubuntu
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@737: Client environment:os.name=Linux
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@738: Client environment:os.arch=5.4.0-146-generic
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@739: Client environment:os.version=#163~18.04.1-Ubuntu SMP Mon Mar 20 15:02:59 UTC 2023
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@747: Client environment:user.name=zixuanhuang
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@755: Client environment:user.home=/home/zixuanhuang
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@log_env@767: Client environment:user.dir=/home/zixuanhuang/mprpc/bin
2023-04-21 06:40:59,603:17505(0x7f4b1d4e5740):ZOO_INFO@zookeeper_init@800: Initiating client connection, host=127.0.0.1:2181 sessionTimeout=30000 watcher=0x5621e198c446 sessionId=0 sessionPasswd=<null> context=(nil) flags=0
2023-04-21 06:40:59,604:17505(0x7f4b1b484700):ZOO_INFO@check_events@1728: initiated connection to server [127.0.0.1:2181]
2023-04-21 06:40:59,608:17505(0x7f4b1b484700):ZOO_INFO@check_events@1775: session establishment complete on server [127.0.0.1:2181], sessionId=0x1879d16838c0040, negotiated timeout=30000
zookeeper_init sucess!
2023-04-21 06:40:59,610:17505(0x7f4b1d4e5740):ZOO_INFO@zookeeper_close@2527: Closing zookeeper sessionId=0x1879d16838c0040 to [127.0.0.1:2181]

rpc GetFriendsList sucess!
index:1name:gao yang
index:2name:gao yi
index:3name:liu yang
```

## 项目讲解

#### [01.环境搭建](https://github.com/terryup/rpc/blob/mprpc/explain/01.%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BA.md)

#### [02.MprpcConfig模块](https://github.com/terryup/rpc/blob/mprpc/explain/02.MprpcConfig%E6%A8%A1%E5%9D%97.md)

#### [**03.RpcProvider模块**](https://github.com/terryup/rpc/blob/mprpc/explain/03.RpcProvider%E6%A8%A1%E5%9D%97.md)

#### [**04.MprpcController模块**](https://github.com/terryup/rpc/blob/mprpc/explain/04.MprpcController%E6%A8%A1%E5%9D%97.md)

#### [**05.MprpcChannel模块**](https://github.com/terryup/rpc/blob/mprpc/explain/05.MprpcChannel%E6%A8%A1%E5%9D%97.md)

#### [**06.Logger模块**](https://github.com/terryup/rpc/blob/mprpc/explain/06.Logger%E6%A8%A1%E5%9D%97.md)

#### [**07.ZooKeeper模块**](https://github.com/terryup/rpc/blob/mprpc/explain/07.ZooKeeper%E6%A8%A1%E5%9D%97.md)

## 项目优化

暂未想好
