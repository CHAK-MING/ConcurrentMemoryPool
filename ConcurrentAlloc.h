#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)
	{
		size_t alginSzie = SizeClass::RoundUp(size);
		size_t kpage = alginSzie >> PAGE_SHIFT;
		PageCache::GetInstace()->_pageMtx.lock();
		Span* span = PageCache::GetInstace()->NewSpan(kpage);
		span->_objSize = size;
		PageCache::GetInstace()->_pageMtx.unlock();
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		// 通过tls无锁的获取自己的ThreadCache对象
		if (tls_threadcache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;
			tls_threadcache = tcPool.New();
		}

		//cout << std::this_thread::get_id() << " : " << tls_threadcache << endl;

		return tls_threadcache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstace()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if (size > MAX_BYTES)
	{
		PageCache::GetInstace()->_pageMtx.lock();
		PageCache::GetInstace()->ReleaseSpanToPageCache(span);
		PageCache::GetInstace()->_pageMtx.unlock();
	}
	else
	{
		assert(tls_threadcache);
		tls_threadcache->Deallocate(ptr, size);
	}
}