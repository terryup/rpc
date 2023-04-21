#include "test.pb.h"
#include <iostream>
#include <string>

int main(){
    fixbug::GetFriendListsResponse rsp;
    fixbug::Resultcode *rc = rsp.mutable_result();
    rc->set_errcode(0);

    fixbug::User *users121 = rsp.add_users();
    users121->set_name("zhang san");
    users121->set_age(20);
    users121->set_sex(fixbug::User::MAN);

    fixbug::User *users242 = rsp.add_users();
    users242->set_name("li si");
    users242->set_age(22);
    users242->set_sex(fixbug::User::MAN);

    fixbug::User *users321 = rsp.add_users();
    users321->set_name("li si");
    users321->set_age(22);
    users321->set_sex(fixbug::User::MAN);

    std::cout << rsp.users_size() << std::endl;

    return 0;
}

int main1(){
    //封装了Login请求对象的数据
    fixbug::LoginRequest req;
    req.set_name("zixuan");
    req.set_pwd("1231231");

    //对象数据序列化->char*
    std::string send_str;
    if(req.SerializeToString(&send_str)){
        std::cout << send_str.c_str() << std::endl;
    }

    //从send_str反序列化一个Login请求对象
    fixbug::LoginRequest reqB;
    if(reqB.ParseFromString(send_str)){
        std::cout << reqB.name() << std::endl;
        std::cout << reqB.pwd() << std::endl;
    }


    return 0;
}

