#pragma once
#include "Common.h"

class ThreadCache
{
public:
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);

	// 释放对象时，链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};

// TLS 线程局部存储
// 每个线程都独立有一个的全局变量，线程之间互不影响
static __declspec(thread) ThreadCache* tls_threadcache = nullptr;