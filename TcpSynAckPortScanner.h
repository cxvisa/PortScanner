/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#ifndef TCPSYNACKPORTSCANNER_H
#define TCPSYNACKPORTSCANNER_H

#include <netinet/in.h>
#include <sys/types.h>
#include <set>
#include <map>
#include <string>
#include <pthread.h>
#include <vector>

using namespace std;

namespace WaveNs
{

class TcpSynAckPacketReceiver;
class TcpSynPacketSender;

class TcpSynAckPortScanner
{
    private :
        static void *receiveTcpSynAckPackets                          (void *pThreadContext);
        static void *sendTcpSynPackets                                (void *pThreadContext);

               void  consumeInputArguments                            (const int numberOfInputArguments, char *pinputArguments[]);
               void  computeSourceIpAddressesAndIfIndexesToBeUsed     ();
               void  launchThreadsToReceiveTcpSynAcks                 ();

               void  launchThreadsToSendTcpSynPacketsToDestinations   ();

               void  waitForSenderThreadsToFinish                     ();
               void  waitForReceiverThreadsToFinish                   ();

        static void printHelp                                         (const char *pProgramName);

        static void lock                                              ();
        static void unlock                                            ();

        static void setNumberOfReceiverThreadsYetToBecomeActive       (const unsigned int numberOfReceiverThreadsYetToBecomeActive);
        static void waitForReceiverThreadsToBecomeActive              ();
        static void setSendingPacketsCompleted                        (const bool &sendingPacketsCompleted);

    protected :
    public :
                     TcpSynAckPortScanner                             ();
                    ~TcpSynAckPortScanner                             ();

               void  initializeBasedOnInput                           (const int argc, char *argv[]);

               void  scanForOpenPorts                                 ();

               void  collectOpenPorts                                 (map<unsigned int, set<int> > &openPorts);
               void  printOpenPortsReport                             ();

        static bool getSendingPacketsCompleted                        ();

        static void decrementNumberOfReceiverThreadsYetToBecomeActive ();

        // Now the data members

    private :
        // INPUT Parameters

        // Destination IP Addresses to be scanned.  At the last there should be one IP Address to be scanned.
        set<string>                                     m_destinationIpAddresses;

        // Start port for the port Range to be scanned.
        //     Default is 1.
        int                                             m_startPort;

        // End port for the port Range to be scanned.
        //     Default is 65535.
        int                                             m_endPort;

        // Timeout for the RCP/IP SYN-ACK packet Receiver threads after the sender threads finish sending TCP/IP SYN packets.
        //     Default is 1 second (1000 ms)
        unsigned int                                    m_timeoutInMilliSeconds;

        // Number of threads to be launched per Source IfIndex to receive TCP/IP SYN-ACK packets.
        //    Default is 10.
        unsigned int                                    m_numberOfThreadsPerSource;

        // TCP/IP SYN packet burst count while sending.  Default is 100.
        unsigned int                                    m_batchCount;

        // Delay in milli seconds for which each of the TCP/IP SYN packet sender threads pauses after sending each burst.
        //     Default is 20 milli seconds (20000000 nano seconds)
        unsigned int                                    m_batchDelayInMilliSeconds;

        // INTERNAL Data structures

        // Map containing Destination IP Address to Source IP address through which the packets are sent out by kernel.
        //    Obtained by looking up FIB routing table in Kernel (RTM_F_LOOKUP_TABLE)
        map<string, string>                             m_destinationIpAddressToSourceIpAddressMap;

        // Set of all Source Ip Addresses used to reach all of the destination IP Addresses.
        set<string>                                     m_sourceIpAddresses;

        // Map of Source IP Address to corresponding in_addr *
        map<string, struct in_addr *>                   m_sourceInetAddressesMap;

        // Map containing Source IP Address to Interface Index obtained via Kernel FIB table (RTM_F_LOOKUP_TABLE)
        //     Please note that this is not a destination IP Address to egress IF Index map.  it is deliberately kept
        //     from source IP Address to egress IF Index.  We first lookup Destination to egress source IP address and then
        //     do egress Source IP Address to egress IF Index lookup.
        map<string, unsigned int>                       m_sourceInterfaceIndexesMap;

        // Map of Destination IP Address to corresponding in_addr *
        map<string, struct in_addr *>                   m_destinationInetAddressesMap;

        // Map containing list of all threads receiving TCP/IP SYNC-ACK packets per egress Source IP Address.
        map<string, vector<pthread_t> >                 m_tcpSynAckReceiverThreadBySourceIp;

        // Map containing list of all TcpSynAckPacketReceiver instances receiving TCP/IP SYNC-ACK packets per egress Source IP Address.
        //     All of the receivers corresponding to a given egress source IP Address / IF Index will participate in a
        //     PACKET_FANOUT Group.
        map<string, vector<TcpSynAckPacketReceiver *> > m_tcpSynAckPacketReceiverBySourceIp;

        // Map containing TCP/IP SYN packet sender thread per destination IP Address.
        map<string, pthread_t>                          m_tcpSynSenderThreadByDestinationIp;

        // Map containing TcpSynPacketSender instance per destination IP Address.
        map<string, TcpSynPacketSender *>               m_tcpSynPacketSenderByDestinationIp;

        // Internal Mutex maintained for the minimal locking needed across sender and receiver threads.
        static pthread_mutex_t                          m_mutex;

        // Internal Condition required to achieve synchronization between Receiver and Sender threads during thread launch.
        static pthread_cond_t                           m_condition;
        static bool                                     m_conditionInitialized;

        static unsigned int                             m_numberOfReceiverThreadsYetToBecomeActive;

        // Internal state tracking variable to indicate if all of the sender threads corresponding to to all destination IP addresses
        //     have finished sending TCP/IP SYN packets
        static bool                                     m_sendingPacketsCompleted;

    protected :
    public :
};

}

#endif // TCPSYNACKPORTSCANNER_H
