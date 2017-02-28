/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/


#include "TcpSynAckPacketReceiver.h"
#include "TcpSynAckPortScanner.h"

#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

using namespace std;

namespace WaveNs
{

TcpSynAckPacketReceiver::TcpSynAckPacketReceiver (const struct in_addr * const pSourceInetAddress, const unsigned int &outputInterfaceIndex, const unsigned short &packetFanOutGroup, const unsigned int &timeOutInMilliSeconds, const int rawSocketForTcpIp)
    : m_pSourceInetAddress    (pSourceInetAddress),
      m_outputInterfaceIndex  (outputInterfaceIndex),
      m_packetFanOutGroup     (packetFanOutGroup),
      m_timeOutInMilliSeconds (timeOutInMilliSeconds),
      m_rawSocketForTcpIp     (0)
{
}

TcpSynAckPacketReceiver::~TcpSynAckPacketReceiver ()
{
}

void TcpSynAckPacketReceiver::initializeTimeout (struct timeval *pTimeVal)
{
    pTimeVal->tv_sec  = m_timeOutInMilliSeconds / 1000;
    pTimeVal->tv_usec = (m_timeOutInMilliSeconds % 1000) * 1000;
}

void TcpSynAckPacketReceiver::initializeWithDefaultTimeout (struct timeval *pTimeVal)
{
    pTimeVal->tv_sec  = 1;
    pTimeVal->tv_usec = 0;
}

void TcpSynAckPacketReceiver::receivePackets ()
{
    bool locallyAllocatedRawSocket = false;

    if (0 == m_rawSocketForTcpIp)
    {
        m_rawSocketForTcpIp = socket (AF_PACKET, SOCK_DGRAM, htons (ETH_P_IP));

        if(m_rawSocketForTcpIp < 0)
        {
            perror ("Error creating Raw Socket for receiving TCP/IP SYN-ACK packets : ");

            exit(-1);
        }

        locallyAllocatedRawSocket = true;
    }

    if (locallyAllocatedRawSocket)
    {
        struct sockaddr_ll socketAddressToBind;

        memset (&socketAddressToBind, 0, sizeof (socketAddressToBind));

        socketAddressToBind.sll_family   = AF_PACKET;
        socketAddressToBind.sll_protocol = htons (ETH_P_IP);
        socketAddressToBind.sll_ifindex  = m_outputInterfaceIndex;

        int status = bind (m_rawSocketForTcpIp, (struct sockaddr *) &socketAddressToBind, sizeof (socketAddressToBind));

        if (status < 0)
        {
            perror ("Error binding Raw Socket to source interface for receiving TCP/IP SYN/ACK packets : ");

            exit(-1);
        }

        int       socketOptions       =  m_packetFanOutGroup | (PACKET_FANOUT_LB << 16);
        socklen_t sizeOfSocketOptions = sizeof (socketOptions);

        status = setsockopt (m_rawSocketForTcpIp, SOL_PACKET, PACKET_FANOUT, &socketOptions, sizeOfSocketOptions);

        if (status < 0)
        {
            perror ("Error setting SOL_PACKET / PACKET_FANOUT option for receiving TCP/IP SYN/ACK packets ");

            exit(-1);
        }
    }

    const unsigned int packetSize = 4 * 1024; // The 4 KB size is chosen arbitrarily.  If need be, it can be increased upto 65 KB max packet size.

    char packetToReceive[packetSize];

    struct sockaddr_ll receivedSocketAddress;
    socklen_t          receivedSocketAddressSize = sizeof (receivedSocketAddress);
    int                receivedBufferSize        = 0;

    struct timeval timeOut;
    bool           sendingPacketsCompleted                     = false;
    bool           userTimeOutUsedAfterSendingPacketsCompleted = false;

    initializeWithDefaultTimeout (&timeOut);

    fd_set fdSet;

    TcpSynAckPortScanner::decrementNumberOfReceiverThreadsYetToBecomeActive ();

    while (true)
    {
        FD_ZERO (&fdSet);
        FD_SET  (m_rawSocketForTcpIp, &fdSet);

        int selectStatus = select (m_rawSocketForTcpIp + 1, &fdSet, NULL, NULL, &timeOut);

        if (0 == selectStatus)
        {
            sendingPacketsCompleted = TcpSynAckPortScanner::getSendingPacketsCompleted ();

            if (sendingPacketsCompleted)
            {
                // After sender threads have completed, ensure that we wait for one iteration of timeout

                if (userTimeOutUsedAfterSendingPacketsCompleted)
                {
                    break;
                }
                else
                {
                    initializeTimeout (&timeOut);

                    userTimeOutUsedAfterSendingPacketsCompleted = true;

                    continue;
                }
            }
            else
            {
                // Prior to sender threads complete sending packets, we wait with a timeout of 1 seconds
                // in a loop.  Once the sender threads finish sending, the timeout (default of 1 second
                // (or the value set from command line will come into effect).

                initializeWithDefaultTimeout (&timeOut);

                continue;
            }
        }
        else if (0 > selectStatus)
        {
            if (EINTR == errno)
            {
                continue;
            }
            else
            {
                break;
            }
        }

        receivedBufferSize = recvfrom (m_rawSocketForTcpIp, packetToReceive, packetSize, 0, (struct sockaddr *) &receivedSocketAddress, &receivedSocketAddressSize);

        if (0 > receivedBufferSize)
        {
            perror ("Error receiving TCP/IP SYN/ACK packets : ");

            return;
        }

        // Currently we handle only TCP/IP SYN-ACK packets below.
        // This can be enhanced to handle other type of packets.

        struct iphdr *pIpHeader = (struct iphdr *) packetToReceive;

        if (IPPROTO_TCP == pIpHeader->protocol)
        {
            const unsigned short ipHeaderLength = pIpHeader->ihl * 4;

            struct sockaddr_in sourceSocketAddress;
            struct sockaddr_in destinationSocketAddress;

            memset (&sourceSocketAddress,      0, sizeof (sourceSocketAddress));
            memset (&destinationSocketAddress, 0, sizeof (destinationSocketAddress));

            sourceSocketAddress.sin_addr.s_addr      = pIpHeader->saddr;
            destinationSocketAddress.sin_addr.s_addr = pIpHeader->daddr;

            struct tcphdr *pTcpHeader = (struct tcphdr *) (packetToReceive + ipHeaderLength);

            //string sourceIpAddress      = inet_ntoa (sourceSocketAddress.sin_addr);
            //string destinationIpAddress = inet_ntoa (destinationSocketAddress.sin_addr);

            //cout << "Received Packet from " << sourceIpAddress << " to " << destinationIpAddress <<  ", from Port : " << ntohs (pTcpHeader->source)  << ", id : " << ntohs (pIpHeader->id) << endl;

            if ((1 == pTcpHeader->syn) && (1 == pTcpHeader->ack))
            {
                //cout << "Open " << ntohs (pTcpHeader->source) << " @ " << ntohl (sourceSocketAddress.sin_addr.s_addr) << endl;

                (m_sourceIpAddressToOpenPorts[sourceSocketAddress.sin_addr.s_addr]).insert (pTcpHeader->source);
            }
        }
    }

    if (locallyAllocatedRawSocket)
    {
        close (m_rawSocketForTcpIp);
    }
}

void TcpSynAckPacketReceiver::collectOpenPorts (map<unsigned int, set<int> > &openPorts)
{
    map<unsigned int, set<int> >::const_iterator element    = m_sourceIpAddressToOpenPorts.begin ();
    map<unsigned int, set<int> >::const_iterator endElement = m_sourceIpAddressToOpenPorts.end   ();

    while (endElement != element)
    {
        const unsigned int  s_addr                     = element->first;
        const set<int>     &portsOpenAtThisDestination = element->second;

        (openPorts[s_addr]).insert (portsOpenAtThisDestination.begin (), portsOpenAtThisDestination.end ());

        element++;
    }
}

}
