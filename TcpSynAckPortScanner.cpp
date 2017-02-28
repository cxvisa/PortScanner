/***************************************************************************
 *   Copyright (C) 2005-2016 Vidyasagara Guntaka                           *
 *   All rights reserved.                                                  *
 *   Author : Vidyasagara Reddy Guntaka                                    *
 ***************************************************************************/

#include "TcpSynAckPortScanner.h"

#include <arpa/inet.h>

#include "RouteUtils.h"
#include "TcpSynPacketSender.h"
#include "TcpSynAckPacketReceiver.h"

#include <iostream>
#include <string.h>
#include <algorithm>

#include <netdb.h>

using namespace std;

namespace WaveNs
{

pthread_mutex_t TcpSynAckPortScanner::m_mutex                                    = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  TcpSynAckPortScanner::m_condition                                = PTHREAD_COND_INITIALIZER;
bool            TcpSynAckPortScanner::m_conditionInitialized                     = false;
unsigned int    TcpSynAckPortScanner::m_numberOfReceiverThreadsYetToBecomeActive = 0;
bool            TcpSynAckPortScanner::m_sendingPacketsCompleted                  = false;

TcpSynAckPortScanner::TcpSynAckPortScanner ()
    : m_startPort                (1),
      m_endPort                  (65535),
      m_timeoutInMilliSeconds    (1000),
      m_numberOfThreadsPerSource (10),
      m_batchCount               (100),
      m_batchDelayInMilliSeconds (20)
{
    lock ();

    if (! m_conditionInitialized)
    {
        int status = pthread_cond_init (&m_condition, NULL);

        if (0 != status)
        {
            perror ("Error initializing a pthread condition ");

            exit (-1);
        }

        m_conditionInitialized = true;
    }

    unlock ();
}

TcpSynAckPortScanner::~TcpSynAckPortScanner ()
{
    map<string, struct in_addr *>::const_iterator element1    = m_sourceInetAddressesMap.begin ();
    map<string, struct in_addr *>::const_iterator endElement1 = m_sourceInetAddressesMap.end   ();

    while (endElement1 != element1)
    {
        struct in_addr *pSourceIndetAddress = element1->second;

        if (NULL != pSourceIndetAddress)
        {
            delete pSourceIndetAddress;
        }

        element1++;
    }

    map<string, struct in_addr *>::const_iterator element2    = m_destinationInetAddressesMap.begin ();
    map<string, struct in_addr *>::const_iterator endElement2 = m_destinationInetAddressesMap.end   ();

    while (endElement2 != element2)
    {
        struct in_addr *pDestinationIndetAddress = element2->second;

        if (NULL != pDestinationIndetAddress)
        {
            delete pDestinationIndetAddress;
        }

        element2++;
    }

    map<string, vector<TcpSynAckPacketReceiver *> >::const_iterator element3    = m_tcpSynAckPacketReceiverBySourceIp.begin ();
    map<string, vector<TcpSynAckPacketReceiver *> >::const_iterator endElement3 = m_tcpSynAckPacketReceiverBySourceIp.end   ();

    while (endElement3 != element3)
    {
        const vector<TcpSynAckPacketReceiver *> &tcpSynAckPacketReceivers  = element3->second;

        vector<TcpSynAckPacketReceiver *>::const_iterator element31    = tcpSynAckPacketReceivers.begin ();
        vector<TcpSynAckPacketReceiver *>::const_iterator endElement31 = tcpSynAckPacketReceivers.end   ();

        while (endElement31 != element31)
        {
            TcpSynAckPacketReceiver *pTcpSynAckPacketReceiver = *element31;

            if (NULL != pTcpSynAckPacketReceiver)
            {
                delete pTcpSynAckPacketReceiver;
            }

            element31++;
        }

        element3++;
    }

    map<string, TcpSynPacketSender *>::const_iterator element4    = m_tcpSynPacketSenderByDestinationIp.begin ();
    map<string, TcpSynPacketSender *>::const_iterator endElement4 = m_tcpSynPacketSenderByDestinationIp.end   ();

    while (endElement4 != element4)
    {
        TcpSynPacketSender *pTcpSynPacketSender = element4->second;

        if (NULL != pTcpSynPacketSender)
        {
            delete pTcpSynPacketSender;
        }

        element4++;
    }
}

void *TcpSynAckPortScanner::receiveTcpSynAckPackets (void *pThreadContext)
{

    TcpSynAckPacketReceiver *pTcpSynAckPacketReceiver = (TcpSynAckPacketReceiver *) pThreadContext;

    pTcpSynAckPacketReceiver->receivePackets ();

    return (pTcpSynAckPacketReceiver);
}

void *TcpSynAckPortScanner::sendTcpSynPackets (void *pThreadContext)
{
    TcpSynPacketSender *pTcpSynPacketSender = (TcpSynPacketSender *) pThreadContext;

    pTcpSynPacketSender->sendTcpSynPacketsToPortsAtDestination ();

    return (pTcpSynPacketSender);
}

void TcpSynAckPortScanner::initializeBasedOnInput (const int argc, char *argv[])
{
    consumeInputArguments (argc, argv);

    computeSourceIpAddressesAndIfIndexesToBeUsed ();
}

void  TcpSynAckPortScanner::consumeInputArguments (const int numberOfInputArguments, char *pInputArguments[])
{
    int i= 0;

    for (i = 1; i < numberOfInputArguments; i++)
    {
        //cout << pInputArguments[i] << " " << endl;

        if (0 == (strcasecmp ("-ip", pInputArguments[i])))
        {
            if (numberOfInputArguments > (i + 1))
            {
                in_addr inetAddress;

                int   status = inet_aton (pInputArguments[i +1], &inetAddress);
                bool isValid = (1 == status) ? true : false;

                if (isValid)
                {
                    m_destinationIpAddresses.insert (pInputArguments[i +1]);
                }
                else
                {
                    cerr << "Invalid IP Address." << endl;

                    printHelp (pInputArguments[0]);

                    exit (-1);
                }

                i++;
            }
            else
            {
                printHelp (pInputArguments[0]);

                exit (-1);
            }
        }
        else if (0 == (strcasecmp ("-p", pInputArguments[i])))
        {
            if (numberOfInputArguments > (i + 1))
            {
                sscanf (pInputArguments[i +1], "%d-%d", &m_startPort, &m_endPort);

                if ((m_endPort < m_startPort) || (1 > m_startPort) || (65535 < m_startPort) || (1 > m_endPort) || (65535 < m_endPort))
                {
                    cerr << "Invalid Port Range." << endl;

                    printHelp (pInputArguments[0]);

                    exit (-1);
                }
            }
            else
            {
                printHelp (pInputArguments[0]);

                exit (-1);
            }

            i++;
        }
        else if (0 == (strcasecmp ("-t", pInputArguments[i])))
        {
            if (numberOfInputArguments > (i + 1))
            {
                int timeoutInMilliSeconds = 0;

                sscanf (pInputArguments[i + 1], "%d", &timeoutInMilliSeconds);

                if (0 < timeoutInMilliSeconds)
                {
                    m_timeoutInMilliSeconds = timeoutInMilliSeconds;
                }
                else
                {
                    printHelp (pInputArguments[0]);

                    exit (-1);
                }
            }
            else
            {
                printHelp (pInputArguments[0]);

                exit (-1);
            }

            i++;
        }
        else if (0 == (strcasecmp ("-tps", pInputArguments[i])))
        {
            if (numberOfInputArguments > (i + 1))
            {
                int numberOfThreadsPerSource = 0;

                sscanf (pInputArguments[i + 1], "%d", &numberOfThreadsPerSource);

                if (0 < m_numberOfThreadsPerSource)
                {
                    m_numberOfThreadsPerSource = numberOfThreadsPerSource;
                }
                else
                {
                    printHelp (pInputArguments[0]);

                    exit (-1);
                }
            }
            else
            {
                printHelp (pInputArguments[0]);

                exit (-1);
            }

            i++;
        }
        else if (0 == (strcasecmp ("-bc", pInputArguments[i])))
        {
            if (numberOfInputArguments > (i + 1))
            {
                int batchCount = 0;

                sscanf (pInputArguments[i + 1], "%d", &batchCount);

                if (0 < batchCount)
                {
                    m_batchCount = batchCount;
                }
                else
                {
                    printHelp (pInputArguments[0]);

                    exit (-1);
                }
            }
            else
            {
                printHelp (pInputArguments[0]);

                exit (-1);
            }

            i++;
        }
        else if (0 == (strcasecmp ("-bd", pInputArguments[i])))
        {
            if (numberOfInputArguments > (i + 1))
            {
                int batchDelayInMilliSeconds = 0;

                sscanf (pInputArguments[i + 1], "%d", &batchDelayInMilliSeconds);

                if (0 < batchDelayInMilliSeconds)
                {
                    m_batchDelayInMilliSeconds = batchDelayInMilliSeconds;
                }
                else
                {
                    printHelp (pInputArguments[0]);

                    exit (-1);
                }
            }
            else
            {
                printHelp (pInputArguments[0]);

                exit (-1);
            }

            i++;
        }
        else
        {
            printHelp (pInputArguments[0]);

            exit (-1);
        }
    }

    if (m_destinationIpAddresses.empty ())
    {
        printHelp (pInputArguments[0]);

        exit (-1);
    }
}

void TcpSynAckPortScanner::printHelp (const char *pProgramName)
{
    cerr << pProgramName << " <-ip <IPV4 Address>> [[-ip <IPV4Address>]...] [-p <Port Range>] [-tps <Number Of Threads Per Source Interface>] [-t <Receiver Thread Time Out in Milli Seconds>] [-bc <Batch Count>] [-bd <Batch Delay in Milli Seconds>]" << endl;
    cerr << endl;
    cerr << "-ip  : Requires an IP V4 Address." << endl;
    cerr << "       At least one IPV4 Address must be specified." << endl;
    cerr << "       Multiple destination IPv4 Addresses can be specified by repeating this option." << endl;
    cerr << "       If any of the supplied IP Addresses is not a valid IPV4 Address, program errors out." << endl;
    cerr << endl;
    cerr << "-p   : Requires a Port Range of the format <Start Port>-<End Port>." << endl;
    cerr << "       Start Port and End Port should be within the range [1, 65535]." << endl;
    cerr << "       Start Port value should NOT be greater than that of End Port." << endl;
    cerr << "       Default port range if this option is not used is : 1-65535" << endl;
    cerr << "       Example : 9-100 : will consider all ports including 9 and 100 and all the numbers in between." << endl;
    cerr << "                 23    : will be considered as 23-65535" << endl;
    cerr << "                 23-   : will be considered as 23-65535" << endl;
    cerr << "                 -10   : will fail the input since it will be read as a negative number" << endl;
    cerr << "                 7-7   : will result in scanning only the port number 7" << endl;
    cerr << endl;
    cerr << "-tps : Requires number of threads to be spawned per egress interface (Port Fanout Group))." << endl;
    cerr << "       Please see README.1st for more details regarding the thread group and how they are used." << endl;
    cerr << "       Default value is 10." << endl;
    cerr << "       Example : 100 : Spawns 100 threads to read TCP/IP SYN-ACK packets for the port fanout group" << endl;
    cerr << "                       corresponding to each the egress interfaces." << endl;
    cerr << endl;
    cerr << "-t   : Requires timeout in Milli Seconds." << endl;
    cerr << "       This is the amount of time that the receiver threads will wait reading the" << endl;
    cerr << "       TCP/IP SYN-ACK packets after sender threads have finished." << endl;
    cerr << endl;
    cerr << "-bc  : Requires a batch count" << endl;
    cerr << "       This is the number of TCP/IP SYN packets that are sent out in a burst by a Sender thread." << endl;
    cerr << "       Default value is 100." << endl;
    cerr << endl;
    cerr << "-bd  : Requires a delay in Milli Seconds." << endl;
    cerr << "       This is the amount of time sender thread waits between sending TCP/IP SYN packet bursts." << endl;
    cerr << "       Default value is 20 Milli Seconds." << endl;
    cerr << endl;
    cerr << "For more information regarding any of the input options, please consider going through the README.1st document." << endl;
    cerr << endl;
}

void TcpSynAckPortScanner::computeSourceIpAddressesAndIfIndexesToBeUsed ()
{
    RouteUtils routeUtils;

    set<string>::const_iterator element    = m_destinationIpAddresses.begin ();
    set<string>::const_iterator endElement = m_destinationIpAddresses.end   ();

    while (endElement != element)
    {
        const string destinationIpAddress               = *element;
              string sourceToBeUsedForThisDestination;
              string gatewayToBeUsedForThisDestination;
              unsigned int outputInterfaceIndex         = 0;

        routeUtils.getSourceIpAddressToReachDestination (destinationIpAddress, sourceToBeUsedForThisDestination, gatewayToBeUsedForThisDestination, outputInterfaceIndex);

        if (0 == outputInterfaceIndex)
        {
            cerr << destinationIpAddress << " is not reachable." << endl;

            exit (-1);
        }

        printf ("Packets to %15s are sent using source %15s on Interface Index %5u via gateway %15s\n", destinationIpAddress.c_str (), sourceToBeUsedForThisDestination.c_str (), outputInterfaceIndex, gatewayToBeUsedForThisDestination.c_str ());

        m_destinationIpAddressToSourceIpAddressMap[destinationIpAddress] = sourceToBeUsedForThisDestination;
        m_sourceIpAddresses.insert (sourceToBeUsedForThisDestination);

        struct in_addr *pDestinationInetAddress = new in_addr;

        int status = inet_aton (destinationIpAddress.c_str (), pDestinationInetAddress);

        if (1 != status)
        {
            cerr << "Destination " << destinationIpAddress << " is not a valid destination IPV4 address." << endl;

            exit (-1);
        }

        m_destinationInetAddressesMap[destinationIpAddress] = pDestinationInetAddress;

        if ((m_sourceInetAddressesMap.end ()) == (m_sourceInetAddressesMap.find (sourceToBeUsedForThisDestination)))
        {
            struct in_addr *pSourceInetAddress = new struct in_addr;

            status = inet_aton (sourceToBeUsedForThisDestination.c_str (), pSourceInetAddress);

            if (1 != status)
            {
                cerr << "Source " << sourceToBeUsedForThisDestination << " is not a valid destination IPV4 address." << endl;

                exit (-1);
            }

            m_sourceInetAddressesMap[sourceToBeUsedForThisDestination]    = pSourceInetAddress;
            m_sourceInterfaceIndexesMap[sourceToBeUsedForThisDestination] = outputInterfaceIndex;
        }

        element++;
    }
}

void TcpSynAckPortScanner::launchThreadsToReceiveTcpSynAcks ()
{
    setNumberOfReceiverThreadsYetToBecomeActive ((m_sourceIpAddresses.size ()) * m_numberOfThreadsPerSource);

    set<string>::const_iterator element    = m_sourceIpAddresses.begin ();
    set<string>::const_iterator endElement = m_sourceIpAddresses.end   ();

    unsigned short packetFanOutGroup = 0;

    while (endElement != element)
    {
        const string sourceIpAddressToBeUsed = *element;

        const struct in_addr * const pSourceInetAddress   = m_sourceInetAddressesMap[sourceIpAddressToBeUsed];
        const unsigned int           outputInterfaceIndex = m_sourceInterfaceIndexesMap[sourceIpAddressToBeUsed];

        pthread_t tcpSynAckReceiverThreadId;

        packetFanOutGroup++;

        for (unsigned int i = 0; i < m_numberOfThreadsPerSource; i++)
        {
            TcpSynAckPacketReceiver *pTcpSynAckPacketReceiver = new TcpSynAckPacketReceiver (pSourceInetAddress, outputInterfaceIndex, packetFanOutGroup, m_timeoutInMilliSeconds);

            printf ("(%5d-%10lu) Launching a thread to monitor TCP SYN/ACK packets coming back to source %s\n", packetFanOutGroup, tcpSynAckReceiverThreadId, sourceIpAddressToBeUsed.c_str ());

            if (pthread_create (&tcpSynAckReceiverThreadId, NULL, receiveTcpSynAckPackets, pTcpSynAckPacketReceiver) < 0)
            {
                perror ("Could not create a thread to monitor TCP SYN/ACK packets : ");

                exit(0);
            }

            (m_tcpSynAckReceiverThreadBySourceIp[sourceIpAddressToBeUsed]).push_back (tcpSynAckReceiverThreadId);
            (m_tcpSynAckPacketReceiverBySourceIp[sourceIpAddressToBeUsed]).push_back (pTcpSynAckPacketReceiver);
        }

        element++;
    }
}

void TcpSynAckPortScanner::launchThreadsToSendTcpSynPacketsToDestinations ()
{
    set<string>::const_iterator element    = m_destinationIpAddresses.begin ();
    set<string>::const_iterator endElement = m_destinationIpAddresses.end   ();

    while (endElement != element)
    {
        const string destinationIpAddress = *element;
        const string sourceIpAddressToBeUsedForThisDestination = m_destinationIpAddressToSourceIpAddressMap[destinationIpAddress];

        pthread_t tcpSynSenderThreadId;

        TcpSynPacketSender *pTcpSynPacketSender = new TcpSynPacketSender (destinationIpAddress, sourceIpAddressToBeUsedForThisDestination, m_startPort, m_endPort, 0, m_batchCount, m_batchDelayInMilliSeconds);

        printf ("Sending Packets to ports at destination %15s using source %15s\n", destinationIpAddress.c_str (), sourceIpAddressToBeUsedForThisDestination.c_str ());

        if (pthread_create (&tcpSynSenderThreadId, NULL, sendTcpSynPackets, pTcpSynPacketSender) < 0)
        {
            perror ("Could not create a thread to send TCP SYN packets : ");

            exit(0);
        }

        m_tcpSynSenderThreadByDestinationIp[destinationIpAddress] = tcpSynSenderThreadId;
        m_tcpSynPacketSenderByDestinationIp[destinationIpAddress] = pTcpSynPacketSender;

        element++;
    }
}

void TcpSynAckPortScanner::waitForSenderThreadsToFinish ()
{
    map<string, pthread_t>::const_iterator element    = m_tcpSynSenderThreadByDestinationIp.begin ();
    map<string, pthread_t>::const_iterator endElement = m_tcpSynSenderThreadByDestinationIp.end   ();

    while (endElement != element)
    {
        const string    destinationIpAddress = element->first;
        const pthread_t tcpSynSenderThreadId = element->second;

        pthread_join (tcpSynSenderThreadId , NULL);

        element++;
    }

    cout << "All Sender threads completed sending packets." << endl;
}

void TcpSynAckPortScanner::waitForReceiverThreadsToFinish ()
{
    map<string, vector<pthread_t> >::const_iterator element    = m_tcpSynAckReceiverThreadBySourceIp.begin ();
    map<string, vector<pthread_t> >::const_iterator endElement = m_tcpSynAckReceiverThreadBySourceIp.end   ();

    while (endElement != element)
    {
        const string             sourceIpAddress            = element->first;
        const vector<pthread_t> &tcpSynAckReceiverThreadIds = element->second;

        vector<pthread_t>::const_iterator element1    = tcpSynAckReceiverThreadIds.begin ();
        vector<pthread_t>::const_iterator endElement1 = tcpSynAckReceiverThreadIds.end   ();

        while (endElement1 != element1)
        {
            const pthread_t tcpSynAckReceiverThreadId = *element1;

            pthread_join (tcpSynAckReceiverThreadId, NULL);

            element1++;
        }

        element++;
    }
}

void TcpSynAckPortScanner::scanForOpenPorts ()
{
    // First ensure that the internal state to indicate send complete is set to false.
    // This state tracking will be used to ensure that the receiver threads do not finish
    // before sender threads finish.

    setSendingPacketsCompleted                     (false);

    // Before launching any TCP/IP SYNC packet sender threads, launch the TCP/IP SYNC-ACK Receiver threads
    // so that we do not miss any acknowledgments coming back.
    //
    // We launch a group of threads per egress IF Index we determined earlier to reach all of the given
    // destination IP Addresses.  By default we launch 10 threads per egress IF Index.  Each of the threads
    // in the group will open a PACKET socket and joins the corresponding PACKET_FANOUT group with load balancing
    // (PACKET_FANOUT_LB) attribute.

    cout << "Launching Receiver Threads for Packet Fanout Groups to receive TCP/IP SYN-ACK packets." << endl;

    launchThreadsToReceiveTcpSynAcks               ();

    // Next, wait for all of the reciever threads to come to a stage where they have created packet sockets and are
    // ready to receive the incoming packets.

    cout << "Waiting for all of the Receiver threads to become active." << endl;

    waitForReceiverThreadsToBecomeActive           ();

    // Now that the receiver threads are ready,  launch the TCP/IP SYN packet sender threads.
    // We launch one sender thread per destination currently.
    //
    // Each thread open a raw socket and send out TCP/IP SYN packets to all of the ports that are to be
    // scanned.

    cout << "Launching Sender threads one per destination to send TCP/IP SYN packets." << endl;

    launchThreadsToSendTcpSynPacketsToDestinations ();

    // Wait for all of the sender threads to finish.

    cout << "Waiting for all of the sender threads to finish." << endl;

    waitForSenderThreadsToFinish                   ();

    // Indicate to the receiver threads that the sender threads finished sending packets.

    setSendingPacketsCompleted                     (true);

    // At this point, the receiver threads will wait for a minimum time out waiting for TCP/IP SYN-ACK packets.
    // Default timeout is 1 second.  It is configurable via command line options.

    //Now, wait for all of the receiver threads to finish.

    cout << "Waiting for all of the receiver threads to finish." << endl;

    waitForReceiverThreadsToFinish                 ();
}

void TcpSynAckPortScanner::printOpenPortsReport ()
{
    map<unsigned int, set<int> > openPorts;

    collectOpenPorts (openPorts);

    map<unsigned int, set<int> >::const_iterator element    = openPorts.begin ();
    map<unsigned int, set<int> >::const_iterator endElement = openPorts.end   ();

    setservent (1);

    while (endElement != element)
    {
        unsigned int s_addr = element->first;

        in_addr destinationInetAddress;

        destinationInetAddress.s_addr = s_addr;

        const set<int> &portsOpenAtThisDestination = element->second;

        cout << inet_ntoa (destinationInetAddress) << " : " << endl;
        cout << "    Number of Open Ports : " << portsOpenAtThisDestination.size () << endl;

        vector<int> sortedPortsOpenAtThisDestination;

        set<int>::const_iterator element1    = portsOpenAtThisDestination.begin ();
        set<int>::const_iterator endElement1 = portsOpenAtThisDestination.end   ();

        while (endElement1 != element1)
        {
            int port = ntohs (*element1);

            sortedPortsOpenAtThisDestination.push_back (port);

            element1++;
        }

        sort (sortedPortsOpenAtThisDestination.begin (), sortedPortsOpenAtThisDestination.end ());

        vector<int>::const_iterator element2    = sortedPortsOpenAtThisDestination.begin ();
        vector<int>::const_iterator endElement2 = sortedPortsOpenAtThisDestination.end   ();

        printf ("        %5s %s\n", "PORT", "SERVICE");
        printf ("        %5s %s\n", "____", "_______");

        while (endElement2 != element2)
        {
            int port = *element2;

            struct servent *pServiceEntry = getservbyport (htons (port), "tcp");

            printf ("        %5u %s\n", port, pServiceEntry != NULL ? pServiceEntry->s_name : "Unknown");

            element2++;
        }

        cout << endl;

        element++;
    }

    endservent ();
}

void TcpSynAckPortScanner::collectOpenPorts (map<unsigned int, set<int> > &openPorts)
{
    map<string, vector<TcpSynAckPacketReceiver *> >::const_iterator element    = m_tcpSynAckPacketReceiverBySourceIp.begin ();
    map<string, vector<TcpSynAckPacketReceiver *> >::const_iterator endElement = m_tcpSynAckPacketReceiverBySourceIp.end   ();

    while (endElement != element)
    {
        const vector<TcpSynAckPacketReceiver *> &tcpSynAckPacketReceivers  = element->second;

        vector<TcpSynAckPacketReceiver *>::const_iterator element1    = tcpSynAckPacketReceivers.begin ();
        vector<TcpSynAckPacketReceiver *>::const_iterator endElement1 = tcpSynAckPacketReceivers.end   ();

        while (endElement1 != element1)
        {
            TcpSynAckPacketReceiver *pTcpSynAckPacketReceiver = *element1;

            if (NULL != pTcpSynAckPacketReceiver)
            {
                pTcpSynAckPacketReceiver->collectOpenPorts (openPorts);
            }

            element1++;
        }

        element++;
    }
}

void TcpSynAckPortScanner::lock ()
{
    int status = pthread_mutex_lock (&m_mutex);

    if (0 != status)
    {
        perror ("Locking failed : ");

        exit (-1);
    }
}

void TcpSynAckPortScanner::unlock ()
{
    int status = pthread_mutex_unlock (&m_mutex);

    if (0 != status)
    {
        perror ("Unlocking failed : ");

        exit (-1);
    }
}

bool TcpSynAckPortScanner::getSendingPacketsCompleted ()
{
    bool temp;

    lock ();

    temp = m_sendingPacketsCompleted;

    unlock ();

    return (temp);
}

void  TcpSynAckPortScanner::setSendingPacketsCompleted (const bool &sendingPacketsCompleted)
{
    lock ();

    m_sendingPacketsCompleted = sendingPacketsCompleted;

    unlock ();
}

void TcpSynAckPortScanner::setNumberOfReceiverThreadsYetToBecomeActive (const unsigned int numberOfReceiverThreadsYetToBecomeActive)
{
    lock ();

    m_numberOfReceiverThreadsYetToBecomeActive = numberOfReceiverThreadsYetToBecomeActive;

    unlock ();
}

void TcpSynAckPortScanner::decrementNumberOfReceiverThreadsYetToBecomeActive ()
{
    lock ();

    m_numberOfReceiverThreadsYetToBecomeActive--;

    if (0 == m_numberOfReceiverThreadsYetToBecomeActive)
    {
        int status = pthread_cond_signal (&m_condition);

        if (0 != status)
        {
            perror ("Error signaling on a pthread condition ");

            exit (-1);
        }
    }

    unlock ();
}

void TcpSynAckPortScanner::waitForReceiverThreadsToBecomeActive ()
{
    lock ();

    if (0 < m_numberOfReceiverThreadsYetToBecomeActive)
    {
        int status = pthread_cond_wait (&m_condition, &m_mutex);

        if (0 != status)
        {
            perror ("Error waiting on a pthread condition ");

            exit (-1);
        }
    }

    unlock ();
}

}

