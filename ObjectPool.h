#pragma once
#include "Common.h"

// 定长内存池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		// 如果freeList有内存，优先把还回来的内存去申请
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			// 剩余内存不够一个对象大小的时候，重新开一个新的大块内存
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024; // 128k
				_memory = (char*)SystemAlloc(_remainBytes >> 13);
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			// 如果小于一个指针的大小，就分配一个指针的大小
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}
		
		// 定位new，显示调用T的构造函数初始化
		// new (place_address) type
		new(obj)T;

		return obj;
	}

	void Delete(T* obj)
	{
		// 显示调用析构函数清理对象
		obj->~T(); 
		// 头插
		*(void**)obj = _freeList; // 取一个指针的大小
		_freeList = obj;
	}

private:
	char* _memory = nullptr;    // 指向大块内存的指针
	size_t _remainBytes = 0;    // 剩余字节大小
	void* _freeList = nullptr;  // 还回来过程中的自由链表的指针
};
//
//struct TreeNode
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};
//
//static void TestObjectPool()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 3;
//	// 每轮申请释放多少次
//	const size_t N = 100000;
//	size_t begin1 = clock();
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new TreeNode);
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i];
//		}
//		v1.clear();
//	}
//	size_t end1 = clock();
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < 100000; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//	cout << "new cost time:" << end1 - begin1 << endl;
//	cout << "object pool cost time:" << end2 - begin2 << endl;
//}