/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#include "TcpSynAckPortScanner.h"

using namespace WaveNs;

int main (int argc, char *argv[])
{
    // 1.  Create a TCP/IP SYN/ACK based port scanner instance

    TcpSynAckPortScanner tcpSynAckPortScanner;

    // 2.  Initialize the TCP/IP SYN/ACK port scanner based on input.
    //
    //     Various input parameters will be looked into.
    //     Default values are retained for the parameters that are not set via command line.
    //
    //     Computes list of all egress source IP address and corresponding egress IF Indexes
    //     needed to reach all destination IP Addresses.
    //
    //     Determines the egress source IP Address / egress IF Index across all destination IP Addresses
    //     using kernel RTM_F_LOOKUP_TABLE lookup by opening netlink sockets.

    tcpSynAckPortScanner.initializeBasedOnInput (argc, argv);

    // 3.  Initiates scanning for open ports based on input parameters.
    //
    //     First Launches threads to receive TCP/IP SYN-ACK packets from the destinations.
    //
    //     A  group of threads is launched per egress source IP address / IF Index since
    //     the egress Source IP Address during the TCP/IP SYN packet sending phase will be the
    //     ingress Destination IP Address inside the TCP/IP SYN-ACK packet being received.
    //
    //     All receiver threads belonging to a receiver group will participate in a PORT_FANOUT group
    //     with PACKET_FANOUT_LB option.
    //
    //     Then, launches threads to send TCP/IP SYN packets to destinations.
    //
    //     Waits for all of the sender threads to finish.
    //
    //     Waits for the receiver threads to finish with the specified timeout.

    tcpSynAckPortScanner.scanForOpenPorts ();

    // 4.  Collects open ports from all of the receiver threads across all destinations.
    //
    //     Merges results for destinations across all threads.
    //
    //     sorts ports for each of the destinations.
    //
    //     Displays the open ports per destination with potential service names.

    tcpSynAckPortScanner.printOpenPortsReport ();

    return (0);
}
