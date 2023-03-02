#pragma once
#include "Common.h"

// �����ڴ��
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		// ���freeList���ڴ棬���Ȱѻ��������ڴ�ȥ����
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			// ʣ���ڴ治��һ�������С��ʱ�����¿�һ���µĴ���ڴ�
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
			// ���С��һ��ָ��Ĵ�С���ͷ���һ��ָ��Ĵ�С
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}
		
		// ��λnew����ʾ����T�Ĺ��캯����ʼ��
		// new (place_address) type
		new(obj)T;

		return obj;
	}

	void Delete(T* obj)
	{
		// ��ʾ�������������������
		obj->~T(); 
		// ͷ��
		*(void**)obj = _freeList; // ȡһ��ָ��Ĵ�С
		_freeList = obj;
	}

private:
	char* _memory = nullptr;    // ָ�����ڴ��ָ��
	size_t _remainBytes = 0;    // ʣ���ֽڴ�С
	void* _freeList = nullptr;  // �����������е����������ָ��
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
//	// �����ͷŵ��ִ�
//	const size_t Rounds = 3;
//	// ÿ�������ͷŶ��ٴ�
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