#pragma once

#include"Common.hpp"
#include"CentralCache.hpp"


class ThreadCache
{
public:

	//分配自由链表空间 -- 单位：对象
	void* Allocate(size_t size)
	{
		//对象内存最大为MAX_BYTES
		assert(size <= MAX_BYTES);

		size_t alignSize = SizeClass::RoundUp(size);
		size_t index = SizeClass::Index(size);

		//如果自由链表有空间
		if (!_freeLists[index].Empty())
		{
			return _freeLists[index].Pop();
		}
		//没空间向CentralCache申请空间
		else
		{
			return FetchFromCentralCache(index, alignSize);
		}

	}

	
	//回收空间到自由链表
	void Deallocate(void* ptr, size_t size)
	{
		assert(ptr);
		assert(size <= MAX_BYTES);

		//先找自由链表， 后头插
		size_t index = SizeClass::Index(size);
		_freeLists[index].Push(ptr);

		//回收
		//若当前自由链表的长度 大于 下次申请的节点的个数， 就回收
		if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
		{
			ListTooLong(_freeLists[index], size);
		}

	}


	//向central cache申请多个对象空间
	void* FetchFromCentralCache(size_t index, size_t size)
	{

		//慢开始反馈调节算法
		//1. 最开始不会向central cache 一次要太多
		//2. 如果一直有size大小内存的需要，那么batchNum就会不断增长
		size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
		if (_freeLists[index].MaxSize() == batchNum)
		{
			_freeLists[index].MaxSize() += 1; //修改自由链表每次申请的对象个数
		}

		void* start = nullptr;
		void* end = nullptr;
		size_t actualNum = CentralCache::GetInStance()->FetchRangeObj(start, end, batchNum, size);
		assert(actualNum > 0);

		//只有一个，直接分配
		if (actualNum == 1)
		{
			assert(start == end);
			return start;
		}
		//大于一个，把剩下的放到自由链表
		else
		{
			_freeLists[index].PushRange(ObjNext(start), end, actualNum - 1);
			return start;
		}
	}

	//回收空间交给CentralCache
	void ListTooLong(FreeList& list, size_t size)
	{
		void* start = nullptr;
		void* end = nullptr;
		list.PopRange(start, end, list.MaxSize());

		CentralCache::GetInStance()->ReleaseListToSpans(start, size);
	}

private:

	FreeList _freeLists[NFREELISTS];//自由链表
};

//静态全局变量
//静态TLS ThreadCache 线程独占的线程内全局
static _declspec(thread)ThreadCache* pTLSThreadCache = nullptr;
