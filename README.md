===========================
TCP/IP SYN-ACK Port Scanner
===========================

**INTRODUCTION**
----------------
____________

This document briefly introduces the TCP/IP SYN-ACK based port scanner written
by Vidyasagara Guntaka.

It dscribes the high level design / code flow for the scanner.  Also describes
how to compile and run the scanner code.  Finally it documents limitations and
potential improvements.

**SCOPE**
---------
_____

The scope of the exercise is limited to the following:

  * Working with Kernel Routing/FIB tables
  * Working with netlink sockets
  * Working with RAW Sockets
  * Working with PACKET sockets
  * Working with multiple IP addresses for scanning
  * Working with IPV4 addresses
  * Making use of TCP/IP SYN and SYN/ACK packets
  * Making using of PTHREADs for concurrency and minimize packet loss

Please note that working with the following areas among others were covered
in this code base as they are available in Framework/Utils directory under Wave Repository:

  * Working with IPV6 Adresses
  * Working with Asynchrnous sockets
  * Working with Multiple Processes, IPC and coordination among the processes
  * Working with multiple sockets concurrently using select/poll
  * Auto detecting certain resource limits and constraining and map/reducing
    work loads

HIGH LEVEL DESIGN
-----------------
_________________

1.  The scanner collects the input IPV4 addresses and other input parameters.

2.  Then for each of the IP addresses it detemines the egress interface through
      which the packets will be sent out to destination on the machine where
      the scanner is running.

      Using netlink message with type RTM_GETROUTE and using the RTM Flag
      RTM_F_LOOKUP_TABLE, it detemines the preferred source IP address and
      corresponding IF Index through which packets will be sent out to reach the
      destination IP Address.  It will also determine if a gateway is involved.
      We currently do not make use of gateway address directly in our code base.
      But, it will be handy in future use cases.

3.  For each of the egress interfaces, it will launch a group of threads.  We
      call these Receiver threads since we expect to receive TCP/IP SYN-ACK on
      these threads.  Each of these receiver threads opens a PACKET socket
      as in the following to receive all IP packets with ethernet header
      removed.

      m_rawSocketForTcpIp = socket (AF_PACKET, SOCK_DGRAM, htons (ETH_P_IP))

      Then the socket will join a PACKET_FANOUT group with PACKET_FANOUT_LB
      option.  this will ensure that all TCP/IP SYN-ACK packets coming back to
      the interface are load balanced across the threads in the fanout group
      so that the packets could be read as soon as possible and hence not loose
      the packets on the receiving side.

      By default we launch a group of 10 threads per egress interface.  All of
      those 10 threads corresponding to the egress interface join the same
      packet fanout group with load balancing option.  A command line interface
      option is provided to control number of threads per group.

      Please note that we launch the receiver threads before we launch the
      threads that are used to send TCP/IP SYN packets so that we are ready to
      read TCP/IP SYN-ACK packets as soon as they start arriving.

4.  Then the scanner waits for all of the receiver threads to come to a stage 
      where they can start reading the TCP/IP SYN-ACK packets.

5.  Next the scanner launches the threads to send TCP/IP SYN packets to
      destination IP Addresses.  We call these sender threads.

      We launch a separate thread per destination IP address as below:

      m_rawSocketForTcpIp = socket (AF_INET, SOCK_RAW, IPPROTO_TCP)

      We then indicate that the IP headers are included in the packet since
      we will be crafting the IP Header and TCP header to send out the TCP/IP
      SYN packets.  We craft the IPV4 header and compute its checkum and fill
      it in the header.  Then we craft the TCP header to indicate a SYN packet.

      We use the prefered source IP Address determined using the kernel FIB
      table lookup as the source IP address in the IP header so that the
      acks come back to this ip address.

      Then for each of the ports in the range specified, we generate a TCP/IP
      packet by filling the destination port and compute TCP checksum and fill
      it in the TCP header.

      By default the port range is 1-65535.  There is a command line interface
      option provided to control the port range to be scanned.

      Also, there are two controls provided that help regulating the flow of
      TCP/IP SYN packets on the sending side:

         * Batch Count
       	 * Batch Delay

      Batch Count controls how many TCP/IP SYN packets are sent in succession
      in a tight loop.  Please note that each of those packets corresponds to a
      different destination port.

      Batch Delay controls the amount of time we pause between batch bursts.

      Combined usage of both of these rudimentary controls helps us in various
      scenarios like the following:

        * Evading "BASIC" TCP/IP SYN attack detectors
        * Avoiding severe packet loss because of congestion on egress
	    and also on the ingress side (when the ACK packets come back)

6.  Then the scanner waits for all of the sender threads to finish sending
      TCP/IP SYN packets.

      Essentially, we wait for the sender threads to join back the scanner.

7.  Then, the scanner marks the internal state indicating that all sender
      threads have completed sending the TCP/IP SYN packets.

      Prior to this completion on the sending side, the receiver threads are in
      a continuous select loop with 1 sec timeout reading in coming TCP/IP
      SYN-ACK packets.  The 1 sec time out is chosen arbitrarily and this helps
      us break out of the reading loop even if no packets arrive in the worst
      case scenario.  When the 1 sec time out occurs, if the senders have not
      finished sending yet, then the receivers go back into another 1 sec time
      out based select loop.  If the internal state indicates that senders have
      finished sending, then the the receivers go into a one more final cycle of
      select loop with a 1 sec timeout.  For the final select loop, by default,
      time out is chosen as 1 sec.  This is controllable via command line
      interface.  We do this one final select loop cycle so that the receiver
      threads get a chance to receive TCP/IP SYN-ACK packets to account for the
      situations like the following:
      
        * Arriving late due to network congestion
	* ACK s corresponding to SYN packets sent out during the last batches

8.  Then the scanner waits for all of the receiver threads to finish receiving
    TCP/IP SYN-ACK packets.

      Essentially, we wait for the receiver threads to join back the scanner.

9.  Then the scanner collects the results from each of the receiver instances.

      When the receiver threads are receiving TCP/IP SYN-ACK packets, each
      receiver stores the list of open ports individually to avoid
      synchronization.

      Please note that TCP/IP SYN-ACK packets from a destination can be
      received on any of the threads launched for the packet fanout group for
      the corresponding egress interface where the TCP/IP SYN packets were sent
      out.

      Also, at the same time, on a given packet fanout thread group, TCP/IP
      SYN-ACK packets corresponding to multiple destinations can be received
      since those destinations are reachable via given interface corresponding
      to the packet fanout group we launched.

10.  Once the open ports are collected from all of the threads from each of
       the thread groups, the results are merged per destination Ip Address.
       Then the open ports are sorted within each of the destination IP Adresses.
       Then they are displayed per destination with potential service name.

The current implementation takes the following input parameters:

  * Multiple IPV4 Addresses
      * via repeating "-ip" command line option
  * Port Range that need to be scanned
      * via "-p" command line option
  * Number of threads to be spawned per interface to receive TCP/IP SYN-ACK
    packets
      * via "-tps" command line option
  * Time that SYN-ACK receiver threads wait after SYN packet sender threads
    finish sending packets 
      * via "-t" command line option
  * Batch Count - number of SYN packets sent by senders in a burst (each of the
    packets corresponds to a separate TCP/IP port)
      * via "-bc" command line option
  * Batch Delay - Delay between packet bursts by the SYN packet sender threads
      * via "-bd" command line option

**SOURCE CODE**
---------------
___________

*  README.1st
     * This file

*  Makefile
     * A simple Makefile

*  RouteUtils.cpp
*  RouteUtils.h
     * A class containing Kernel Routing table lookups

*  SourceForDest.cpp
     * A simple test application invoking route utils to obtain
       preferred source, ifindex and gateway for a given destination
       ipv4 address

*  TcpSynAckPacketReceiver.cpp
*  TcpSynAckPacketReceiver.h
     * A class abstracting the TCP/IP SYN-ACK packer receiver side code

*  TcpSynPacketSender.cpp
*  TcpSynPacketSender.h
     * A Class abstracting the TCP/IP SYN Packet sender side code

*  TcpSynAckPortScanner.cpp
*  TcpSynAckPortScanner.h
     * A class abstracting the overall TCP/IP SYN/ACK based port scanner
       functionality

*  TcpSynAckPortScannerApplication.cpp
     * Top level scanner application invoking the TcpSynAckPortScanner  

**WHERE TO START**
--------------
______________

Pleaser start in the file TcpSynAckPortScannerApplication.cpp and walk through
the code for top down flow.


**BUILDING THE DEMO CODE**
----------------------
______________________

*  Optimized version
     * Issue the following command

       gmake

* Debug version
     * Issue the following command

       gmake debug

* Clean up
     *  Issue the following command:

        gmake clean

When Optmized/Debug version is compiled using the commands mentioned above,
the folliwng two binaries will be generated:

*  SourceForDest
     * A simple application that prints the following information for a given
         Destination IPV4 Address.

	 * Preferred Source IP Address via which the packets to the destination
	     will be sent out
	 * IF Index of the interface through which the destination can be
	   reached.
	 * Gateway through which the destination can be reached

*  TcpSynAckPortScannerApplication
     * This is the program of prime interest for this exercise.
     * Takes in one or more IPV4 Addresses and other command line options and
         produces a report of open ports per destination IPv4 Address.


**RUNNING THE DEMO CODE**
---------------------
_____________________

To run the demo, one requires super user privileges since we deal with
RAW sockets and PACKET sockets.

General usage help and a sample execution output are listed below

The binary TcpSynAckPortScannerApplication can be run with the following
command line options:

./TcpSynAckPortScannerApplication <-ip <IPV4 Address>> [[-ip <IPV4Address>]...] [-p <Port Range>] [-tps <Number Of Threads Per Source Interface>] [-t <Receiver Thread Time Out in Milli Seconds>] [-bc <Batch Count>] [-bd <Batch Delay in Milli Seconds>]

-ip  : Requires an IP V4 Address.
       At least one IPV4 Address must be specified.
       Multiple destination IPv4 Addresses can be specified by repeating this option.
       If any of the supplied IP Addresses is not a valid IPV4 Address, program errors out.

-p   : Requires a Port Range of the format <Start Port>-<End Port>.
       Start Port and End Port should be within the range [1, 65535].
       Start Port value should NOT be greater than that of End Port.
       Default port range if this option is not used is : 1-65535
       Example : 9-100 : will consider all ports including 9 and 100 and all the numbers in between.
                 23    : will be considered as 23-65535
                 23-   : will be considered as 23-65535
                 -10   : will fail the input since it will be read as a negative number
                 7-7   : will result in scanning only the port number 7

-tps : Requires number of threads to be spawned per egress interface (Port Fanout Group)).
       Please see README.1st for more details regarding the thread group and how they are used.
       Default value is 10.
       Example : 100 : Spawns 100 threads to read TCP/IP SYN-ACK packets for the port fanout group
                       corresponding to each the egress interfaces.

-t   : Requires timeout in Milli Seconds.
       This is the amount of time that the receiver threads will wait reading the
       TCP/IP SYN-ACK packets after sender threads have finished.

-bc  : Requires a batch count
       This is the number of TCP/IP SYN packets that are sent out in a burst by a Sender thread.
       Default value is 100.

-bd  : Requires a delay in Milli Seconds.
       This is the amount of time sender thread waits between sending TCP/IP SYN packet bursts.
       Default value is 20 Milli Seconds.

For more information regarding any of the input options, please consider going through the README.1st document.

Sample Execution Session
------------------------
sudo ./TcpSynAckPortScannerApplication -tps 20 -t 2000 -bc 100 -bd 20 -ip 10.255.138.201 -ip 10.255.138.201 -ip 192.168.122.1
[sudo] password for sagar: 
Packets to  10.255.138.201 are sent using source   172.16.179.77 on Interface Index     2 via gateway    172.16.179.2
Packets to   192.168.122.1 are sent using source   192.168.122.1 on Interface Index     1 via gateway         0.0.0.0
Launching Receiver Threads for Packet Fanout Groups to receive TCP/IP SYN-ACK packets.
(    1-140513789082016) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513771783936) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513763391232) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513754998528) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513746605824) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513738213120) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513729820416) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513721427712) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513713035008) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513704642304) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513696249600) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513687856896) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513679464192) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513671071488) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513662678784) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513654286080) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513645893376) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513637500672) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513629107968) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    1-140513620715264) Launching a thread to monitor TCP SYN/ACK packets coming back to source 172.16.179.77
(    2-140513612322560) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513603929856) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513595537152) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513587144448) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513578751744) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513570359040) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513561966336) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513553573632) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513545180928) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513536788224) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513528395520) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513520002816) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513511610112) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513503217408) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513494824704) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513486432000) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513478039296) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513469646592) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513461253888) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
(    2-140513452861184) Launching a thread to monitor TCP SYN/ACK packets coming back to source 192.168.122.1
Waiting for all of the Receiver threads to become active.
Launching Sender threads one per destination to send TCP/IP SYN packets.
Sending Packets to ports at destination  10.255.138.201 using source   172.16.179.77
Sending Packets to ports at destination   192.168.122.1 using source   192.168.122.1
Waiting for all of the sender threads to finish.
All Sender threads completed sending packets.
Waiting for all of the receiver threads to finish.
192.168.122.1 : 
    Number of Open Ports : 6
         PORT SERVICE
         ____ _______
           22 ssh
           53 domain
         9000 cslistener
         9090 websm
        10137 Unknown
        20080 Unknown

10.255.138.201 : 
    Number of Open Ports : 35
         PORT SERVICE
         ____ _______
           22 ssh
           53 domain
           80 http
           82 xfer
           83 mit-ml-dev
           85 Unknown
           87 Unknown
           89 su-mit-tg
           90 dnsix
           91 mit-dov
          139 netbios-ssn
          445 microsoft-ds
          555 dsf
          556 remotefs
          558 sdnskmp
          560 rmonitor
          562 chshell
          563 nntps
          564 9pfs
         1025 blackjack
         1026 cap
         8000 irdmi
         8002 teradataordbms
         8003 mcreport
         8005 mxi
         8007 Unknown
         8009 Unknown
         8010 Unknown
         8011 Unknown
         8200 trivnet1
        10000 ndmp
        10080 amanda
        49152 Unknown
        49153 Unknown
        52367 Unknown


**TODO**
----
____

* IPV6 Support
* UDP Support
* Streamlined error handling
    * Currently the ode exits on most errors.  But, could be enhanced to
      handle more gracefully.
    * most places  have same exit code crrently.  We can return more specific
      error codes.
* Auto detecting resource limits with respect to TCP/IP connection tracking and
  limiting inprogress connections at any given time.
* More flexible input handling
    * IP address blocks
    * Comma separated port ranges
    * timeouts in other units (other than milli seconds)
* Randomizing the port order while sending the SYN packets
* Much more ...

