/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#include "TcpSynPacketSender.h"
#include "TcpSynAckPacketReceiver.h"
#include "RouteUtils.h"

#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <stdio.h>
#include <iostream>

namespace WaveNs
{

TcpSynPacketSender::TcpSynPacketSender (const string &destinationIpAddress, const string &sourceIpAddressToBeUsed, const int startPort, const int endPort, const int rawSocketForTcpIp, const unsigned int &batchCount, const unsigned int &batchDelayInMilliSeconds)
    : m_destinationIpAddress     (destinationIpAddress),
      m_startPort                (startPort),
      m_endPort                  (endPort),
      m_rawSocketForTcpIp        (rawSocketForTcpIp),
      m_batchCount               (batchCount),
      m_batchDelayInMilliSeconds (batchDelayInMilliSeconds),
      m_sourceIpAddressToBeUsed  (sourceIpAddressToBeUsed)
{
    int status = inet_aton (m_destinationIpAddress.c_str (), &m_destinationInetAddress);

    if (1 != status)
    {
        cerr << m_destinationIpAddress << " is not a valid destination IPV4 address." << endl;

        exit (-1);
    }

    m_destinationSocketAddress.sin_family      = AF_INET;
    m_destinationSocketAddress.sin_port        = 0;
    m_destinationSocketAddress.sin_addr.s_addr = m_destinationInetAddress.s_addr;

    if (m_sourceIpAddressToBeUsed.empty ())
    {
        cerr << "Could not determine a source ip address through which the destination ip address can be reached." << endl;

        exit (-1);
    }

    status = inet_aton (m_sourceIpAddressToBeUsed.c_str (), &m_sourceInetAddressToBeUsed);

    if (1 != status)
    {
        cerr << "Not a valid Source IPV4 address." << endl;

        exit (-1);
    }
}

TcpSynPacketSender::~TcpSynPacketSender ()
{
}

unsigned short TcpSynPacketSender::computeChecksum (const unsigned short *pBuffer, int numberOfBytes)
{
    register long  sum        = 0;
    register short checksum;
    unsigned short extraByte;

    while (numberOfBytes > 1)
    {
        sum           += *pBuffer++;
        numberOfBytes -= 2;
    }

    if (1 == numberOfBytes)
    {
        extraByte = 0;

        *((unsigned char *) (&extraByte)) = *((unsigned char *) pBuffer);

        sum += extraByte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);

    checksum = (short) (~sum);

    return(checksum);
}

void TcpSynPacketSender::sendTcpSynPacketsToPortsAtDestination ()
{
    bool locallyAllocatedRawSocket = false;

    if (0 == m_rawSocketForTcpIp)
    {
        m_rawSocketForTcpIp = socket (AF_INET, SOCK_RAW, IPPROTO_TCP);

        if(m_rawSocketForTcpIp < 0)
        {
            perror ("Error creating Raw Socket to send TCP/IP SYN packets : ");

            exit(-1);
        }

        locallyAllocatedRawSocket = true;
    }

    const unsigned int packetSize = 4 * 1024; // The 4 KB size is chosen arbitrarily.  If need be, it can be increased upto 65 KB max packet size.

    char packetToSend[packetSize];

    memset (packetToSend, 0, packetSize);

    struct iphdr  *pIpHeader  = (struct iphdr  *) packetToSend;
    struct tcphdr *pTcpHeader = (struct tcphdr *) (packetToSend + sizeof (struct ip));

    TcpHeaderForChecksumCalculation tcpHeaderForChecksumCalculation;

    const int sourcePort = 3016; // Arbitrarily chosen source port.

    memset (packetToSend, 0, packetSize);

    pIpHeader->ihl      = 5;
    pIpHeader->version  = 4;
    pIpHeader->tos      = 0;
    pIpHeader->tot_len  = sizeof (struct ip) + sizeof (struct tcphdr);
    pIpHeader->id       = htons (0); // For now keeping 0 as ID for all packets
    pIpHeader->frag_off = htons (0); // No fragmentation is needed for now.
    pIpHeader->ttl      = 255;
    pIpHeader->protocol = IPPROTO_TCP;
    pIpHeader->check    = 0;
    pIpHeader->saddr    = inet_addr (m_sourceIpAddressToBeUsed.c_str ());
    pIpHeader->daddr    = m_destinationInetAddress.s_addr;

    pIpHeader->check    = computeChecksum ((unsigned short *) packetToSend, pIpHeader->tot_len);

    pTcpHeader->source  = htons (sourcePort);
    pTcpHeader->seq     = htonl (9013);
    pTcpHeader->ack_seq = 0;
    pTcpHeader->doff    = sizeof (struct tcphdr) / 4;
    pTcpHeader->fin     = 0;
    pTcpHeader->syn     = 1;
    pTcpHeader->rst     = 0;
    pTcpHeader->psh     = 0;
    pTcpHeader->ack     = 0;
    pTcpHeader->urg     = 0;
    pTcpHeader->window  = htons (packetSize + 8);
    pTcpHeader->check   = 0;
    pTcpHeader->urg_ptr = 0;

    const int ipHeaderIncluded = 1;

    int status = setsockopt (m_rawSocketForTcpIp, IPPROTO_IP, IP_HDRINCL, &ipHeaderIncluded, sizeof (ipHeaderIncluded));

    if (0 > status)
    {
        perror ("Could not set IP_HDRINCL on raw socket : ");

        exit(-1);
    }

    int destinationPort;
    int burstCount       = 0;

    for (destinationPort = m_startPort; destinationPort <= m_endPort; destinationPort++)
    {
        pTcpHeader->dest   = htons (destinationPort);
        pTcpHeader->check  = 0;

        tcpHeaderForChecksumCalculation.sourceAddress      = inet_addr (m_sourceIpAddressToBeUsed.c_str ());
        tcpHeaderForChecksumCalculation.destinationAddress = m_destinationSocketAddress.sin_addr.s_addr;
        tcpHeaderForChecksumCalculation.unused             = 0;
        tcpHeaderForChecksumCalculation.protocol           = IPPROTO_TCP;
        tcpHeaderForChecksumCalculation.tcpHeaderLength    = htons (sizeof (struct tcphdr));

        memcpy (&tcpHeaderForChecksumCalculation.tcpHeader, pTcpHeader, sizeof (struct tcphdr));

        pTcpHeader->check = computeChecksum ((unsigned short*) &tcpHeaderForChecksumCalculation, sizeof (TcpHeaderForChecksumCalculation));

        status = sendto (m_rawSocketForTcpIp, packetToSend, (sizeof (struct iphdr)) + (sizeof (struct tcphdr)), 0, (struct sockaddr *) &m_destinationSocketAddress, sizeof (m_destinationSocketAddress));

        if (0 > status)
        {
            fprintf (stderr, "socket : %d, destination : %s, port : %d\n", m_rawSocketForTcpIp, m_destinationIpAddress.c_str (), destinationPort);
            perror ("Could not send TCP/SYN packet on raw socket ");
            exit(0);
        }

        burstCount++;

        if (0 == (burstCount % m_batchCount))
        {
            struct timespec sleepTime;
            struct timespec remainingTime;

            sleepTime.tv_sec  = 0;
            sleepTime.tv_nsec = m_batchDelayInMilliSeconds * 1000000L;

            nanosleep (&sleepTime, &remainingTime);
        }
    }

    if (locallyAllocatedRawSocket)
    {
        close (m_rawSocketForTcpIp);
    }
}

}
