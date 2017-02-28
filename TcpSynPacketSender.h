/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#ifndef TCPSYNPACKETSENDER_H
#define TCPSYNPACKETSENDER_H

#include <string>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>

using namespace std;

namespace WaveNs
{

struct TcpHeaderForChecksumCalculation
{
    unsigned int   sourceAddress;
    unsigned int   destinationAddress;
    unsigned char  unused;
    unsigned char  protocol;
    unsigned short tcpHeaderLength;

    struct tcphdr  tcpHeader;
};

class TcpSynPacketSender
{
    private :
        unsigned short computeChecksum (const unsigned short *pBuffer, int numberOfBytes);

    protected :
    public :
              TcpSynPacketSender                    (const string &destinationIpAddress, const string &sourceIpAddressToBeUsed, const int startPort = 1, const int endPort = 65535, const int rawSocketForTcpIp = 0, const unsigned int &batchCount = 100, const unsigned int &batchDelayInMilliSeconds = 20);
             ~TcpSynPacketSender                    ();

        void  sendTcpSynPacketsToPortsAtDestination ();

        // Now the data members

    private :
              string             m_destinationIpAddress;
              int                m_startPort;
              int                m_endPort;
              int                m_rawSocketForTcpIp;
        const unsigned int       m_batchCount;
        const unsigned int       m_batchDelayInMilliSeconds;
              struct in_addr     m_destinationInetAddress;
              struct sockaddr_in m_destinationSocketAddress;
              string             m_sourceIpAddressToBeUsed;
              struct in_addr     m_sourceInetAddressToBeUsed;

    protected :
    public :
};

}

#endif // TVPSYNCPACKETSENDER_H
