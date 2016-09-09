#include "DVR/DVRDevice.h"
#include "DVR/DVRSession.h"

namespace DVR{
	DVRDevice::DVRDevice(DVRSession& session) :
		_MainSession(session),
		_name(session.connectorName()),
		_ipAndport(session.connectionString())
	{
		InitChannel();
	}


	DVRDevice::~DVRDevice()
	{
	}

	void DVRDevice::InitChannel()
	{
		for (int i = 0; i < NumberOfChannel; i++)
		{
			_channels.push_back(false);
		}
	}

	const std::string DVRDevice::name() const
	{
		poco_bugcheck_msg("generate the unique string");
		//must be generate the "unique string"
		return _MainSession.connectorName() + "unique string";
	}

	const std::string DVRDevice::address() const
	{
		return _MainSession.connectionString();
	}

	bool DVRDevice::isLoggedIn()const
	{
		return _MainSession.isLoggedIn();
	}

	DVRSession& DVRDevice::session()
	{
		return _MainSession;
	}
}