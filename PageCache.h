#pragma once

#include "Common.h"
#include "ObjectPool.h" // 定长内存池
#include "PageMap.h"

// 单例模式
class PageCache
{
public:
	static PageCache* GetInstace()
	{
		return &_sInst;
	}

	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	// 获取k页的span
	Span* NewSpan(size_t k);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);
public:
	std::mutex _pageMtx;
private:
	SpanList _spanLists[NPAGES];
	ObjectPool<Span> _spanPool;

	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;


	PageCache()
	{}

	PageCache(const PageCache&) = delete;
	
	static PageCache _sInst;
};