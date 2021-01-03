#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpConnection.hpp"
#include "tcp/KTcpNetwork.h"
#include "util/KEndian.h"
namespace klib
{
#define MaxRegisterCount 32765
    struct KModbusMessage :public KTcpMessage
    {
    public:
        enum
        {
            ModbusNull, ModbusRequest, ModbusResponse
        };

        KModbusMessage(int mt = ModbusResponse)
            :messageType(mt), seq(0), ver(0), len(0), dev(0),
            func(0), saddr(0), count(0), ler(0) {}

        virtual size_t GetPayloadSize() const { return len; }
        virtual size_t GetHeaderSize() const { return sizeof(seq) + sizeof(ver) + sizeof(len); }
        virtual bool IsValid() { return (ver == 0x0 && ((dev == 0xff && func == 0x04) || (dev == 0x01 && func == 0x03))); }
        virtual void Clear() { len = 0; dev = 0; func = 0; }

        bool ToRequest()
        {
            if (sizeof(saddr) + sizeof(count) == payload.GetSize())
            {
                uint8_t* src = (uint8_t*)payload.GetData();
                size_t offset = 0;
                KEndian::FromNetwork(src + offset, saddr);
                offset += sizeof(saddr);
                KEndian::FromNetwork(src + offset, count);
                return count <= MaxRegisterCount;
            }
            return false;
        }

        void ToResponse()
        {
            char* src = payload.GetData();
            size_t offset = 0;
            ler = src[offset++];
            size_t lsz = payload.GetSize() - offset;
            if (lsz > 0)
            {
                dat.Release();
                dat = KBuffer(lsz);
                dat.ApendBuffer(src + offset, lsz);
            }
        }
    public:
        uint16_t seq;
        uint16_t ver;
        uint16_t len;

        uint8_t dev;
        uint8_t func;

        KBuffer payload; // response payload, need to release manually

        uint16_t saddr; // request
        uint16_t count; // request

        uint8_t ler;// response len or error code
        KBuffer dat;// response, need to release manually

        int messageType;
    };

    template<>
    int ParsePacket(const KBuffer& dat, KModbusMessage& msg, KBuffer& left);

    class KTcpModbus :public KTcpConnection<KModbusMessage>
    {
    public:
        KTcpModbus(KTcpNetwork<KModbusMessage>* poller)
            :m_seq(0), m_func(0x04), m_dev(0xff),
            KTcpConnection<KModbusMessage>(poller)
        {

        }

        void SetDev(uint8_t dev) { m_dev = dev; }
        void SetFunc(uint8_t func) { m_func = func; }

        virtual KModbusMessage Request(uint16_t addr, uint16_t count)
        {
            KModbusMessage msg(KModbusMessage::ModbusRequest);
            uint16_t s = m_seq++;
            if (s == 0xffff)
                m_seq = 0;
            msg.seq = s;
            msg.ver = 0;
            msg.len = 6;
            msg.dev = m_dev;
            msg.func = m_func;
            msg.saddr = addr;
            msg.count = count;
            return msg;
        }

        virtual KModbusMessage Response(uint16_t seq, const KBuffer& r)
        {
            KModbusMessage msg(KModbusMessage::ModbusResponse);
            msg.seq = seq;
            msg.ver = 0;
            msg.len = 3 + r.GetSize();
            msg.dev = m_dev;
            msg.func = m_func;
            msg.ler = r.GetSize();
            msg.dat = r;
            return msg;
        }

        virtual void Serialize(const KModbusMessage& msg, KBuffer& result) const
        {
            switch (msg.messageType)
            {
            case KModbusMessage::ModbusRequest:
            {
                // 00 01 00 00 00 06 ff 04 00 01 00 01
                result = KBuffer(12);
                size_t offset = 0;
                uint8_t* dst = (uint8_t*)result.GetData();
                KEndian::ToBigEndian(msg.seq, dst + offset);
                offset += sizeof(msg.seq);
                KEndian::ToBigEndian(msg.ver, dst + offset);
                offset += sizeof(msg.ver);
                KEndian::ToBigEndian(msg.len, dst + offset);
                offset += sizeof(msg.len);

                dst[offset++] = msg.dev;
                dst[offset++] = msg.func;

                KEndian::ToBigEndian(msg.saddr, dst + offset);
                offset += sizeof(msg.saddr);
                KEndian::ToBigEndian(msg.count, dst + offset);
                offset += sizeof(msg.count);
                result.SetSize(offset);
                break;
            }
            case KModbusMessage::ModbusResponse:
            {
                size_t sz = msg.dat.GetSize();
                result = KBuffer(9 + sz);
                size_t offset = 0;
                uint8_t* dst = (uint8_t*)result.GetData();
                KEndian::ToBigEndian(msg.seq, dst + offset);
                offset += sizeof(msg.seq);
                KEndian::ToBigEndian(msg.ver, dst + offset);
                offset += sizeof(msg.ver);
                KEndian::ToBigEndian(msg.len, dst + offset);
                offset += sizeof(msg.len);

                dst[offset++] = msg.dev;
                dst[offset++] = msg.func;
                dst[offset++] = msg.ler;

                memcpy(dst + offset, msg.dat.GetData(), sz);
                offset += sz;
                result.SetSize(offset);
                break;
            }
            default:
                break;
            }
        }

    protected:
        virtual void OnMessage(const std::vector<KModbusMessage>& msgs)
        {
            std::vector<KModbusMessage>& ms = const_cast<std::vector<KModbusMessage>&>(msgs);
            std::vector<KModbusMessage>::iterator it = ms.begin();
            while (it != ms.end())
            {
                if (it->ToRequest())
                    printf("request header size:[%d], payload size:[%d]\n", it->GetHeaderSize(), it->GetPayloadSize());
                else
                {
                    it->ToResponse();
                    printf("response header size:[%d], payload size:[%d]\n", it->GetHeaderSize(), it->GetPayloadSize());
                }
                ++it;
            }
        }

    private:
        uint16_t m_seq;
        uint8_t m_dev;
        uint8_t m_func;
    };
};
