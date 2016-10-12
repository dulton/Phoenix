#pragma once
#include "TDWYLite.h"

#include <Poco/DateTime.h>
#include <string>
#include "DVR/DVRSession.h"
#include <Poco/Net/SocketAddress.h>
#include <Poco/SharedLibrary.h>
#include <map>
#include "NetSdkClient.h"



namespace DVR {
namespace TDWYLite {

	extern "C"
	{
		typedef NVS_FILE_DATA        Record;
		typedef NVS_FILE_QUERY		Condition;		
		typedef NVS_FILE_DATA		 Play;
		//typedef struct tagNETDEVPlayBackCondition PlayCondition;
	}


class TDWYLite_API Utility
{
public:
	~Utility(); 
	typedef int HANDLE;
	typedef unsigned int FileHandle;
	typedef LPVOID PlayHandle;
	static const int success;

	static Utility::HANDLE dvrHandle(const DVRSession& session);	
	static int Utility::lastError();	
	static void throwException(long rc, const std::string& addErrMsg = std::string());

	static void Init();
	static void Cleanup();
	static void SetNotifyFunction(PVOID pNotify);

	static Utility::HANDLE login(const Poco::Net::SocketAddress& _addr, const std::string& user, const std::string& password);
	static bool logout(Utility::HANDLE handle);	
	static bool getChannel(Utility::HANDLE handle, int *count);

	static Utility::FileHandle readStream(Utility::HANDLE handle, Play& data, const std::string& newname);	
	static Utility::FileHandle readStreamByTime(Utility::HANDLE handle, Play& data, const std::string& newname);
	static int readStreamPos(Utility::FileHandle handle);
	static bool closeStream(Utility::FileHandle handle);
	

	static Utility::PlayHandle playStream(Utility::HANDLE handle, Play& data);
	static Utility::PlayHandle playStreamByTime(Utility::HANDLE handle, Play& data);
	static bool playPos(Utility::PlayHandle handle, INT64 *iTime);
	static bool stopStream(Utility::PlayHandle handle);

	static bool seek(Utility::PlayHandle handle, INT64 pos);
	static bool pause(Utility::PlayHandle handle);
	static bool resume(Utility::PlayHandle handle);

	static bool findStream(Utility::HANDLE handle, const Condition& cond);
	static bool getFindCount(Utility::HANDLE handle, int *count);
	static bool getFindResult(Utility::HANDLE handle, int iIndex, Record& data);

	//static void readDeviceInfo(DeviceInfo& info);	

	typedef void(*EventCallbackType)(void* pVal);

	template <typename T, typename CBT>
	static bool registerUpdateHandler(Utility::HANDLE Handle, CBT callbackFn, T* pParam)
		/// Registers the callback for (1)(insert, delete, update), (2)(commit) or 
		/// or (3)(rollback) events. Only one function per group can be registered
		/// at a time. Registration is not thread-safe. Storage pointed to by pParam
		/// must remain valid as long as registration is active. Registering with
		/// callbackFn set to zero disables notifications.
		/// 
		/// See http://www.sqlite.org/c3ref/update_hook.html and 
		/// http://www.sqlite.org/c3ref/commit_hook.html for details.
	{
		typedef std::pair<CBT, T*> CBPair;
		typedef std::multimap<Utility::HANDLE, CBPair> CBMap;
		typedef typename CBMap::iterator CBMapIt;
		typedef std::pair<CBMapIt, CBMapIt> CBMapItPair;

		static CBMap retMap;

		if (retMap.find(Handle) == retMap.end())//insert callbackFn when key is noexist.
		{
			retMap.insert(std::make_pair(Handle, CBPair(callbackFn, pParam)));
			return true;
		}
		else //update callbackFn when key is exist.
		{
			CBMapItPair retMapRange = retMap.equal_range(Handle);
			for (CBMapIt it = retMapRange.first; it != retMapRange.second; ++it)
			{
				poco_assert(it->second.first != 0);
				if ((callbackFn == 0) && (*pParam == *it->second.second))
				{
					retMap.erase(it);
					return true;
				}

				if ((callbackFn == it->second.first) && (*pParam == *it->second.second))
				{
					it->second.second = pParam;
					return true;
				}
			}
		}

		return false;
	}

	template <typename T, typename CBT>
	static bool registerUpdateHandler(const DVRSession& session, CBT callbackFn, T* pParam)
		/// Registers the callback by calling registerUpdateHandler(sqlite3*, CBT, T*).
	{
		return registerUpdateHandler(dvrHandle(session), callbackFn, pParam);
	}

protected:
	//static void callback();
private:
	Utility();
	Utility(const Utility&);
	Utility& operator = (const Utility&);

	static Poco::Mutex _mutex;
	static Poco::SharedLibrary _library;			
};

//inline std::string Utility::lastError(Utility::HANDLE handle)
//{
//	long errCode = Utility::lastError();
//	return std::string("errCode");
//}

}}