/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_JOBS_H
#define ENGINE_SHARED_JOBS_H

#include <atomic>
#include <memory>

class IJob;
class CJobPool;

class IJob
{
	friend CJobPool;

private:
	std::shared_ptr<IJob> m_pNext;

	std::atomic<int> m_Status;
	virtual void Run() = 0;

public:
	IJob();
	IJob(const IJob &Other);
	IJob &operator=(const IJob &Other);
	virtual ~IJob();
	int Status();

	enum
	{
		STATE_PENDING=0,
		STATE_RUNNING,
		STATE_DONE
	};
};

class CJobPool
{
	enum
	{
		MAX_THREADS=32
	};
	int m_NumThreads;
	void *m_apThreads[MAX_THREADS];
	volatile bool m_Shutdown;

	LOCK m_Lock;
	SEMAPHORE m_Semaphore;
	std::shared_ptr<IJob> m_pFirstJob;
	std::shared_ptr<IJob> m_pLastJob;

	static void WorkerThread(void *pUser);

public:
	CJobPool();
	~CJobPool();

	int Init(int NumThreads);
	int Add(std::shared_ptr<IJob> pJob);
};
#endif
