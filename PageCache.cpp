#include "PageCache.h"

PageCache PageCache::_sInst;

Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
	//std::unique_lock<std::mutex> lock(_pageMtx); // �Զ���������
	//auto ret = _idSpanMap.find(id);
	//if (ret != _idSpanMap.end())
	//{
	//	return ret->second;
	//}
	//else
	//{
	//	assert(false);
	//	return nullptr;
	//}
	auto ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	if (k > NPAGES - 1)
	{
		// �Ӷ�����
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();
		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;
		//_idSpanMap[span->_pageId] = span;
		_idSpanMap.set(span->_pageId, span);
		return span;
	}
	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();

		// ����id��span��ӳ��
		for (PAGE_ID i = 0; i < kSpan->_n; ++i)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}
	// �������Ͱ��û��span���������У����Խ����з�
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			// �з�
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = _spanPool.New();
			// ��nSpan��ͷ����һ��k����
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;

			_spanLists[nSpan->_n].PushFront(nSpan);

			// �洢nSpan����βҳ�Ÿ�nSpan��ӳ��,����page cache�����ڴ�ʱ�ĺϲ�����
			/*_idSpanMap[nSpan->_pageId] = nSpan;
			_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;*/
			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);


			// ����id��span��ӳ��
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);

			}

			return kSpan;
		}
	}
	// ˵��128ҳû��span����Ҫ�Ӷ�������
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// ����128ҳֱ�ӻ�������
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		_spanPool.Delete(span);
		return;
	}
	// ��spanǰ���ҳ���Խ��кϲ��������ڴ���Ƭ������
	while (true)
	{
		PAGE_ID prevId = span->_pageId - 1;
		//auto ret = _idSpanMap.find(prevId);
		//// ǰ���ҳ��û�У����ϲ�
		//if (ret == _idSpanMap.end())
		//	break;

		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr)
			break;

		// ǰ������ҳ��span��ʹ��
		Span* prevSpan = ret;
		if (prevSpan->_isUse == true)
			break;

		// �ϲ�������128��span��û�취�������ϲ�
		if (prevSpan->_n + span->_n > NPAGES - 1)
			break;

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		_spanPool.Delete(prevSpan);
	}

	// ���ϲ�
	while (true)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		/*auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end())
			break;*/

		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
			break;
		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
			break;

		if (nextSpan->_n + span->_n > NPAGES - 1)
			break;

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		_spanPool.Delete(nextSpan);
	}

	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;
	/*_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;*/
	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}
