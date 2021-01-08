#include "KActivemqConsumer.h"

namespace thirdparty {
    KActivemqConsumer::KActivemqConsumer(const std::string& brokerURI, const std::string& destURI)
        : m_connection(NULL),
        m_session(NULL),
        m_destination(NULL),
        m_brokerURI(brokerURI),
        m_destURI(destURI),
        m_state(CSDisconnected)
    {
    }

    void KActivemqConsumer::Initialize()
    {
        activemq::library::ActiveMQCPP::initializeLibrary();
    }

    void KActivemqConsumer::Shutdown()
    {
        activemq::library::ActiveMQCPP::shutdownLibrary();
    }

    void KActivemqConsumer::Cleanup()
    {
        // Close open resources.
        try {
            if (m_connection != NULL)
            {
                m_connection->stop();
                m_connection->close();
                printf("<%s> Dest:[%s], connection closed.\n"
                    , __FUNCTION__, m_destURI.c_str());
            }
        }
        catch (CMSException& e) {}

        try {
            if (m_session != NULL)
            {
                m_session->close();
                printf("<%s> Dest:[%s], session closed.\n"
                    , __FUNCTION__, m_destURI.c_str());
            }
        }
        catch (CMSException& e) {}

        try {
            if (m_consumer != NULL)
            {
                m_consumer->close();
                printf("<%s> Consumer stopping.\n", __FUNCTION__);
            }
        }
        catch (CMSException& e) {}

        // Destroy resources.
        Release(m_destination);
        Release(m_session);
        Release(m_connection);
        Release(m_consumer);
        printf("<%s> Consumer stopped.\n", __FUNCTION__);
        m_state = CSDisconnected;
    }

    bool KActivemqConsumer::Start()
    {
        ActiveMQConnectionFactory* factory = NULL;
        bool rc = false;
        switch (m_state)
        {
        case CSException:
        {
            Cleanup();
        }
        case CSDisconnected:
        {
            try {

                printf("<%s> Consumer Starting.\n", __FUNCTION__);
                // Create a ConnectionFactory
                factory = new ActiveMQConnectionFactory(m_brokerURI);
                RedeliveryPolicy* policy = factory->getRedeliveryPolicy();
                policy->setMaximumRedeliveries(3);
                m_connection = factory->createConnection();

                ActiveMQConnection* amqConnection = dynamic_cast<ActiveMQConnection*>(m_connection);
                if (amqConnection != NULL) {
                    amqConnection->addTransportListener(this);
                }
                else
                {
                    printf("<%s> Consumer Create connection failed.\n", __FUNCTION__);
                    return false;
                }

                m_connection->start();
                m_connection->setExceptionListener(this);
                m_session = m_connection->createSession(Session::INDIVIDUAL_ACKNOWLEDGE);
                m_destination = m_session->createQueue(m_destURI);
                m_consumer = m_session->createConsumer(m_destination);
                m_consumer->setMessageListener(this);
                m_session->recover();
                printf("<%s> Consumer Started.\n", __FUNCTION__);
                m_state = CSConnected;
                rc = true;
            }
            catch (CMSException& e)
            {
                printf("<%s> Consumer Exception:[%s].\n", 
                    __FUNCTION__, e.getStackTraceString().c_str());
                m_state = CSException;
            }
            break;
        }
        case CSConnected:
        {
            rc = true;
            break;
        }
        default:
            ;
        }
        if (factory)
            delete factory;
        return rc;
    }

    void KActivemqConsumer::Stop()
    {
        switch (m_state)
        {
        case CSException:
        case CSConnected:
        {
            Cleanup();
            break;
        }
        default:
            break;
        }
    }

    void KActivemqConsumer::onMessage(const Message* msg)
    {
        bool rc = true;
        const TextMessage* tmsg = dynamic_cast<const TextMessage*>(msg);
        if (tmsg != NULL)
            rc = OnText(tmsg);
        else// �Ƿ���Ϣ
            printf("<%s> Not text message.\n", __FUNCTION__);

        if (rc)
            msg->acknowledge();
        else
            throw decaf::lang::exceptions::RuntimeException();
    }
};