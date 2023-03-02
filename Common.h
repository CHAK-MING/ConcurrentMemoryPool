#pragma once
#include <iostream>
#include <vector>
#include <ctime>
#include <cassert>
#include <thread>
#include <mutex>
#include <algorithm>
#include <unordered_map>
using std::cout;
using std::endl;

#ifdef _WIN32
#include <Windows.h>
#endif

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELISTS = 208; // 哈希桶数量
static const size_t NPAGES = 129;    // page cache 管理span list哈希表大小
static const size_t PAGE_SHIFT = 13; // 页大小转换偏移, 即一页定义为2^13,也就是8KB


// 64位有_WIN64和_WIN32,32位只有_WIN32
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#endif


inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}

static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

// 管理切分好的小对象的自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);
		// 头插
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}

	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		
		//// 测试验证+条件断点
		//void* cur = start;
		//int i = 0;
		//while (cur)
		//{
		//	cur = NextObj(cur);
		//	++i;
		//}
		//if (n != i)
		//	int x = 0;

		_size += n;
	}

	void* Pop()
	{
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}

	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);
		start = _freeList;
		end = start;
		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
};

// 计算对象大小的对齐映射规则
class SizeClass
{
public:
	// 整体控制在最多10%左右的内碎片浪费
	// [1,128]					8byte对齐			freelist[0,16)
	// [128+1,1024]				16byte对齐			freelist[16,72)
	// [1024+1,8*1024]			128byte对齐			freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐		freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte对齐		freelist[184,208)
	
	static inline size_t _RoundUp(size_t bytes, size_t align)
	{
		return ((bytes + align - 1) & ~(align - 1));
	}
	
	static inline size_t RoundUp(size_t bytes)
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);
		}
		else if(bytes <= 1024)
		{
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);
		}
		else if (bytes <= 256 * 1024)
		{
			return _RoundUp(bytes, 8 * 1024);
		}
		else
		{
			return _RoundUp(bytes, 1 << PAGE_SHIFT); // 以页为单位对齐
		}
		return -1;
	}

	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else {
			assert(false);
		}
		return -1;
	}
	
	// 一次thread cache从中心缓存获取多少个size
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	// 计算一次向系统获取几个页
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}

};

// 管理多个连续页的大块内存跨度结构
struct Span
{
	PAGE_ID _pageId = 0;  // 大块内存起始页的页号
	size_t _n = 0;		  // 页的数量

	Span* _prev = nullptr; // 双向链表结构
	Span* _next = nullptr;

	size_t _objSize = 0; // 切好的小对象的大小
	size_t _useCount = 0; // 切好小块内存，被分配给thread cache的计数
	void* _freeList = nullptr;      // 切好小块内存的自由链表

	bool _isUse = false;   // 是否在被使用
};

// 带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		// prev newspan pos
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);
		
		pos->_prev->_next = pos->_next;
		pos->_next->_prev = pos->_prev;
	}

	
private:
	Span* _head = nullptr;
public:
	std::mutex _mtx; // 桶锁
};