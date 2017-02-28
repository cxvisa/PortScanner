/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#ifndef ROUTEUTILS_H
#define ROUTEUTILS_H

#include <asm/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

using namespace std;

namespace WaveNs
{

typedef struct
{
    struct nlmsghdr m_netLinkMessageHeader;
    struct rtmsg    m_routeMessage;
           char     m_pBuffer[4096];    // Fow now statically allocating 4 KB buffer.
                                        // Can be optimized as needed based on the use cases.
} NetLinkRouteMessage;

class RouteUtils
{
    private :
    protected :
    public :
                RouteUtils                           ();
               ~RouteUtils                           ();

        void  getSourceIpAddressToReachDestination (const string &destinationIpAddress, string &outputSourceIpAddress, string &outputGateway, unsigned int &outputInterfaceIndex);

    // Now the data members

    unsigned int m_sequence;
    unsigned int m_pid;
};

}

#endif // ROUTEUTILS_H
