#pragma once
#include "Common.h"

class ThreadCache
{
public:
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);

	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};

// TLS �ֲ߳̾��洢
// ÿ���̶߳�������һ����ȫ�ֱ������߳�֮�以��Ӱ��
static __declspec(thread) ThreadCache* tls_threadcache = nullptr;