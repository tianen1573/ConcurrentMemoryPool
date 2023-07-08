#define _CRT_SECURE_NO_WARNINGS 1


#include"ConcurrentAlloc.hpp"

using std::cout;
using std::endl;

void Alloc1()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(7);
	}
}
void Alloc2()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(7);
	}
}
void TLSTest()
{
	std::thread t1(Alloc1);
	t1.join();

	std::thread t2(Alloc2);
	t2.join();
}

void ObjPoolTest()
{
	//TestPool();
}

void TestConcurrentAlloc()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(4);
	void* p4 = ConcurrentAlloc(3);
	void* p5 = ConcurrentAlloc(2);
	void* p6 = ConcurrentAlloc(1);

}
void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; ++i)
	{
		void* p1 = ConcurrentAlloc(7);
			std::cout << p1 << std::endl;
	}

	void* p2 = ConcurrentAlloc(7);
	std::cout << p2 << std::endl;
}

void TestConcurrentAlloc3()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(4);
	void* p4 = ConcurrentAlloc(3);
	void* p5 = ConcurrentAlloc(2);
	void* p6 = ConcurrentAlloc(1);
	void* p7 = ConcurrentAlloc(1);
	void* p8 = ConcurrentAlloc(1);


	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
	cout << p6 << endl;

	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
	ConcurrentFree(p6);
	ConcurrentFree(p7);
	ConcurrentFree(p8);



}


void MultiThreadAlloc1()
{
	//Sleep(0.01);
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}

void MultiThreadAlloc2()
{

	
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(16);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}

void MultiThead()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);
	//std::thread t3(MultiThreadAlloc2);


	t1.join();
	t2.join();
	//t3.join();

}

void BigAlloc()
{
	void* p1 = ConcurrentAlloc(258 * 1024);
	cout << p1 << endl;


	void* p3 = ConcurrentAlloc(254 * 1024);
	cout << p3 << endl;
	ConcurrentFree(p1);
	ConcurrentFree(p3);

	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	cout << p2 << endl;
	ConcurrentFree(p2);
}




//int main()
//{
//	//¶¨³¤
//	//ObjPoolTest();
//	
//	//¾²Ì¬TLS
//	//TLSTest();
//
//	//TestConcurrentAlloc();
//	//TestConcurrentAlloc2();
//	//TestConcurrentAlloc3();
//
//	//MultiThead();
//
//	BigAlloc();
//
//	return 0;
//}