#ifndef THREADING_H
#define THREADING_H

#include "../g_defines.h"
#include "mutex.h"
#include "semaphores.h"
#include <atomic>

#if defined(__posix__)
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef pthread_t thread_t;

#else
#define NOMINMAX
#include "../wincludes.h"
#include <Psapi.h>
#include "stdint.h"

typedef unsigned long thread_t;

#endif

#define IS_RUNNING (1 << 0)
#define IS_FINISHED (1 << 1)

typedef void(*JobFn)(void*);
typedef void*(__stdcall*threadFn)(void*);

struct Job
{
	JobFn function;
	void* args;
	bool ref;
	std::atomic_char flags;

	void WaitForFinish()
	{
		while (!(flags & IS_FINISHED));
	}
};

template <typename T>
struct LList
{
	struct LEntry
	{
		T* entry;
		LEntry* prev;
		LEntry* next;
	};

	Mutex lock;
	struct LEntry* front;
	struct LEntry* back;
	Semaphore sem;

	T* Enqueue(T* data) {
		struct LEntry* entry = new LEntry({ data, nullptr, back });
		lock.lock();
		if (back)
			back->prev = entry;
		if (!front)
			front = entry;
		entry->next = back;
		back = entry;
		lock.unlock();
		sem.Post();
		return data;
	}

	T* PopFront(Mutex* lck = nullptr) {
		sem.Wait();
		lock.lock();
		if (lck)
			lck->lock();
		if (!front) {
			lock.unlock();
			return nullptr;
		}
		struct LEntry* entry = front;
		front = entry->prev;
		if (front)
			front->next = nullptr;
		else
			back = nullptr;
		T* ret = entry->entry;
		delete entry;
		lock.unlock();
		return ret;
	}
};

struct JobThread
{
	std::atomic_bool shouldQuit;
	std::atomic_bool isRunning;
	std::atomic_bool queueEmpty;
	Mutex* jLock;
	LList<struct Job>* jobs;
	int id;
	void* handle;
};

namespace Threading
{
	extern unsigned int numThreads;
	struct Job* _QueueJob(JobFn function, void* data, bool ref = false);
	void InitThreads();
	void EndThreads();
	void FinishQueue();
	JobThread* BindThread(LList<struct Job>* jobsQueue);
	void UnbindThread(LList<struct Job>* jobsQueue);
	thread_t StartThread(threadFn start, void* param);
	thread_t StartThread(threadFn start, void* param, thread_t* thread);

	template<typename N, typename T>
	Job* QueueJob(N function, T data) {
		void* d = malloc(sizeof(T));
		memcpy(d, (void*)&data, sizeof(T));
		return _QueueJob((JobFn)function, d, false);
	}

	template<typename N, typename T>
	Job* QueueJobRef(N function, T* data) {
		return _QueueJob((JobFn)function, (void*)data, true);
	}
}

#endif