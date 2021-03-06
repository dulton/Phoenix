#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define HAVE_REMOTE

#include <winsock2.h>
#include <string>
#include "portscan.h"
#include "windowutils.h"
#include "netdefine.h"
#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/ThreadPool.h"
#include "Poco/Thread.h"
#include "Poco/Runnable.h"
#include "Poco/Mutex.h"
#include "Poco/Random.h"
#include "Poco/AutoPtr.h"
#include "Poco/Delegate.h"
#include "PING.h"
#include "QMSqlite.h"
#include "log.h"

using Poco::Notification;
using Poco::NotificationQueue;
using Poco::ThreadPool;
using Poco::Thread;
using Poco::Runnable;
using Poco::FastMutex;
using Poco::AutoPtr;

FastMutex SendData::_mutex;


//PortScan::PortScan()
//{
//	_adhandle = NULL;
//	initPcapDev();
//	setDefaultScanPorts();
//	WindowUtils::getLocalIPsMac(_localIps, _localMac);
//}

PortScan::PortScan(NotificationQueue& queue) :
_queue(queue)
{
	_adhandle = NULL;
	initPcapDev();
	setDefaultScanPorts();
	WindowUtils::getLocalIPsMac(_localIps, _localMac);
}


PortScan::~PortScan()
{
	pcap_close(_adhandle);
	_adhandle = NULL;
}



void ReceiveData::analyzePacket(const u_char *pkt_data, u_short size, vector<long> ips, vector<u_short> ports, vector<SCANRESULT>& outIps)
{
	struct iphdr *ip = (struct iphdr *)(pkt_data + sizeof(struct ethhdr));
	struct tcphdr *tcp = (struct tcphdr *)(pkt_data + sizeof(struct ethhdr) + sizeof(struct iphdr));


	if ((htons(tcp->dport) == 23456) && (tcp->flag == 0x12))
	{
		in_addr srcaddr;
		srcaddr.S_un.S_addr = ip->sourceIP;
		string srcip(inet_ntoa(srcaddr));
				
		SCANRESULT out = { 0 };
		char *sip = inet_ntoa(srcaddr);
		memcpy(out.ip, sip, strlen(sip));
		out.port = htons(tcp->sport);
		outIps.push_back(out);

	}

}

bool PortScan::initPcapDev()
{
	pcap_if_t *alldevs;
	char errbuf[PCAP_ERRBUF_SIZE];
	u_int netmask;

	//get all net device
	if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1)
	{
		poco_error_f1(logger_handle, "Error in pcap_findalldevs: %s", string(errbuf));
		return false;
	}

	if (!alldevs)
	{
		poco_error(logger_handle, "cannot find net device!  install WinPcap?");
		return false;
	}

	poco_information_f2(logger_handle, "desc: %s, %s", string(alldevs->description), string(alldevs->name));

	//get local device
	string uuid = WindowUtils::getLocalUuid();

	string pcap_name = WindowUtils::ConvertNICUUIDtoPcapName(alldevs, uuid);

	if ((_adhandle = pcap_open_live(pcap_name.data(), 65536, 1, 1000, errbuf)) == NULL)
	{
		poco_error(logger_handle, "kevin : pcap_open_live failed!  not surpport by WinPcap ? alldev->name : %1");
		pcap_freealldevs(alldevs);
		return false;
	}

	if (pcap_datalink(_adhandle) != DLT_EN10MB || alldevs->addresses == NULL) {
		poco_error(logger_handle, "kevin : pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL");
		return false;
	}

	//set netmask
	netmask = ((struct sockaddr_in *)(alldevs->addresses->netmask))->sin_addr.S_un.S_addr;
	pcap_freealldevs(alldevs);


	//build filter, only capture tcp packet
	char packet_filter[] = "ip and tcp";
	struct bpf_program fcode;

	if (pcap_compile(_adhandle, &fcode, packet_filter, 1, netmask) < 0)
	{
		poco_error(logger_handle, "unable to compile the packet filter.Check the syntax.");
		return false;
	}

	//set filter
	if (pcap_setfilter(_adhandle, &fcode) < 0)
	{
		poco_error(logger_handle, "Error setting the filter.");
		return false;
	}

	return true;
}

bool PortScan::searchFactory(vector<u_short> ports, vector<SCANRESULT>& outIps)
{

	int i, j;
	NotificationQueue queue;

	outIps.clear();


	vector<string> Ips;
	std::shared_ptr<bool> bpCancel = std::make_shared<bool>(false);

	CPing::instance().ScanIp(Ips, false, bpCancel);	;
	poco_information_f1(logger_handle, "ping ip size: %d", (int)Ips.size());

	//get network mac address
	vector<IPMAC> IpMacs;
	if (Ips.size() > 0)
	{
		WindowUtils::getMacByArpTable(Ips, IpMacs);
	}	
	poco_information_f1(logger_handle, " get mac size: %d", (int)IpMacs.size());

	vector <SendData> sendThreads;
	for (i = 0; i < _localIps.size(); i++)
	{
		IPMAC localIpMac = { 0 };
		localIpMac.ip = inet_addr(_localIps[i].c_str());
		memcpy(localIpMac.mac, _localMac, 6);
		for (j = 0; j < IpMacs.size(); j++)
		{
			IPMAC dstMac = { 0 };
			dstMac.ip = IpMacs[j].ip;
			memcpy(dstMac.mac, IpMacs[j].mac, 6);
			in_addr inaddr;
			inaddr.S_un.S_addr = dstMac.ip;
			string dstIp = inet_ntoa(inaddr);
			if (isNetworkSegment(_localIps[i], dstIp))
			{
				SendData sendthread(localIpMac, dstMac, ports, queue, _adhandle);
				sendThreads.push_back(sendthread);
			}
		}
	}	
	poco_information_f1(logger_handle, "sendThreads size: %d", (int)sendThreads.size());

	Poco::ThreadPool scanPool;
	scanPool.addCapacity(sendThreads.size());

	for (i = 0; i < sendThreads.size(); i++)
	{
		scanPool.start(sendThreads[i]);
	}

	vector<long> destIps;
	for (i = 0; i < IpMacs.size(); i++)
	{
		destIps.push_back(IpMacs[i].ip);
	}
	//start listen thread
	ReceiveData receiveThread(destIps, ports, _adhandle);
	scanPool.start(receiveThread);

	//send scan packet
	for (int i = 0; i < sendThreads.size(); ++i)
	{
		queue.enqueueNotification(new ScanNotification(1));
	}

	while (!queue.empty())
		Thread::sleep(200);
	Thread::sleep(1000 * 9);

	//close send and listen thread
	queue.wakeUpAll();
	_theEvent += Poco::delegate(&receiveThread, &ReceiveData::onEvent);
	fireEvent(true);
	_theEvent -= Poco::delegate(&receiveThread, &ReceiveData::onEvent);
	scanPool.joinAll();

	//get scan result
	outIps = receiveThread.getScanResult();	
	poco_information_f1(logger_handle, "scan result size: %d", (int)outIps.size());

	std::sort(outIps.begin(), outIps.end(), sortByIp);
	std::unique(outIps.begin(), outIps.end(), UniqueByIp);

	writeDb();

	return true;
}

//build tcp head and ip head
void SendData::initHeader(struct iphdr *ip, struct tcphdr *tcp, struct pseudohdr *pseudoheader, long dst_ip, USHORT dst_port)
{
	int len = sizeof(struct iphdr) + sizeof(struct tcphdr);
	//init IP head
	ip->hl = (4 << 4 | sizeof(struct iphdr) / sizeof(unsigned int));
	ip->tos = 0;
	ip->total_len = htons(len);
	ip->id = 1;
	ip->frag_and_flags = 0x40;
	ip->ttl = 128;
	ip->proto = IPPROTO_TCP;
	ip->checksum = 0;
	//ip->sourceIP = 0;
	ip->destIP = dst_ip;

	// init TCP head
	tcp->sport = htons(23456);
	tcp->dport = htons(dst_port);
	tcp->seq = htonl(rand() % 90000000 + 2345);
	tcp->ack = 0;
	tcp->lenres = (sizeof(struct tcphdr) / 4 << 4 | 0);
	tcp->flag = 0x02;
	tcp->win = htons(32768);
	tcp->sum = 0;
	tcp->urp = 0;

	//pseudo TCP head
	pseudoheader->zero = 0;
	pseudoheader->protocol = IPPROTO_TCP;
	pseudoheader->length = htons(sizeof(struct tcphdr));
	pseudoheader->daddr = dst_ip;
	srand((unsigned)time(NULL));

}

//create send packet
void SendData::buildTcpPacket(IPMAC srcip, IPMAC dstip, vector<u_short> ports, vector<SendPacket>& packets)
{
	int  i;
	struct iphdr ip;		//IP head
	struct tcphdr tcp;		//TCP head
	struct pseudohdr pseudoheader;	//pseudo TCP head



	for (i = 0; i < ports.size(); i++)
	{
		char data[100] = { 0 };
		ip.sourceIP = srcip.ip;
		initHeader(&ip, &tcp, &pseudoheader, dstip.ip, ports[i]);
		memcpy(data, &ip, sizeof(struct iphdr));
		ip.checksum = checksum((u_short *)data, sizeof(struct iphdr));

		pseudoheader.saddr = ip.sourceIP;

		//tcp check sum
		memset(data, 0, sizeof(data));
		memcpy(data, &pseudoheader, sizeof(pseudoheader));
		memcpy(data + sizeof(pseudoheader), &tcp, sizeof(struct tcphdr));
		tcp.sum = checksum((u_short *)data, sizeof(pseudoheader)+sizeof(struct tcphdr));

		memset(data, 0, sizeof(data));
		unsigned short ethlen = htons(ETH_P_IP);
		memcpy(data, dstip.mac, ETH_ALEN);
		memcpy(data + ETH_ALEN, srcip.mac, ETH_ALEN);
		memcpy(data + ETH_ALEN * 2, &ethlen, sizeof(short));
		memcpy(data + sizeof(struct ethhdr), &ip, sizeof(struct iphdr));
		memcpy(data + sizeof(struct ethhdr) + sizeof(struct iphdr), &tcp, sizeof(struct tcphdr));

		SendPacket senddata(data, sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct tcphdr));
		packets.push_back(senddata);
	}

}

void SendData::send(pcap_t * adhandle, vector<SendPacket> packets)
{
	int i;

	for (i = 0; i < packets.size(); i++)
	{
		if (pcap_sendpacket(adhandle, (const u_char *)packets[i].getPacket(), packets[i].getPacketSize()) != 0)
		{			
			poco_error_f1(logger_handle, "send error: %s", string(pcap_geterr(adhandle)));
		}
	}
}

void SendData::run()
{
	Poco::Random rnd;
	for (;;)
	{
		Notification::Ptr pNf(_queue.waitDequeueNotification());
		if (pNf)
		{
			ScanNotification::Ptr pWorkNf = pNf.cast<ScanNotification>();
			if (pWorkNf)
			{
				{
					FastMutex::ScopedLock lock(_mutex);
					vector<SendPacket> packets;
					buildTcpPacket(_srcIp, _dstIp, _ports, packets);
					send(_adhandle, packets);
				}
				Thread::sleep(rnd.next(200));
			}
		}
		else
		{
			break;
		}
	}
}

void ReceiveData::run()
{

	struct pcap_pkthdr * header;
	const u_char * pkt_data;

	while (true)
	{

		if (_break)
		{
			poco_information(logger_handle, "listen is break");
			break;
		}

		//get all tcp packet
		if (pcap_next_ex(_adhandle, &header, &pkt_data) == 0){
			continue;
		}

		//analyze tcp packet
		analyzePacket(pkt_data, header->len, _ips, _ports, _outIps);

	}
}

vector<SCANRESULT> ReceiveData::getScanResult()
{

	return _outIps;
}

void ReceiveData::onEvent(const void* pSender, bool& arg)
{
	poco_information_f1(logger_handle, "onEvent: %d", (int)arg);
	_break = arg;
}

bool PortScan::sortByIp(SCANRESULT srFirst, SCANRESULT srSecond)
{
	unsigned long lIp1 = inet_addr(srFirst.ip);
	unsigned long lIp2 = inet_addr(srSecond.ip);
	if (lIp1 == lIp2)
	{
		return srFirst.port < srSecond.port;
	}
	else
	{
		return (lIp1 < lIp2);
	}
}

bool PortScan::UniqueByIp(SCANRESULT srFirst, SCANRESULT srSecond)
{
	unsigned long lIp1 = inet_addr(srFirst.ip);
	unsigned long lIp2 = inet_addr(srSecond.ip);

	return (lIp1 == lIp2) && (srFirst.port == srSecond.port);	
}

bool PortScan::isNetworkSegment(string srcIP, string dstIP)
{
	string strSrc = srcIP;
	string strDst = dstIP;
	strSrc = strSrc.substr(0, strSrc.find_last_of(".") + 1);
	strDst = strDst.substr(0, strDst.find_last_of(".") + 1);

	return (strSrc.compare(strDst) == 0);
}



void PortScan::run()
{
	time_t t_start, t_end;
	t_start = time(NULL);
	searchFactory(_scanPorts, _outReuslts);
	t_end = time(NULL);
	poco_information_f1(logger_handle, "0 time: %.0f s\n", difftime(t_end, t_start));

	_queue.enqueueNotification(new ScanNotification(0));
}

bool PortScan::writeDb()
{
	int i;

	try
	{
		QMSqlite *pDb = QMSqlite::getInstance();

		pDb->dropTable(DROP_SCAN_PORT_TABLE);

		pDb->createTable(CREATE_SCAN_PORT_TABLE);

		vector<ScanPortRecord> scanResults;
		for (i = 0; i < _outReuslts.size(); i++)
		{
			ScanPortRecord scanRecord(_outReuslts[i].ip, _outReuslts[i].port);
			scanResults.push_back(scanRecord);
		}
		pDb->writeDataByVector(INSERT_SCAN_PORTE, scanResults);
	}
	catch (DatabaseException& ex)
	{
		poco_error_f1(logger_handle, " error: %s", ex.displayText());
	}


	return true;
}

PortScan::PortScan(vector<u_short> ports, NotificationQueue& queue) :
_queue(queue)
{
	_adhandle = NULL;
	initPcapDev();
	InitPorts(ports);
	WindowUtils::getLocalIPsMac(_localIps, _localMac);
}

void PortScan::InitPorts(vector<u_short> ports)
{
	if (ports.empty())
	{
		setDefaultScanPorts();
	}
	else
		_scanPorts = ports;
}

void PortScan::setDefaultScanPorts()
{
	//da hua
	_scanPorts.push_back(37777);
	//di zhi pu
	_scanPorts.push_back(34567);
	//jia xing jie
	_scanPorts.push_back(3321);
	//bao xing shen, hai kang, wo shi da, xing peng an fang
	_scanPorts.push_back(8000);
	//lan she xing ji
	_scanPorts.push_back(3721);
	//bo li
	_scanPorts.push_back(9000);
	//da li ke ji
	_scanPorts.push_back(9998);
	//dong yang guo ji
	_scanPorts.push_back(6001);
	//han bang
	_scanPorts.push_back(8101);
	//hang jin ke ji
	_scanPorts.push_back(7777);
	//jiu an guang dian
	_scanPorts.push_back(80);
	//jun ming shi
	_scanPorts.push_back(8670);
	//tian di wei ye
	_scanPorts.push_back(3001);
	//tong wei
	_scanPorts.push_back(6036);
	//xing wang rui jie
	_scanPorts.push_back(8081);
	//yu shi ke ji
	_scanPorts.push_back(82);
	//zhong wei
	_scanPorts.push_back(9101);
}