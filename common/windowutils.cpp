#include "stdafx.h"
#include "windowutils.h"
#include "pcap.h"
#include <windows.h>
#include <Winsock2.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include "PING.h"
#include <IPTypes.h>
#include "utils.h"
#include <iostream>
#include <algorithm>
#include <map>
#include <tchar.h>
#include <Shellapi.h>
#include <NetCon.h>  
#include "log.h"



#pragma comment(lib,"iphlpapi.lib")

inline bool caseInsCharCompSingle(char a, char b)
{
	return toupper(a) == b;
}

string::const_iterator caseInsFind(string& s, const string& p)
{
	string tmp;	 
	transform(p.begin(), p.end(), back_inserter(tmp), (int(*)(int))toupper);
	return (search(s.begin(), s.end(), tmp.begin(), tmp.end(), caseInsCharCompSingle));
}

void GetLocalNetCar(std::string& Desc, std::string& AdapterName, std::vector<string>& IPList, u_char *Macaddr)
{
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	int netCardNum = 0;
	int IPnumPerNetCard = 0;
	string::const_iterator it;
	std::string sNetName;
	PIP_ADAPTER_INFO pfirstIpAdapterInfo;

	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		delete pIpAdapterInfo;
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		pfirstIpAdapterInfo = pIpAdapterInfo;
		nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	}
	else
	{
		pfirstIpAdapterInfo = pIpAdapterInfo;
	}
		if (ERROR_SUCCESS == nRel)
		{
			while (pIpAdapterInfo)
			{
				switch (pIpAdapterInfo->Type)
				{
				case MIB_IF_TYPE_OTHER:
					break;
				case MIB_IF_TYPE_ETHERNET:
					sNetName = std::string(pIpAdapterInfo->Description);
					it = caseInsFind(sNetName, string("pci"));
					if (it != sNetName.end())
					{
						Desc = sNetName;
						AdapterName = pIpAdapterInfo->AdapterName;
						IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
						do
						{
							IPList.push_back(std::string(pIpAddrString->IpAddress.String));							
							poco_information_f1(logger_handle, "IP address : %s", string(pIpAddrString->IpAddress.String));
							
							pIpAddrString = pIpAddrString->Next;
						} while (pIpAddrString);

						//copy mac
						for (int i = 0; i < 6; i++)
						{
							Macaddr[i] = pIpAdapterInfo->Address[i];
						}						
					}					
					
					break;
				case MIB_IF_TYPE_TOKENRING:
					break;
				case MIB_IF_TYPE_FDDI:
					break;
				case MIB_IF_TYPE_PPP:
					break;
				case MIB_IF_TYPE_LOOPBACK:
					break;
				case MIB_IF_TYPE_SLIP:
					break;
				default:

					break;
				}			
			
				pIpAdapterInfo = pIpAdapterInfo->Next;				
			}
		}		
		delete[] pfirstIpAdapterInfo;		
		
}


//get connect name 
string GetNetCardName(std::string uuid)
{
	string CardName;
	HKEY hKey;
	string strKeyName = "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\";
	strKeyName += uuid;
	strKeyName += "\\Connection";
	LONG lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, strKeyName.c_str(), 0, KEY_QUERY_VALUE, &hKey);
	if (lRet != ERROR_SUCCESS)
		return CardName;

	char sz[20] = { 0 };
	DWORD dwSize = sizeof(sz);
	if (RegQueryValueExA(hKey, "Name", NULL, NULL, (LPBYTE)sz, &dwSize) == ERROR_SUCCESS)
	{		
		CardName = string(sz);
	}

	
	return CardName;
}

string WindowUtils::ConvertNICUUIDtoPcapName(pcap_if_t* devs, const string& uuid)
{
	string pcap_name;
	
	for (pcap_if_t *d = devs; d; d = d->next)
	{
		string tmp(d->name);		
		if (/*tmp.compare(uuid) == 0*/tmp.find(uuid) != string::npos)
		{
			pcap_name = tmp;
			break;
		}
			
	}

	return pcap_name;
}

WindowUtils::WindowUtils()
{
}


bool WindowUtils::isValidNetMacaddress(const string& macaddress)
{

	return !macaddress.empty() && (macaddress.find("00:00:00:00:00:00:00:E0") == string::npos)
            && (macaddress.find("00:50:56:C0") == string::npos);
}

void WindowUtils::GetIPfromLocalNIC(std::vector<string> &IPs)
{
	getLocalIPs(IPs);
}

void WindowUtils::getLocalIPs(std::vector<string> &IPs)
{
    IPs.clear();
	string sDesc;
	string uuid;
	u_char mac[6] = { 0 };
	GetLocalNetCar(sDesc, uuid, IPs, mac);
}


bool WindowUtils::setNetConfig(const string& sName, const string& sIP, const string& sMask, const string& sGate, bool bWait){

	string mask(" mask=");
	mask.append(sMask);
	string sNetName = GetNetCardName(sName);
	string name(" name = \"");
	name += sNetName;
	name.append("\"");
	string addr(" addr=");
	addr += sIP;
	string argList; 
	
    if (!sGate.empty())
    {        
		argList = "interface ip set address";
		argList += name;
		argList.append(" source = static ");
		argList = argList + addr + mask;
		string gateway(" gateway=");
		gateway += sGate;
		argList += gateway;    
		
	}
	else
	{
		argList = "interface ip add address";
		argList += name;
		argList = argList + addr + mask;
	}
	int nRun = (int)::ShellExecuteA(NULL, (LPCSTR)"open", (LPCSTR)"netsh.exe", (LPCSTR)argList.c_str(), NULL, SW_HIDE);
	if (nRun <= 32)
		return false;
   
    /*if (!bWait)
    {
        return true;
    }
    int maxPingTime = 1000 * 3;
    ::Sleep(2000);
	while (maxPingTime > 0 && !CPing::instance().Ping(sGate.c_str(), 8)){
        ::Sleep(1000);
        maxPingTime -= 1000;
    }
    
    return maxPingTime > 0;*/
	return true;
}

bool WindowUtils::setNetDhcp(const string& sName){  
	string sCmd("interface ip set address name=\"");
	sCmd += sName;
	sCmd.append("\" source=dhcp");
	int nRun = (int)::ShellExecuteA(NULL, (LPCSTR)"open", (LPCSTR)"netsh.exe", (LPCSTR)sCmd.c_str(), NULL, SW_HIDE);
	if (nRun <= 32)
		return false;
    return true;
}

bool WindowUtils::isConnecteTo(const string& IP, int millSeconds){
    return CPing::instance().Ping(IP.c_str(), millSeconds);
}


typedef struct arppkt
{
    unsigned short hdtyp;//hd type
    unsigned short protyp;//protocol type
    unsigned char hdsize;//hd length
    unsigned char prosize;//protocol length
    unsigned short op;//op type
    u_char smac[6];//src MAC address
    u_char sip[4];//src IP
    u_char dmac[6];//dest MAC address
    u_char dip[4];//dest IP
}arpp;


#define  INNDER_SPECIAL_IP "170.151.24.203"
bool WindowUtils::getDirectDevice(set<string>& ipList, set<string>& dstList)
{
	ipList.clear();	

	int i = 0;
	pcap_if_t * alldevs;//pcap_if_t list point
	pcap_t * adhandle;//define get packet handle
	char errbuf[PCAP_ERRBUF_SIZE];
	u_int netmask; //define netmask 
	char packet_filter[] = "ether proto \\arp";
	struct bpf_program fcode;
	struct pcap_pkthdr * header;
	const u_char * pkt_data;	

	//get all net device
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{		
		poco_error_f1(logger_handle, "Error in pcap_findalldevs:%s", string(errbuf));
		return false;
	}

	if (!alldevs)
	{		
		poco_error(logger_handle, "cannot find net device!  install WinPcap?");
		return false;
	}
		
	poco_information_f2(logger_handle, "desc: %s, name: %s", string(alldevs->description), string(alldevs->name));

	if ((adhandle = pcap_open_live(alldevs->name, 65536, 1, 1000, errbuf)) == NULL)
	{		
		poco_error_f1(logger_handle, "pcap_open_live failed!  not surpport by WinPcap : %s", string(alldevs->name));
		pcap_freealldevs(alldevs);
		return false;
	}

	if (pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL) {		
		poco_error(logger_handle, "pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL");
		return false;
	}


	netmask = ((struct sockaddr_in *)(alldevs->addresses->netmask))->sin_addr.S_un.S_addr;
	pcap_freealldevs(alldevs);


	//build filter, only get ARP packet
	if (pcap_compile(adhandle, &fcode, packet_filter, 1, netmask) < 0)
	{		
		poco_error(logger_handle, "unable to compile the packet filter.Check the syntax.");
		return false;
	}

	//set filter
	if (pcap_setfilter(adhandle, &fcode) < 0)
	{
		poco_error(logger_handle, "Error setting the filter.");
		return false;
	}
	std::vector<string> IPs;
	getLocalIPs(IPs);
	const int nMaxSeconds = 5;
	int start = GetTickCount();
	string specialIP;
	while (true)
	{
		if (GetTickCount() - start > nMaxSeconds * 1000)
		{
			poco_information(logger_handle, "arp time out");
			break;
		}
		//get arp packet
		if (pcap_next_ex(adhandle, &header, &pkt_data) == 0){
			continue;
		}
		arpp* arph = (arpp *)(pkt_data + 14);
		if (htons(arph->op) == 1) //arp
		{
			if (arph->sip[0] == 0)
			{
				continue;
			}
			if (arph->sip[0] == 169 && arph->sip[1] == 254)
			{
				continue;
			}

			char szIP[30] = { 0 };
			sprintf_s(szIP, "%d.%d.%d.%d", arph->sip[0], arph->sip[1], arph->sip[2], arph->sip[3]);			
			poco_information_f1(logger_handle, "ip 0: %s", string(szIP));

			string source = string(szIP);
			if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
				continue;
			}
			
			ipList.insert(source);			
		}
		else
		{
			if (arph->sip[0] == 0)
			{
				continue;
			}
			if (arph->sip[0] == 169 && arph->sip[1] == 254)
			{
				continue;
			}

			char szsource[30] = { 0 };
			sprintf_s(szsource, "%d.%d.%d.%d", arph->sip[0], arph->sip[1], arph->sip[2], arph->sip[3]);			
			poco_information_f1(logger_handle, "ip 6: %s", string(szsource));
			string source = string(szsource);
			if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
				continue;
			}			

			dstList.insert(source);			

			break;
		}
	}
	if (adhandle)
		pcap_close(adhandle);

	return !ipList.empty();
}

bool WindowUtils::getDirectDevice(string& ip, string& netGate)
{
    ip.clear();

    int i = 0;
    pcap_if_t * alldevs;//pcap_if_t list point
    pcap_t * adhandle;//define get packet handle
    char errbuf[PCAP_ERRBUF_SIZE];
    u_int netmask; //define netmask 
    char packet_filter[] = "ether proto \\arp";
    struct bpf_program fcode;
    struct pcap_pkthdr * header;
    const u_char * pkt_data;
    
    //get all net device
    if (pcap_findalldevs(&alldevs, errbuf) == -1)
    {
		poco_error(logger_handle, "Error in pcap_findalldevs:");
        return false;
    }

    if (!alldevs)
    {
		poco_error(logger_handle, "cannot find net device!  install WinPcap?");
        return false;
    }
   
	poco_information_f2(logger_handle, "desc: %s, name: %s", string(alldevs->description), string(alldevs->name));

    if ((adhandle = pcap_open_live(alldevs->name, 65536, 1, 1000, errbuf)) == NULL)
    {
		poco_error(logger_handle, "pcap_open_live failed!  not surpport by WinPcap ?");
        pcap_freealldevs(alldevs);
        return false;
    }

    if (pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL) {       
		poco_error(logger_handle, "pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL");
        return false;
    }


    netmask = ((struct sockaddr_in *)(alldevs->addresses->netmask))->sin_addr.S_un.S_addr;
    pcap_freealldevs(alldevs);
    

    //build filter, only get ARP packet
    if (pcap_compile(adhandle, &fcode, packet_filter, 1, netmask) < 0)
    {
		poco_error(logger_handle, "unable to compile the packet filter.Check the syntax.");
        return false;
    }

    //set filter
    if (pcap_setfilter(adhandle, &fcode) < 0)
    {
		poco_error(logger_handle, "Error setting the filter.");
        return false;
    }
    std::vector<string> IPs;
    getLocalIPs(IPs);
    const int nMaxSeconds = 8;
    int start = GetTickCount();
    string specialIP;
    while (true)
    {
		if (GetTickCount() - start > nMaxSeconds * 1000)
        {
			poco_information(logger_handle, "arp time out");
            break;
        }
        //get arp packet
        if (pcap_next_ex(adhandle, &header, &pkt_data) == 0){
            continue;
        }
        arpp* arph = (arpp *)(pkt_data + 14);
        if (arph->op == 256) //arp
        {
            if (arph->sip[0] == 0)
            {
                continue;
            }
            if (arph->sip[0] == 169 && arph->sip[1] == 254)
            {
                continue;
            }

			char szIP[30] = { 0 };
			sprintf_s(szIP, "%d.%d.%d.%d", arph->sip[0], arph->sip[1], arph->sip[2], arph->sip[3]);
			poco_information_f1(logger_handle, "ip 0: %s", string(szIP));
			
			string source = string(szIP);
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }
            if (!Utils::isInnerIP(source))
            {
                continue;
            }
            netGate = source;
            if (ip.empty())
            {
                start = GetTickCount() - 7 * 1000;
            }

			char szdIP[30] = { 0 };
			sprintf_s(szdIP, "%d.%d.%d.%d", arph->dip[0], arph->dip[1], arph->dip[2], arph->dip[3]);
			poco_information_f1(logger_handle, "ip 1: %s", string(szdIP));
			
			ip = string(szdIP);
            if (arph->dip[0] != arph->sip[0] || arph->dip[1] != arph->sip[1] || arph->dip[2] != arph->sip[2] || ip == netGate)
            {
                if (arph->sip[3] != 44)
                {
					char sztmp[30] = { 0 };
					sprintf_s(sztmp, "%d.%d.%d.44", arph->sip[0], arph->sip[1], arph->sip[2]);
					ip = string(sztmp);
					poco_information_f1(logger_handle, "ip 2: %s", ip);
                }
                else{
					char sztmp[30] = { 0 };
					sprintf_s(sztmp, "%d.%d.%d.254", arph->sip[0], arph->sip[1], arph->sip[2]);
					ip = string(sztmp);
					poco_information_f1(logger_handle, "ip 3: %s", ip);
                }
            }
            
			poco_information_f2(logger_handle, "arp: %s, gate: %s", ip, netGate);
        }
        else
        {
            if (arph->dip[0] == 0)
            {
                continue;
            }
            if (arph->dip[0] == 169 && arph->dip[1] == 254)
            {
                continue;
            }

			char szsource[30] = { 0 };
			sprintf_s(szsource, "%d.%d.%d.%d", arph->sip[0], arph->sip[1], arph->sip[2], arph->sip[3]);			
			poco_information_f1(logger_handle, "ip 3: %s", string(szsource));
			string source = string(szsource);
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }

            if (!Utils::isInnerIP(source))
            {
                continue;
            }

            netGate = source;
            if (arph->sip[3] != 44)
            {
				char sztmp[30] = { 0 };
				sprintf_s(sztmp, "%d.%d.%d.44", arph->sip[0], arph->sip[1], arph->sip[2]);
				ip = string(sztmp);
				poco_information_f1(logger_handle, "ip 4: %s", ip);
            }
            else{
				char sztmp[30] = { 0 };
				sprintf_s(sztmp, "%d.%d.%d.254", arph->sip[0], arph->sip[1], arph->sip[2]);
				ip = string(sztmp);				
				poco_information_f1(logger_handle, "ip 5: %s", ip);
            }
           
            break;
        }
    }
	if (adhandle)
		pcap_close(adhandle);

    return !ip.empty();	
}

bool WindowUtils::getDirectDevice(string& ip, string& netGate, std::set<string>& otherIPS, long secondsWait){

    ip.clear();
   
    int i = 0;
    pcap_if_t * alldevs;//pcap_if_t list point
    pcap_t * adhandle;//define get packet handle
    char errbuf[PCAP_ERRBUF_SIZE];
    u_int netmask; //define netmask
    char packet_filter[] = "ether proto \\arp";
    struct bpf_program fcode;
    struct pcap_pkthdr * header;
    const u_char * pkt_data;

    //get all net device
    if (pcap_findalldevs(&alldevs, errbuf) == -1)
    {        
		poco_error_f1(logger_handle, "Error in pcap_findalldevs: %s", string(errbuf));
        return false;
    }

    if (!alldevs)
    {
		poco_error(logger_handle, "cannot find net device!  install WinPcap?");
        return false;
    }
   	
	poco_information_f2(logger_handle, "desc: %s, name: %s", string(alldevs->description), string(alldevs->name));
	
	string uuid = WindowUtils::getLocalUuid();
	
	string pcap_name = ConvertNICUUIDtoPcapName(alldevs, uuid);

	if ((adhandle = pcap_open_live(pcap_name.data(), 65536, 1, 1000, errbuf)) == NULL)
    {
		poco_error(logger_handle, "kevin : pcap_open_live failed!  not surpport by WinPcap ? alldev->name : ");
        pcap_freealldevs(alldevs);
        return false;
    }

    if (pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL) {
		poco_error(logger_handle, "kevin : pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL");
        return false;
    }


    netmask = ((struct sockaddr_in *)(alldevs->addresses->netmask))->sin_addr.S_un.S_addr;
    pcap_freealldevs(alldevs);
    std::map<string, std::set<string>> mpDestSource;

    //build filter, only get ARP packet
    if (pcap_compile(adhandle, &fcode, packet_filter, 1, netmask) < 0)
    {
		poco_error(logger_handle, "unable to compile the packet filter.Check the syntax.");
        return false;
    }

    //set filter
    if (pcap_setfilter(adhandle, &fcode) < 0)
    {
		poco_error(logger_handle, "Error setting the filter.");
        return false;
    }
    string specialIP;
    std::vector<string> IPs;
    getLocalIPs(IPs);
    long start = GetTickCount();
    const int MAX_WAIT_OTHER_IP_SECONDS = 10;
    while (true)
    {
        if (GetTickCount() - start > secondsWait * 1000)
        {
			poco_information(logger_handle, "arp time out");
			pcap_close(adhandle);
            break;
        }
        //get arp packet
        if (pcap_next_ex(adhandle, &header, &pkt_data) == 0){
            continue;
        }

        arpp* arph = (arpp *)(pkt_data + 14);
        if (arph->op == 256) //arp
        {
            if (arph->sip[0] == 0)
            {
                continue;
            }
            if (arph->sip[0] == 169 && arph->sip[1] == 254)
            {
                continue;
            }

			char szsip[30] = { 0 };
			sprintf_s(szsip, "%s.%s.%s.%s", arph->sip[0], arph->sip[1], arph->sip[2], arph->sip[3]);
			string source(szsip);
			char szdip[30] = { 0 };
			sprintf_s(szdip, "%s.%s.%s.%s", arph->dip[0], arph->dip[1], arph->dip[2], arph->dip[3]);
            string destIP(szdip) ;
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }
            if (source == INNDER_SPECIAL_IP)
            {
                specialIP = destIP;
                continue;
            }
            if (!Utils::isInnerIP(source))
            {
                continue;
            }
            
            if (netGate.empty())
            {
                netGate = source;
            }
            
            if (ip.empty() && secondsWait * 1000 - GetTickCount() + start > MAX_WAIT_OTHER_IP_SECONDS * 1000)
            {
                start = GetTickCount() - (secondsWait - MAX_WAIT_OTHER_IP_SECONDS) * 1000;
                ip = destIP;
                if (arph->dip[0] != arph->sip[0] || arph->dip[1] != arph->sip[1] || arph->dip[2] != arph->sip[2] || ip == netGate)
                {
                    if (arph->sip[3] != 1)
                    {
						char sztmp[30] = { 0 };
						sprintf_s(sztmp, "%s.%s.%s.1", arph->sip[0], arph->sip[1], arph->sip[2]);
						ip = string(sztmp);
                    }
                    else{
						char sztmp[30] = { 0 };
						sprintf_s(sztmp, "%s.%s.%s.254", arph->sip[0], arph->sip[1], arph->sip[2]);
						ip = string(sztmp);
                    }
                }
            }
            
            otherIPS.insert(source);
            if (Utils::isInnerIP(destIP))
            {
                mpDestSource[destIP].insert(source);
            }
 
			poco_information_f2(logger_handle, "arp : %s, %s", source, destIP);
        }
        else
        {
            if (arph->dip[0] == 0)
            {
                continue;
            }
            if (arph->dip[0] == 169 && arph->dip[1] == 254)
            {
                continue;
            }
            
			char szsource[30] = { 0 };
			sprintf_s(szsource, "%s.%s.%s.%s", arph->sip[0], arph->sip[1], arph->sip[2], arph->sip[3]);
            string source(szsource);
			char szdest[30] = { 0 };
			sprintf_s(szdest, "%s.%s.%s.%s", arph->dip[0], arph->dip[1], arph->dip[2], arph->dip[3]);
			string dest(szdest);
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }
            if (!Utils::isInnerIP(source))
            {
                continue;
            }

            netGate = source;
            otherIPS.insert(source);
            
            if (ip.empty() && secondsWait * 1000 - GetTickCount() + start > MAX_WAIT_OTHER_IP_SECONDS * 1000)
            {
                start = GetTickCount() - (secondsWait - MAX_WAIT_OTHER_IP_SECONDS) * 1000;
            }
			char sztmp[30] = { 0 };
			sprintf_s(sztmp, "%s.%s.%s.44", arph->sip[0], arph->sip[1], arph->sip[2]);
			ip = string(sztmp);
			poco_information_f2(logger_handle, "aarp :%s, %s", source, dest);
        }

    }
    
    std::map<string, std::set<string>>::iterator itNetgate = mpDestSource.end();
    for (auto it = mpDestSource.begin(); it != mpDestSource.end(); it++)
    {
        if (it->second.size() > 1 &&
            (itNetgate == mpDestSource.end() || itNetgate->second.size() < it->second.size()))
        {
            itNetgate = it;
        }
    }
    if (itNetgate != mpDestSource.end())
    {
        netGate = itNetgate->first;		
        string s = netGate.substr(0, netGate.find_last_of(".") + 1);
        for (int i  = 2; i < 255; i++)
        {
            ip = s + std::to_string(i);
            if (ip != netGate && itNetgate->second.find(ip) == itNetgate->second.end())
            {
                break;
            }
        }
    }
    else{
        if (netGate.empty() && !specialIP.empty())
        {
            netGate = specialIP;
        }
        if (!netGate.empty())
        {
			netGate = netGate.substr(0, netGate.find_last_of(".") + 1) + "1";
            for (int i = 2; i < 255; i++)
            {
				ip = netGate.substr(0, netGate.find_last_of(".") + 1) + std::to_string(i);
                if (ip != specialIP)
                {
                    break;
                }
            }
        }
    }

	if (adhandle)
		pcap_close(adhandle);

    return !ip.empty();
	
}
bool WindowUtils::setIPByDHCP(string& ip, string& mask, string& netGate){
	string sName = GetNetCardName(WindowUtils::getLocalUuid());
    
	if (!WindowUtils::setNetDhcp(sName))
		return false;
    std::vector<string> ips;
    //int maxPingTime = 1000 * 3;
    ::Sleep(1000*5);
	WindowUtils::GetIPfromLocalNIC(ips);
	/*for (WindowUtils::GetIPfromLocalNIC(ips); ips.size() == 0 && maxPingTime > 0; WindowUtils::GetIPfromLocalNIC(ips)){
        ::Sleep(1000);
        maxPingTime -= 1000;
    }*/
    
    bool r = false;

	if (ips.size() == 1)
	{
		string stmp = ips[0].substr(0, ips[0].find_last_of("."));
		stmp = stmp.substr(0, stmp.find_last_of("."));
		if (stmp.compare("169.254") == 0)
			return r;
	}
    if (ips.size() > 0)
    {
        _IP_ADAPTER_INFO* pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(sizeof(PIP_ADAPTER_INFO));
		_IP_ADAPTER_INFO* pfirstAdapterInfo = pIpAdapterInfo;
        ULONG uOutBufLen = sizeof(PIP_ADAPTER_INFO);//net card info buffer size
        //first run GetAdapterInfo get ulOutBufLen size
        int errorNo = GetAdaptersInfo(pIpAdapterInfo, &uOutBufLen);
        if (errorNo == ERROR_BUFFER_OVERFLOW)
        {
            //get buffer
            free(pIpAdapterInfo);
            pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(uOutBufLen);
			pfirstAdapterInfo = pIpAdapterInfo;
            errorNo = (GetAdaptersInfo(pIpAdapterInfo, &uOutBufLen));
        }

        if (NO_ERROR == errorNo)
        {
            while (pIpAdapterInfo && (!r))
            {
                for (IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
                    pIpAddrString != NULL && (!r); pIpAddrString = pIpAddrString->Next){
					
                    if (*ips.begin() == pIpAddrString->IpAddress.String)
                    {						
                        ip = *ips.begin();
                        mask = pIpAddrString->IpMask.String;
                        netGate = pIpAdapterInfo->GatewayList.IpAddress.String;
                        r = true;
                    }


                }
                pIpAdapterInfo = pIpAdapterInfo->Next;
            }
        }
		free(pfirstAdapterInfo);
    }

    return r;
}



const string& WindowUtils::getLoacalNetName()
{
	static string localNetName;
	if (localNetName.empty())
	{
		string sUuid;
		vector<string> IpList;
		u_char mac[6] = { 0 };
		GetLocalNetCar(localNetName, sUuid, IpList, mac);
	}

	return localNetName;
}

bool WindowUtils::isOnLine(){
    
    string localNetName = WindowUtils::getLoacalNetName();
    
    if (localNetName.empty())
    {
        return false;
    }  

    DWORD dwSize = sizeof (MIB_IFTABLE);
    DWORD dwRetVal = 0;

    unsigned int i;

    /* variables used for GetIfTable and GetIfEntry */
    char *pIfTable = new char[dwSize];   

    if (GetIfTable((MIB_IFTABLE *)pIfTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
        delete[] pIfTable;
        pIfTable = new char[dwSize];
    }
   
    bool r = false;
    if ((dwRetVal = GetIfTable((MIB_IFTABLE *)pIfTable, &dwSize, 0)) == NO_ERROR) {
        MIB_IFTABLE *pTable = (MIB_IFTABLE *)pIfTable;
        if (pTable->dwNumEntries > 0) {
            MIB_IFROW IfRow;
            for (i = 0; i < pTable->dwNumEntries; i++) {
                IfRow.dwIndex = pTable->table[i].dwIndex;
                if ((dwRetVal = GetIfEntry(&IfRow)) == NO_ERROR) {
					
					std::string sName((char *)IfRow.bDescr);
					if (IfRow.dwType != MIB_IF_TYPE_ETHERNET || sName.compare(localNetName) != 0)
                    {
                        continue;
                    }
                    
                    switch (IfRow.dwOperStatus) {
                    case IF_OPER_STATUS_NON_OPERATIONAL:                        
						poco_information(logger_handle, "Non Operational");
                        break;
                    case IF_OPER_STATUS_UNREACHABLE:
						poco_information(logger_handle, "Unreasonable");
                        break;
                    case IF_OPER_STATUS_DISCONNECTED:
						poco_information(logger_handle, "Disconnected");
                        break;
                    case IF_OPER_STATUS_CONNECTING:						
						poco_information(logger_handle, "Connecting");
                        r = true;
                        break;
                    case IF_OPER_STATUS_CONNECTED:                        
						poco_information(logger_handle, "Connected");
                        r = true;
                        break;
                    case IF_OPER_STATUS_OPERATIONAL:
						poco_information(logger_handle, "Operational");
                        r = true;
                        break;
                    default:                        
						poco_information_f1(logger_handle, "Unknown status: %d", IfRow.dwAdminStatus);
                        break;
                    }
                }

                else {                   
					poco_error_f1(logger_handle, "GetIfEntry failed for index with error: %d", dwRetVal);
                }
            }
        }
        else {			
			poco_error_f1(logger_handle, "GetIfTable failed with error: %d", dwRetVal);
        }

    }
    delete[] pIfTable;
    return r;
}

const string& WindowUtils::getLocalUuid()
{
	static string uuid;
	if (uuid.empty())
	{
		string sNetName;
		vector<string> IpList;
		u_char mac[6] = { 0 };
		GetLocalNetCar(sNetName, uuid, IpList, mac);
	}
	return uuid;
}

void WindowUtils::getMacByArpTable(vector<string>Ips, vector<IPMAC>& IpMacs)
{
	MIB_IPNETTABLE *ipNetTable = NULL;
	ULONG size = 0;
	ULONG result = 0;
	int i, j;
	//in_addr inaddr;
	
	result = GetIpNetTable(ipNetTable, &size, TRUE);
	if (result == ERROR_INSUFFICIENT_BUFFER)
	{
		ipNetTable = (MIB_IPNETTABLE *)malloc(size);
		result = GetIpNetTable(ipNetTable, &size, TRUE);
		if (result == ERROR_SUCCESS) {
			for (i = 0; i < ipNetTable->dwNumEntries; i++)
			{							
				for (j = 0; j < Ips.size(); j++)
				{										
					if (inet_addr(Ips[j].c_str()) == ipNetTable->table[i].dwAddr) 					
					{						
						IPMAC ipmac = { 0 };
						ipmac.ip = ipNetTable->table[i].dwAddr;						
						if (ipNetTable->table[i].bPhysAddr[0] == 0
							&& ipNetTable->table[i].bPhysAddr[1] == 0
							&& ipNetTable->table[i].bPhysAddr[2] == 0
							&& ipNetTable->table[i].bPhysAddr[3] == 0)
						{
							
						}
						else
						{
							for (j = 0; j < 6; j++)
							{
								ipmac.mac[j] = ipNetTable->table[i].bPhysAddr[j];
							}
							IpMacs.push_back(ipmac);
						}
						
						break;
					}
				}
			}
			
		}
	}

}

void WindowUtils::getMacByArpTable(vector<string>Ips, set<string>& findIps)
{
	MIB_IPNETTABLE *ipNetTable = NULL;
	ULONG size = 0;
	ULONG result = 0;
	int i, j;
	//in_addr inaddr;

	result = GetIpNetTable(ipNetTable, &size, TRUE);
	if (result == ERROR_INSUFFICIENT_BUFFER)
	{
		ipNetTable = (MIB_IPNETTABLE *)malloc(size);
		result = GetIpNetTable(ipNetTable, &size, TRUE);
		if (result == ERROR_SUCCESS) {
			for (i = 0; i < ipNetTable->dwNumEntries; i++)
			{
				for (j = 0; j < Ips.size(); j++)
				{
					if (inet_addr(Ips[j].c_str()) == ipNetTable->table[i].dwAddr)
					{			
						if (ipNetTable->table[i].bPhysAddr[0] == 0
							&& ipNetTable->table[i].bPhysAddr[1] == 0
							&& ipNetTable->table[i].bPhysAddr[2] == 0
							&& ipNetTable->table[i].bPhysAddr[3] == 0)
						{

						}
						else
						{							
							findIps.insert(Ips[j]);
						}						
						break;
					}
				}
			}

		}
	}

}

void WindowUtils::getLocalIPsMac(std::vector<string> &IPs, u_char *mac)
{
	IPs.clear();
	string sDesc;
	string uuid;	
	GetLocalNetCar(sDesc, uuid, IPs, mac);
}