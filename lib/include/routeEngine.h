#ifndef ROUTEENGINE_H
#define ROUTEENGINE_H

#include "logger.h"

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <string.h>
#include <unordered_map>

enum class Strategy {
    //随机算法
    Random,
    //轮询算法
    Polling,
    //源地址hash算法
    HashIP
};

//  路由策略接口
class RouteStrategy
{
public:
    using ptr = std::shared_ptr<RouteStrategy>;
    virtual ~RouteStrategy() {}

    virtual std::string select(std::vector<std::string>& list) = 0;
};

//  基于IP的一致哈希策略
class HashIPRouteStrategyImpl : public RouteStrategy
{
private:
    //  存虚拟结点和真实结点的映射
    std::unordered_map<std::string, std::vector<std::string>> virtualNodeMap;
    //  存储虚拟结点名称
    std::vector<std::string> virtualNodes;

public:
    std::string select(std::vector<std::string>& list)
    {
        //  更新一下虚拟结点的列表
        updateVirtualNodes(list);
        //  先拿到本机的IP地址
        static std::string host = GetLocalHost();
        if (host.empty())
        {
            LOG_ERR("GetLocalHost error!");
        }
        size_t hostHash = hashFunc(host);
        auto it = std::upper_bound(virtualNodes.begin(), virtualNodes.end(), std::to_string(hostHash));
        if (it == virtualNodes.end())
        {
            it = virtualNodes.begin();
        }
        std::string virtualNodeHash = *it;
        for (const auto& kvp : virtualNodeMap)
        {
            const std::vector<std::string>& virtualNode = kvp.second;
            if (std::find(virtualNode.begin(), virtualNode.end(), virtualNodeHash) != virtualNode.end()) 
            {
                //  返回找到的真实节点
                return kvp.first;
            }
        }
        //  找不到真实结点
        return "";
    }

private:
    //  获取本机的IP地址
    std::string GetLocalHost()
    {
        /*
        int sockfd;
        struct ifconf ifconf;
        struct ifreq *ifreq = nullptr;
        char buf[512];//缓冲区
        //初始化ifconf
        ifconf.ifc_len =512;
        ifconf.ifc_buf = buf;
        if ((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0)
        {
            return std::string{};
        }
        ioctl(sockfd, SIOCGIFCONF, &ifconf); //获取所有接口信息
        //接下来一个一个的获取IP地址
        ifreq = (struct ifreq*)ifconf.ifc_buf;
        for (int i=(ifconf.ifc_len/sizeof (struct ifreq)); i>0; i--)
        {
            if(ifreq->ifr_flags == AF_INET){ //for ipv4
                if (ifreq->ifr_name == std::string("eth0")) {
                    return std::string(inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr));
                }
                ifreq++;
            }
        }
        return std::string{};
        */
        struct addrinfo *answer, hint, *curr;
        bzero(&hint, sizeof(hint));
	    hint.ai_family = AF_INET;
	    hint.ai_socktype = SOCK_STREAM;

        int iRet = 0;
	    char szHostName[128] = {0};
	    iRet = gethostname(szHostName, sizeof(szHostName));
        if (iRet != 0)
	    {
		    LOG_ERR("gethostname error!");
	    }

        iRet = getaddrinfo(szHostName, NULL, &hint, &answer);
        if (iRet != 0)
        {
            LOG_ERR("getaddrinfo error!");
        }

        char pszIP[128] = {0};
        for (curr = answer; curr != NULL; curr = curr->ai_next)
	    {
	    	inet_ntop(AF_INET, &(((struct sockaddr_in *)(curr->ai_addr))->sin_addr), pszIP, 16);
	    }

	    freeaddrinfo(answer);
        
        return pszIP;
    }

    //  这个函数封装了哈希函数，用于计算字符串的哈希值
    size_t hashFunc(const std::string& str)
    {
        std::hash<std::string> hasher;
        return hasher(str);
    }

    //  更新虚拟结点列表
    void updateVirtualNodes(std::vector<std::string>& lists)
    {
        virtualNodeMap.clear();
        virtualNodes.clear();
        for (const auto& list : lists)
        {
            for (int i = 0; i < 3; ++i)
            {
                std::string virutalNodeName = list + "_" + std::to_string(i);
                std::string virutalNodeHash = std::to_string(hashFunc(virutalNodeName));
                virtualNodeMap[list].push_back(virutalNodeHash);
                virtualNodes.push_back(virutalNodeHash);
            }
        }
    }
};

//  路由均衡引擎
class RouteEngine
{
public:
    static typename RouteStrategy::ptr queryStrategy(Strategy routeStrategyEnum)
    {
        switch (routeStrategyEnum){
            case Strategy::Random:
                LOG_ERR("Random error!");
            case Strategy::Polling:
                LOG_ERR("Polling error!");
            case Strategy::HashIP:
                return s_hashIPRouteStrategy ;
            default:
                LOG_ERR("queryStrategy error!");
        }
    }

private:
    static typename RouteStrategy::ptr s_hashIPRouteStrategy;
};

typename RouteStrategy::ptr RouteEngine::s_hashIPRouteStrategy = std::make_shared<HashIPRouteStrategyImpl>();

#endif