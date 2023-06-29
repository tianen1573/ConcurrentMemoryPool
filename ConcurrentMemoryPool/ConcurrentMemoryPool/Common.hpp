#define _CRT_SECURE_NO_WARNINGS 1

#pragma once

#include<iostream>
#include<vector>
#include<algorithm>
#include<unordered_map>

#include<time.h>
#include<assert.h>

//申请页空间
#ifdef _WIN32
#include<Windows.h>
#else
#include<unistd.h>
#endif

#include<mutex>//锁
#include<thread>//线程

//#define PTR_NEXT(obj) (*(void**)obj) //尽量不使用宏

static const int MAX_BYTES = 256 * 1024;//256KB：能向ThreadCache单次申请的最大字节数
static const size_t NFREELISTS = 208;//ThreadCache/CentralCache的桶的个数
static const size_t NPAGES = 129;//page cache中哈希桶的个数,也是映射的页的个数

//不同位平台下内存的大小
//_WIN32 和 _WIN64 : x86平台只定义_WIN32，x64都定义，所以顺序不能反
#ifdef _WIN64
static const size_t PAGE_SHIFT = 13;//页的大小是2的13次幂
#elif _WIN32
static const size_t PAGE_SHIFT = 12;//页的大小是2的12次幂
#else
	//linux
#endif

//不同位平台下内存的页数
//_WIN32 和 _WIN64 : x86平台只定义_WIN32，x64都定义，所以顺序不能反
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
	//linux
#endif

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)//kpage：页数
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
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
	//linux下sbrk unmmap等
#endif
}

//自由链表节点保存下一结点的空间
static void*& ObjNext(void* ptr)
{
	return (*(void**)ptr);
}

//管理切分好的小对象的自由链表, 每个自由链表的节点大小时固定的
class FreeList
{
public:

	//归还空间 -- 头插
	void Push(void* obj)
	{
		//assert(_freeList);
		assert(obj);

		ObjNext(obj) = _freeList;
		_freeList = obj;
		++ _size;
	}
	//申请自由链表的空间 -- 头删
	void* Pop()
	{
		assert(_freeList);//自由链表必须有节点

		void* obj = _freeList;
		_freeList = ObjNext(_freeList);
		-- _size;
		
		return obj;
	}

	void PushRange(void* start, void* end, size_t n)
	{
		assert(start && end);

		ObjNext(end) = _freeList;
		_freeList = start;

		//测试
		/*int i = 0;
		void* cur = start;
		while (cur)
		{
			cur = ObjNext(cur);
			++i;
		}

		if (n != i)
		{
			int x = 0;
		}*/

		_size += n;

	}
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);
		start = _freeList;
		end = start;
		
		for (size_t i = 0; i < n - 1; ++ i)
		{
			////1. 条件断点
			////2. 查看栈帧
			//if (!end)
			//{
			//	int a = 0;
			//}
			end = ObjNext(end);
		}
		_freeList = ObjNext(end);
		ObjNext(end) = nullptr;

		_size -= n;
	}
public:
	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t& Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;//自由链表头节点
	size_t _maxSize = 1;
	size_t _size = 0;
};


// 计算对象大小，及其映射规则
class SizeClass
{
public:
	/*static inline size_t _RoundUp(size_t size, size_t alignNum)
	{
		size_t alignSize = 0;
		if (size % alignNum != 0)
		{
			alignSize = (size / alignNum + 1) * alignNum;
		}
		else
		{
			alignSize = size;
		}
		return alignSize;
	}*/
	//位运算
	static inline  size_t _RoundUp(size_t size, size_t alignSize)
	{
		return ((size + alignSize - 1) & ~(alignSize - 1));

	}

	/*static inline size_t _Index(size_t size, size_t alignNum)//对齐大小
	{
		size_t index = 0;
		if (size % alignNum != 0)
		{
			index = size / alignNum;
		}
		else
		{
			index = size / alignNum - 1;
		}
		return index;
	}*/
	//位运算
	static inline size_t _Index(size_t size, size_t alignShift)//对齐大小的最高位
	{
		return ((size + (1 << alignShift) - 1) >> alignShift) - 1;
	}
public:


	// 整体控制在最多10%左右的内碎片浪费
	// [1,128] 8byte对齐       freelist[0,16)
	// [128+1,1024] 16byte对齐   freelist[16,72)
	// [1024+1,8*1024] 128byte对齐   freelist[72,128)
	// [8*1024+1,64*1024] 1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024] 8*1024byte对齐   freelist[184,208)
	static size_t RoundUp(size_t size)//计算应该分配的内存大小 -- 对齐数
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	static size_t Index(size_t size)//桶下标
	{
		//每个区间有多少个自由链表
		static size_t groupArray[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{
			return _Index(size, 3);
		}
		else if (size <= 1024)
		{
			return _Index(size - 128, 4) + groupArray[0];
		}
		else if (size <= 8 * 1024)
		{
			return _Index(size - 1024, 7) + groupArray[0] + groupArray[1];
		}
		else if (size <= 64 * 1024)
		{
			return _Index(size - 8 * 1024, 10) + groupArray[0] + groupArray[1] + groupArray[2];
		}
		else if (size <= 256 * 1024)
		{
			return _Index(size - 64 * 1024, 13) + groupArray[0] + groupArray[1] + groupArray[2] + groupArray[3];
		}
		else
		{
			assert(false);
			return -1;
		}
	}

	//thread cache一次从central cache获取对象的上限
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		//[2, 512]
		//对象越小，获得的个数越多
		//对象越大，获得的个数越少
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;

		return num;
	}

	//CentralCacahe 一次向 PageCache申请的页数
	//单个对象的内存越小，分配的页数越少
	//
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


//管理多个连续页大块内存跨度结构
class Span
{
public:
	//Span初始化场景有点多，不建议在这里初始化，在创建出来后，根据情况初始化
	Span()
	{
	}
	~Span()
	{
	}
public:
	PAGE_ID _pageID = 0;//页ID
	size_t _n = 0;//页数

	Span* _next = nullptr;//后指针
	Span* _prev = nullptr;//前指针

	size_t _objSize = 0;
	size_t _useCount = 0;//切好的小块内存的使用计数 -- 使用的数量
	void* _freeList = nullptr; //切好的小块内存自由链表

	bool _isUse = false;// 是否被使用
};

// 带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new Span();
		_head->_next = _head;
		_head->_prev = _head;
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

	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		// prev newSpan pos
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}
	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);//头节点不能删
		
		if (pos == _head)//条件断点
		{

		}

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		
		prev->_next = next;
		next->_prev = prev;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

	std::mutex& GetMutex()
	{
		return _mtx;
	}

private:
	Span* _head;//头节点
	std::mutex _mtx;//桶锁
};