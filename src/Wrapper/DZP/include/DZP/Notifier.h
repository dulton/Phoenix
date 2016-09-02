#pragma once

#include "DZPLite.h"
#include "DVR/DVRSession.h"
#include "DZP/Utility.h"
#include <Poco/BasicEvent.h>

namespace DVR {
namespace DZPLite {

class DZPLite_API Notifier
{
public:
	typedef unsigned char EnabledEventType;

	typedef Poco::BasicEvent<void> Event;

	Event pos;

	static const EnabledEventType DVRLITE_NOTIFY_DOWNLOAD = 1;
	static const EnabledEventType DVRLITE_NOTIFY_PLAYBACK = 2;

	Notifier(const DVRSession& session, EnabledEventType enabled);
	~Notifier();

	static void __stdcall ProcessCallbackFn(long lPlayHandle, long lTotalSize, long lDownLoadSize, long dwUser);

	bool operator == (const Notifier& other) const;
	Poco::Int64 getPos() const;
	
	bool enableDownload();
	bool disableDownload();
	bool downloadEnabled()const;

	bool enablePlayback();
	bool disablePlayback();
	bool playbackEnabled()const;

	bool disableAll();
	
	static void dvrDownloadCallbackFn(void* pVal);
	static void dvrPlaybackCallbackFn(void* pVal);
private:
	Notifier();
	Notifier(const Notifier&);
	Notifier& operator=(const Notifier&);

	const DVRSession&  _session;
	Poco::Int64        _pos;
	EnabledEventType   _enabledEvents;
	Poco::Mutex        _mutex;
};

inline bool Notifier::operator == (const Notifier& other) const
{
	return _pos == other._pos &&
		Utility::dvrHandle(_session) == Utility::dvrHandle(other._session);
}

inline Poco::Int64 Notifier::getPos() const
{
	return _pos;
}

}}