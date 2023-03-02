#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	// 查看当前的spanlist是否还有未分配对象的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
			return it;
		it = it->_next;
	}

	// 先把central cache的桶锁解掉，如果其他线程释放内存对象回来，不会阻塞
	list._mtx.unlock();
	// 说明没有空闲的span，从page cache申请
	PageCache::GetInstace()->_pageMtx.lock();
	Span* span = PageCache::GetInstace()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstace()->_pageMtx.unlock();

	// 对获取的span进行切分，不需要加锁，因为其他线程访问不到这个span

	// 计算span的大块内存的起始地址和大块内存的大小
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	// 把大块内存切成自由链表链接起来
	// 切一块做头
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	// 尾插
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;

	//// 测试验证+条件断点
	//void* cur = span->_freeList;
	//int j = 0;
	//while (cur)
	//{
	//	cur = NextObj(cur);
	//	++j;
	//}
	//if (bytes / size != j)
	//	int x = 0;

	// 切好span以后，需要把span的锁恢复
	list._mtx.lock();
	list.PushFront(span);

	return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	start = span->_freeList;
	end = start;
	size_t i = 0;
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
	}
	size_t actualNum = i + 1;
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;

	//// 测试验证+条件断点
	//void* cur = start;
	//int j = 0;
	//while (cur)
	//{
	//	cur = NextObj(cur);
	//	++j;
	//}
	//if (actualNum != j)
	//	int x = 0;


	_spanLists[index]._mtx.unlock();
	return actualNum;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		Span* span = PageCache::GetInstace()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		
		// 说明切分出去的小块内存都回来了
		// span回收给page cache，page cache可以尝试做前后页的合并
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// 暂时需要解桶锁
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstace()->_pageMtx.lock();
			PageCache::GetInstace()->ReleaseSpanToPageCache(span);
			PageCache::GetInstace()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();

		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();
}
