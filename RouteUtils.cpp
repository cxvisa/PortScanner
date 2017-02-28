/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#include "RouteUtils.h"

#include <errno.h>
#include <iostream>

using namespace std;

namespace WaveNs
{

RouteUtils::RouteUtils ()
{
    m_sequence = 1;
    m_pid      = getpid ();
}

RouteUtils::~RouteUtils ()
{
}

void RouteUtils::getSourceIpAddressToReachDestination (const string &destinationIpAddress, string &outputSourceIpAddress, string &outputGateway, unsigned int &outputInterfaceIndex)
{
    outputSourceIpAddress = "";
    outputGateway         = "";
    outputInterfaceIndex  = 0;

           NetLinkRouteMessage *pNetLinkRouteMessage  = new NetLinkRouteMessage ();
    struct nlmsghdr            *pNetLinkMessageHeader = &(pNetLinkRouteMessage->m_netLinkMessageHeader);

    memset (pNetLinkRouteMessage, 0, sizeof (NetLinkRouteMessage));

    m_sequence++;

    pNetLinkMessageHeader->nlmsg_len   = NLMSG_LENGTH (sizeof (struct rtmsg));
    pNetLinkMessageHeader->nlmsg_flags = NLM_F_REQUEST;
    pNetLinkMessageHeader->nlmsg_type  = RTM_GETROUTE;
    pNetLinkMessageHeader->nlmsg_seq   = m_sequence;
    pNetLinkMessageHeader->nlmsg_pid   = m_pid;

    // Deliberately restricting it to IPV4 for the demo

    pNetLinkRouteMessage->m_routeMessage.rtm_family = AF_INET;
    pNetLinkRouteMessage->m_routeMessage.rtm_flags  = RTM_F_LOOKUP_TABLE;

    int netLinkSocket = socket (PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

    if (0 > netLinkSocket)
    {
        perror ("Could not create a netlink socket : ");

        exit (-1);
    }

    struct in_addr destinationInetAddress;

    int status = inet_aton (destinationIpAddress.c_str (), &destinationInetAddress);

    if (1 != status)
    {
        // Not using perror since errno is not set on error.

        cerr << destinationIpAddress << " is not a valid destination IPV4 address." << endl;

        exit (-2);
    }

           int     routeAttributeLength = RTA_LENGTH (sizeof (destinationInetAddress)); // Since restricting it to IPV4 Address.
    struct rtattr *pRouteAttribute;

    if ((NLMSG_ALIGN (pNetLinkMessageHeader->nlmsg_len) + RTA_ALIGN (routeAttributeLength)) > sizeof (NetLinkRouteMessage))
    {
        cerr << "Out of space. Size : " << sizeof (NetLinkRouteMessage) << "exceeded." << endl;

        exit (-3);
    }

    pRouteAttribute = (struct rtattr *) (((void *) pNetLinkMessageHeader) + NLMSG_ALIGN (pNetLinkMessageHeader->nlmsg_len));

    pRouteAttribute->rta_type = RTA_DST;
    pRouteAttribute->rta_len  = routeAttributeLength;

    memcpy(RTA_DATA (pRouteAttribute), &destinationInetAddress, sizeof (destinationInetAddress));

    pNetLinkMessageHeader->nlmsg_len = NLMSG_ALIGN (pNetLinkMessageHeader->nlmsg_len) + RTA_ALIGN (routeAttributeLength);

    pNetLinkRouteMessage->m_routeMessage.rtm_dst_len = -1;

    // TODO : Iterate through all of the data is sent instead of posting it just once below

    status = send (netLinkSocket, pNetLinkMessageHeader, pNetLinkMessageHeader->nlmsg_len, 0);

    if (0 > status)
    {
        perror ("Could not create a netlink socket : ");

        exit (-4);
    }

    char *pBufferToRead        = (char *) pNetLinkMessageHeader;
    int   bufferLengthReceived = 0;

    // TODO : We may need to run a loop here to all data is read.

    bufferLengthReceived = recv (netLinkSocket, pBufferToRead + bufferLengthReceived, sizeof (NetLinkRouteMessage), 0);

    //cout << "Read Status : " << status << endl;

    if (0 > status)
    {
        perror ("Could not read from netlink socket : ");

        exit (-5);
    }

    if (NLMSG_OK (pNetLinkMessageHeader, bufferLengthReceived) == 0)
    {
        cerr << "Invalid NetLink Message header received." << endl;

        exit (-6);
    }

    if (NLMSG_ERROR == pNetLinkMessageHeader->nlmsg_type)
    {
        cerr << "NetLink Message header received indicates an error." << endl;

        return;
    }

    if ((m_sequence != pNetLinkMessageHeader->nlmsg_seq) || (m_pid != pNetLinkMessageHeader->nlmsg_pid))
    {
        cerr << "Received an unexpected packet." << endl;

        cerr << "m_sequence                       : " << m_sequence                       << endl;
        cerr << "pNetLinkMessageHeader->nlmsg_seq : " << pNetLinkMessageHeader->nlmsg_seq << endl;
        cerr << "m_pid                            : " << m_pid                            << endl;
        cerr << "pNetLinkMessageHeader->nlmsg_pid : " << pNetLinkMessageHeader->nlmsg_pid << endl;

        exit (-8);
    }

    close (netLinkSocket);

    string sourceIpAddress;
    string gatewayIpAddress;

    while (NLMSG_OK (pNetLinkMessageHeader, bufferLengthReceived))
    {
        struct rtmsg        *pRouteMessage      = (struct rtmsg *)  NLMSG_DATA (pNetLinkMessageHeader);
        struct rtattr       *pRouteAttribute    = (struct rtattr *) RTM_RTA    (pRouteMessage);
               int           routePayloadLength = RTM_PAYLOAD (pNetLinkMessageHeader);

               unsigned int  destination        = 0;
               unsigned int  source             = 0;
               unsigned int  gateway            = 0;

        while (RTA_OK (pRouteAttribute, routePayloadLength))
        {
            // cout << "RTA TYPE : " << pRouteAttribute->rta_type << endl;

            switch (pRouteAttribute->rta_type)
            {
                case RTA_DST :
                    destination = *((unsigned int *) (RTA_DATA (pRouteAttribute)));
                    break;

                case RTA_PREFSRC :
                    source = *((unsigned int *) (RTA_DATA (pRouteAttribute)));
                    break;

                case RTA_GATEWAY :
                    gateway = *((unsigned int *) (RTA_DATA (pRouteAttribute)));
                    break;

                case RTA_OIF :
                    outputInterfaceIndex = *((int *) (RTA_DATA (pRouteAttribute)));
                    break;

                default :
                    break;
            }

            pRouteAttribute = RTA_NEXT (pRouteAttribute, routePayloadLength);
        }

        if ((0 != source) && (destination == destinationInetAddress.s_addr))
        {
            in_addr sourceInetAddress;
            in_addr gatewayInetAddress;

            sourceInetAddress.s_addr  = source;
            gatewayInetAddress.s_addr = gateway;

            char *pSourceIpAddress  = inet_ntoa (sourceInetAddress);

            if (NULL != pSourceIpAddress)
            {
                sourceIpAddress = pSourceIpAddress;
            }

            char *pGatewayIpAddress = inet_ntoa (gatewayInetAddress);

            if (NULL != pGatewayIpAddress)
            {
                gatewayIpAddress = pGatewayIpAddress;
            }

            break;
        }

        pNetLinkMessageHeader = NLMSG_NEXT (pNetLinkMessageHeader, bufferLengthReceived);
    }

    // cout << "Destination : " << destinationIpAddress << ", Source : " << sourceIpAddress << ", Gateway : " << gatewayIpAddress << endl;

    outputSourceIpAddress = sourceIpAddress;
    outputGateway         = gatewayIpAddress;

    delete pNetLinkRouteMessage;

    return;
}

}
