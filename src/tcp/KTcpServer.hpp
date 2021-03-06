#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include <sstream>
#include "thread/KEventObject.h"
#include "tcp/KTcpNetwork.h"
#include "util/KStringUtility.h"

/**
tcp服务端类
**/

namespace klib {

    template<typename MessageType>
    class KTcpServer :public KTcpNetwork<MessageType>
    {
    public:
        /************************************
        * Method:    启动服务端
        * Returns:   成功返回true失败返回false
        * Parameter: hosts 格式："1.1.1.1:1234,2.2.2.2:2345"
        * Parameter: needAuth  是否需要授权
        *************************************/
        bool Start(const std::string& hosts, bool needAuth = false)
        {
            std::vector<std::string> brokers;
            klib::KStringUtility::SplitString(hosts, ",", brokers);
            std::vector<std::string>::const_iterator it = brokers.begin();
            while (it != brokers.end())
            {
                const std::string& broker = *it;
                std::vector<std::string> ipandport;
                klib::KStringUtility::SplitString(broker, ":", ipandport);
                if (ipandport.size() != 2)
                {
                    printf("Invalid broker:[%s]\n", broker.c_str());
                }
                else
                {
                    uint16_t port = atoi(ipandport[1].c_str());
                    if (port == 0)
                        printf("Invalid broker:[%s]\n", broker.c_str());
                    else
                        m_hostip[ipandport[0]] = port;
                }
                ++it;
            }

            if (m_hostip.empty())
            {
                printf("Invalid brokers:[%s]\n", hosts.c_str());
                return false;
            }

            m_it = m_hostip.begin();
            return KTcpNetwork<MessageType>::Start(m_it->first, m_it->second, true, needAuth);
        }

    protected:
        /************************************
        * Method:    获取配置
        * Returns:   返回配置
        *************************************/
        virtual std::pair<std::string, uint16_t> GetConfig() const
        {
            if (KTcpNetwork<MessageType>::IsConnected())
                return *m_it;
            else
            {
                if (++m_it != m_hostip.end())
                {
                    return *m_it;
                }
                else
                {
                    m_it = m_hostip.begin();
                    return *m_it;
                }
            }
        }

    private:
        std::map<std::string, uint16_t> m_hostip;
        mutable std::map<std::string, uint16_t>::const_iterator m_it;
    };
};
