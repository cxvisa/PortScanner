COMPLIERFLAGS     := --ansi --pedantic -Werror -Wall
DEBUGFLAGS        := -g3
OPTIMIZATIONFLAGS := -O3

all:
	g++ $(COMPLIERFLAGS) $(OPTIMIZATIONFLAGS) -o TcpSynAckPortScannerApplication RouteUtils.cpp TcpSynPacketSender.cpp TcpSynAckPacketReceiver.cpp TcpSynAckPortScanner.cpp  TcpSynAckPortScannerApplication.cpp -lpthread
	g++ $(COMPLIERFLAGS) $(OPTIMIZATIONFLAGS) -o SourceForDest  RouteUtils.cpp TcpSynPacketSender.cpp TcpSynAckPacketReceiver.cpp TcpSynAckPortScanner.cpp  SourceForDest.cpp -lpthread

debug:
	g++ $(COMPLIERFLAGS) $(DEBUGFLAGS) -o TcpSynAckPortScannerApplication RouteUtils.cpp TcpSynPacketSender.cpp TcpSynAckPacketReceiver.cpp TcpSynAckPortScanner.cpp  TcpSynAckPortScannerApplication.cpp -lpthread
	g++ $(COMPLIERFLAGS) $(DEBUGFLAGS) -o SourceForDest  RouteUtils.cpp TcpSynPacketSender.cpp TcpSynAckPacketReceiver.cpp TcpSynAckPortScanner.cpp  SourceForDest.cpp -lpthread

clean:
	rm -rf TcpSynAckPortScannerApplication;
	rm -rf SourceForDest;
