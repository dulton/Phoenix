#include "stdafx.h"
#include "SearchFileWorker.h"

FastMutex SearchFileWorker::m_mutex;

SearchFileWorker::SearchFileWorker(Device* pDevice, const time_range& range, const std::vector<size_t> channelList, ReciveUIQunue& queue) :
m_queue(queue)
{
	m_pDevice = pDevice;
	m_range = range;
	m_channelList = channelList;
	m_bCancel = false;
}



SearchFileWorker::~SearchFileWorker()
{
}

void SearchFileWorker::run()
{
	Poco::Random rnd;
	int nPos = 0;

	std::vector<time_range> trInfor = CCommonUtrl::getInstance().MakeTimeRangeList(m_range);

	int nDay = trInfor.size();
	int nChannelList = m_channelList.size();

	NotificationQueue& queue = NotificationQueue::defaultQueue();
	

	m_pDevice->ClearLocalRecordFiles();

	for (auto channel : m_channelList)
	{
		for (auto day : trInfor)
		{
			try
			{
				m_pDevice->Search(channel, day);
			}
			catch (SearchFileException &SearchFileExc)
			{
				std::cout << SearchFileExc.displayText() << std::endl;
				queue.enqueueNotification(new SearchFileNotification(Notification_Type_Search_File_Failure, SEARCHFILE_DEFAULT));
			}

			nPos++;
			queue.enqueueNotification(new SearchFileNotification(Notification_Type_Search_File_TotalSize, nDay*nChannelList));
			queue.enqueueNotification(new SearchFileNotification(Notification_Type_Search_File_Process, nPos));

			
			if (!m_queue.empty())
			{
				Notification::Ptr pNf(m_queue.waitDequeueNotification());
				if (pNf)
				{
					ReciveUINotification::Ptr pReciveDataNf = pNf.cast<ReciveUINotification>();
					if (pReciveDataNf)
					{
						{
							FastMutex::ScopedLock lock(m_mutex);
							m_bCancel = pReciveDataNf->GetData();
						}
					}
				}

				if (m_bCancel)
				{
					return;
				}

			}
			
		}
	}

	queue.enqueueNotification(new SearchFileNotification(Notification_Type_Search_File_Finish, SEARCHFILE_DEFAULT));
}


