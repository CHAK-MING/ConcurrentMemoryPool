#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// ����ʼ�����㷨
	// �ʼһ�β�����central cacheҪ̫�࣬�����ò���
	// batchNum�᲻��������NumMoveSize()���Ƕ�Ӧ������
	// sizeԽ��һ����central cacheҪ��batchNumԽС
	// sizeԽС��һ����central cacheҪ��batchNumԽ��
	size_t batchNum = min(_freeLists[index].MaxSize(),SizeClass::NumMoveSize(size));
	if (batchNum == _freeLists[index].MaxSize())
		++_freeLists[index].MaxSize();				// ���Ե���
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstace()->FetchRangeObj(start,end,batchNum,size);
	assert(actualNum > 0);
	if (actualNum == 0)
	{
		assert(start == end);
		return start;
	}
	_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
	return start;
}



void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size); // ����С���뵽��Ӧ��ֵ
	size_t index = SizeClass::Index(alignSize);	 // ����ӳ���Ͱ��λ��
	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();
	}
	else
	{
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	size_t index = SizeClass::Index(size);
	// �ҵ���ӦͰ�����������������ȥ
	_freeLists[index].Push(ptr); 

	// ������ĳ��ȴ���һ������������ڴ�ʱ�Ϳ�ʼ��һ��list��central cache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());
	CentralCache::GetInstace()->ReleaseListToSpans(start, size);
}