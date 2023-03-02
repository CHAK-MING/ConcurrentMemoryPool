#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	// �鿴��ǰ��spanlist�Ƿ���δ��������span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
			return it;
		it = it->_next;
	}

	// �Ȱ�central cache��Ͱ���������������߳��ͷ��ڴ�����������������
	list._mtx.unlock();
	// ˵��û�п��е�span����page cache����
	PageCache::GetInstace()->_pageMtx.lock();
	Span* span = PageCache::GetInstace()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstace()->_pageMtx.unlock();

	// �Ի�ȡ��span�����з֣�����Ҫ��������Ϊ�����̷߳��ʲ������span

	// ����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	// �Ѵ���ڴ��г�����������������
	// ��һ����ͷ
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	// β��
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;

	//// ������֤+�����ϵ�
	//void* cur = span->_freeList;
	//int j = 0;
	//while (cur)
	//{
	//	cur = NextObj(cur);
	//	++j;
	//}
	//if (bytes / size != j)
	//	int x = 0;

	// �к�span�Ժ���Ҫ��span�����ָ�
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

	//// ������֤+�����ϵ�
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
		
		// ˵���зֳ�ȥ��С���ڴ涼������
		// span���ո�page cache��page cache���Գ�����ǰ��ҳ�ĺϲ�
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// ��ʱ��Ҫ��Ͱ��
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
