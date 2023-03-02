#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 慢开始反馈算法
	// 最开始一次不会向central cache要太多，可能用不完
	// batchNum会不断增长，NumMoveSize()就是对应的上限
	// size越大，一次向central cache要的batchNum越小
	// size越小，一次向central cache要的batchNum越大
	size_t batchNum = min(_freeLists[index].MaxSize(),SizeClass::NumMoveSize(size));
	if (batchNum == _freeLists[index].MaxSize())
		++_freeLists[index].MaxSize();				// 可以调节
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
	size_t alignSize = SizeClass::RoundUp(size); // 将大小对齐到对应数值
	size_t index = SizeClass::Index(alignSize);	 // 计算映射的桶的位置
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
	// 找到对应桶的自由链表，并插入进去
	_freeLists[index].Push(ptr); 

	// 当链表的长度大于一次批量申请的内存时就开始还一段list给central cache
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