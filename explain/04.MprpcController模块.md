# `MprpcController`模块

`MprpcContrller`模块继承于`google::protobuf::RpcController`，他声明于`service.h`文件下，而`RpcController`是一个抽象类，他的成员都是纯虚函数，需要我们自己重写实现，我们可以通过`RpcController`的方法得到RPC方法执行过程中的状态和RPC方法执行过程中的错误信息。

```c++
class PROTOBUF_EXPORT RpcController {
 public:
  inline RpcController() {}
  virtual ~RpcController();

  virtual void Reset() = 0;
  virtual bool Failed() const = 0;
  virtual std::string ErrorText() const = 0;
  virtual void StartCancel() = 0;
  virtual void SetFailed(const std::string& reason) = 0;
  virtual bool IsCanceled() const = 0;
  virtual void NotifyOnCancel(Closure* callback) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(RpcController);
};
```

## `RpcController`的API

这里只提及本项目涉及到的接口

`Reset()`可以将`RpcController`重新设定为初始状态，以便他可以被重用。他不能在RPC进行时调用。

```c++
virtual void Reset() = 0;
```

`Failed()`在一个调用结束以后，如果调用失败则返回`ture`。失败的原因取决于RPC的实现。`Failed()`不能在调用结束前被调用。如果返回`true`则响应消息的内容未被定义。

```c++
virtual bool Failed() const = 0;
```

如果`Failed()`返回为`true`此方法则返回一个用户可读的错误描述。

```c++
virtual std::string ErrorText() const = 0;
```

`StartCancel()`通知RPC系统，调用者希望RPC调用取消，RPC系统可以立刻取消，也可以等待一段时间后再取消调用，也可以不取消。如果调用被取消，`done`回调任然会被调用，`RpcController`会表明当时的调用失败。

```c++
virtual void StartCancel() = 0;
```

## `MprpcController`声明

```c++
class MprpcController : public google::protobuf::RpcController{
public:
    MprpcController();
    void Reset();
    bool Failed() const;
    std::string ErrorText() const;
    void SetFailed(const std::string& reason);

    //  目前未实现具体的功能
    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure* callback);


private:
    bool m_failed;  //  RPC方法执行过程中的状态
    std::string m_errText; //  RPC方法执行过程中的错误信息
};
```

## `MprpcController`实现

```c++
MprpcController::MprpcController(){
    m_failed = false;
    m_errText = "";
}

void MprpcController::Reset(){
    m_failed = false;
    m_errText = "";
}

bool MprpcController::Failed() const{
    return m_failed;
}

std::string MprpcController::ErrorText() const{
    return m_errText;
}

void MprpcController::SetFailed(const std::string& reason){
    m_failed = true;
    m_errText = reason;
}

//  目前未实现具体的功能
void MprpcController::StartCancel(){}
bool MprpcController::IsCanceled() const{return false;}
void MprpcController::NotifyOnCancel(google::protobuf::Closure* callback){}
```

