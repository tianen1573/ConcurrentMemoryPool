#define _CRT_SECURE_NO_WARNINGS 1

#pragma once

#include<iostream>
#include<vector>
#include<algorithm>
#include<unordered_map>

#include<time.h>
#include<assert.h>

//����ҳ�ռ�
#ifdef _WIN32
#include<Windows.h>
#else
#include<unistd.h>
#endif

#include<mutex>//��
#include<thread>//�߳�

//#define PTR_NEXT(obj) (*(void**)obj) //������ʹ�ú�

static const int MAX_BYTES = 256 * 1024;//256KB������ThreadCache�������������ֽ���
static const size_t NFREELISTS = 208;//ThreadCache/CentralCache��Ͱ�ĸ���
static const size_t NPAGES = 129;//page cache�й�ϣͰ�ĸ���,Ҳ��ӳ���ҳ�ĸ���

//��ͬλƽ̨���ڴ�Ĵ�С
//_WIN32 �� _WIN64 : x86ƽֻ̨����_WIN32��x64�����壬����˳���ܷ�
#ifdef _WIN64
static const size_t PAGE_SHIFT = 13;//ҳ�Ĵ�С��2��13����
#elif _WIN32
static const size_t PAGE_SHIFT = 12;//ҳ�Ĵ�С��2��12����
#else
	//linux
#endif

//��ͬλƽ̨���ڴ��ҳ��
//_WIN32 �� _WIN64 : x86ƽֻ̨����_WIN32��x64�����壬����˳���ܷ�
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
	//linux
#endif

// ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)//kpage��ҳ��
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
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
	//linux��sbrk unmmap��
#endif
}

//��������ڵ㱣����һ���Ŀռ�
static void*& ObjNext(void* ptr)
{
	return (*(void**)ptr);
}

//�����зֺõ�С�������������, ÿ����������Ľڵ��Сʱ�̶���
class FreeList
{
public:

	//�黹�ռ� -- ͷ��
	void Push(void* obj)
	{
		//assert(_freeList);
		assert(obj);

		ObjNext(obj) = _freeList;
		_freeList = obj;
		++ _size;
	}
	//������������Ŀռ� -- ͷɾ
	void* Pop()
	{
		assert(_freeList);//������������нڵ�

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

		//����
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
			////1. �����ϵ�
			////2. �鿴ջ֡
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
	void* _freeList = nullptr;//��������ͷ�ڵ�
	size_t _maxSize = 1;
	size_t _size = 0;
};


// ��������С������ӳ�����
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
	//λ����
	static inline  size_t _RoundUp(size_t size, size_t alignSize)
	{
		return ((size + alignSize - 1) & ~(alignSize - 1));

	}

	/*static inline size_t _Index(size_t size, size_t alignNum)//�����С
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
	//λ����
	static inline size_t _Index(size_t size, size_t alignShift)//�����С�����λ
	{
		return ((size + (1 << alignShift) - 1) >> alignShift) - 1;
	}
public:


	// ������������10%���ҵ�����Ƭ�˷�
	// [1,128] 8byte����       freelist[0,16)
	// [128+1,1024] 16byte����   freelist[16,72)
	// [1024+1,8*1024] 128byte����   freelist[72,128)
	// [8*1024+1,64*1024] 1024byte����     freelist[128,184)
	// [64*1024+1,256*1024] 8*1024byte����   freelist[184,208)
	static size_t RoundUp(size_t size)//����Ӧ�÷�����ڴ��С -- ������
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

	static size_t Index(size_t size)//Ͱ�±�
	{
		//ÿ�������ж��ٸ���������
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

	//thread cacheһ�δ�central cache��ȡ���������
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		//[2, 512]
		//����ԽС����õĸ���Խ��
		//����Խ�󣬻�õĸ���Խ��
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;

		return num;
	}

	//CentralCacahe һ���� PageCache�����ҳ��
	//����������ڴ�ԽС�������ҳ��Խ��
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


//����������ҳ����ڴ��Ƚṹ
class Span
{
public:
	//Span��ʼ�������е�࣬�������������ʼ�����ڴ��������󣬸��������ʼ��
	Span()
	{
	}
	~Span()
	{
	}
public:
	PAGE_ID _pageID = 0;//ҳID
	size_t _n = 0;//ҳ��

	Span* _next = nullptr;//��ָ��
	Span* _prev = nullptr;//ǰָ��

	size_t _objSize = 0;
	size_t _useCount = 0;//�кõ�С���ڴ��ʹ�ü��� -- ʹ�õ�����
	void* _freeList = nullptr; //�кõ�С���ڴ���������

	bool _isUse = false;// �Ƿ�ʹ��
};

// ��ͷ˫��ѭ������
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
		assert(pos != _head);//ͷ�ڵ㲻��ɾ
		
		if (pos == _head)//�����ϵ�
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
	Span* _head;//ͷ�ڵ�
	std::mutex _mtx;//Ͱ��
};