#pragma once
#include "Common.h"

// ����ģʽ
class CentralCache
{
public:
	static CentralCache* GetInstace()
	{
		return &_sInst;
	}

	// ��SpanList����page cache��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t size);

	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t size);

private:
	SpanList _spanLists[NFREELISTS]; // ��ϣͰ
private:
	CentralCache()
	{}
	CentralCache(const CentralCache&) = delete;
	static CentralCache _sInst;
};