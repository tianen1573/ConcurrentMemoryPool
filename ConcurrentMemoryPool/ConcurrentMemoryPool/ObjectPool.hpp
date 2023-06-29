#pragma once

#include<iostream>
#include<vector>
#include<time.h>

#include"Common.hpp"



template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* ptr = nullptr;

		//优先使用自由链表的空间
		if (_freeListPtr)
		{
			void* next = *(void**)_freeListPtr;//自由链表的下一个节点的指针存在当前节点的前面，这种方式可以取到前指针大小的内存
			ptr = (T*)_freeListPtr;
			_freeListPtr = next;
		}
		else
		{
			//剩余内存不够一次使用，重新开辟大块内存
			if (_remainBytes < sizeof(T))
			{
				//_remainBytes = 128 * 1024;//字节数
				//_memoryPtr = (char*)malloc(_remainBytes);

				//字节数 -- 需要根据 对象大小 调整
				//可以考虑在构造函数时，指定对象个数
				_remainBytes = 128 * 1024;
				_memoryPtr = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);

				//申请页空间
				if (_memoryPtr == nullptr)
				{
					throw std::bad_alloc();//抛异常
				}
			}

			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);//分配的内存至少可以存储指针
			ptr = (T*)_memoryPtr;
			_memoryPtr += objSize;
			_remainBytes -= objSize;
		}

		// 我们给对象“分配”的是已经存在的内存，需要使用定位new初始化
		// 显式调用构造函数初始化 -- 定位new
		new(ptr)T;
		//ptr->T();

		return ptr;
	}
	void Delete(T* ptr)
	{
		// 显示调用析构函数清理对象，不释放内存
		ptr->~T();
		//delete(ptr)T;//会释放内存

		// 头插
		*(void**)ptr = _freeListPtr;
		_freeListPtr = ptr;
	}

private:
	char* _memoryPtr = nullptr;// 指向大块内存的指针
	size_t _remainBytes = 0;// 当前大块内存的剩余字节数
	void* _freeListPtr = nullptr;//释放的自由链表头指针
};


//struct TreeNode
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};
//
//void TestPool()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 5;
//
//	// 每轮申请释放多少次
//	const size_t N = 100000;
//
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//
//	size_t begin1 = clock();
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
//
//	size_t end1 = clock();
//
//
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//
//	std::cout << "new cost time:" << end1 - begin1 << std::endl;
//	std::cout << "object pool cost time:" << end2 - begin2 << std::endl;
//
//}