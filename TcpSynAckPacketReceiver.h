/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#ifndef TCPSYNACKPACKETRECEIVER_H
#define TCPSYNACKPACKETRECEIVER_H

#include <arpa/inet.h>

#include <map>
#include <set>

using namespace std;

namespace WaveNs
{

class TcpSynAckPacketReceiver
{
    private :
        void initializeTimeout            (struct timeval *pTimeVal);
        void initializeWithDefaultTimeout (struct timeval *pTimeVal);

    protected :
    public :
              TcpSynAckPacketReceiver (const struct in_addr * const pSourceInetAddress, const unsigned int &outputInterfaceIndex, const unsigned short &packetFanOutGroup, const unsigned int &timeOutInMilliSeconds = 3000, const int rawSocketForTcpIp = 0);
             ~TcpSynAckPacketReceiver ();

        void  receivePackets          ();

        void  collectOpenPorts        (map<unsigned int, set<int> > &openPorts);

        // Now the data members

    private :
        const struct in_addr * const       m_pSourceInetAddress;
        const unsigned int                 m_outputInterfaceIndex;
        const unsigned short               m_packetFanOutGroup;

        const unsigned int                 m_timeOutInMilliSeconds;

              int                          m_rawSocketForTcpIp;

              map<unsigned int, set<int> > m_sourceIpAddressToOpenPorts;

    protected :
    public :
};
}

#endif // TCPSYNACKPACKETRECEIVER_H
