/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#include "RouteUtils.h"
#include "TcpSynPacketSender.h"

#include <iostream>

using namespace std;
using namespace WaveNs;

int main (int argc, char *argv[])
{
    if (2 > argc)
    {
        cerr << "Usage : " << argv[0] << " <IPV4 Address>" << endl;

	return (0);
    }

    RouteUtils routeUtils;

    string       destinationIpAddress              = argv[1];
    string       sourceToBeUsedForThisDestination;
    string       gatewayToBeUsedForThisDestination;
    unsigned int outputInterfaceIndex = 0;

    routeUtils.getSourceIpAddressToReachDestination (destinationIpAddress, sourceToBeUsedForThisDestination, gatewayToBeUsedForThisDestination, outputInterfaceIndex);

    cout << "Packets to " << destinationIpAddress << " are sent using source " << sourceToBeUsedForThisDestination << " on Interface Index " << outputInterfaceIndex << " via gateway " << gatewayToBeUsedForThisDestination << endl;

    return (0);
}
