#include "ObjectPool.h"
#include "ConcurrentAlloc.h"
//
//void Alloc1()
//{
//	for (size_t i = 0; i < 5; ++i)
//	{
//		void* ptr = ConcurrentAlloc(7);
//	}
//}
//
//void Alloc2()
//{
//	for (size_t i = 0; i < 5; ++i)
//	{
//		void* ptr = ConcurrentAlloc(7);
//	}
//}
//
//void TLSTest()
//{
//	std::thread t1(Alloc1);
//	std::thread t2(Alloc2);
//
//	t1.join();
//	t2.join();
//}
//
//void TestConcurrentAlloc()
//{
//	void* p1 = ConcurrentAlloc(6);
//	void* p2 = ConcurrentAlloc(8);
//	void* p3 = ConcurrentAlloc(1);
//	void* p4 = ConcurrentAlloc(7);
//	void* p5 = ConcurrentAlloc(8);
//	cout << p1 << endl;
//	cout << p2 << endl;
//	cout << p3 << endl;
//	cout << p4 << endl;
//	cout << p5 << endl;
//	ConcurrentFree(p1);
//	ConcurrentFree(p2);
//	ConcurrentFree(p3);
//	ConcurrentFree(p4);
//	ConcurrentFree(p5);
//}
//
//int main()
//{
//	TestConcurrentAlloc();
//}
