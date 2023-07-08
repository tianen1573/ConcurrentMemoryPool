#pragma once

#include "Common.hpp"
#include "PageCache.hpp"

//单例模式--饿汉
//中心件
class CentralCache
{
public:
	static CentralCache* GetInStance()
	{
		return &_sInst;
	}
public:

	//获取span
	//若CentralCache的对应大小的span自由链表存在有空间的span，返回对应span
	//若不存在，向PageCache申请sapn对象空间
	Span* GetOneSpan(SpanList& list, size_t size)//此时list已经是对应大小的自由链表
	{
		Span* it = list.Begin();
		//遍历Span自由链表，找非空span
		while (it != list.End())
		{
			//存在空间
			if (it->_freeList != nullptr)
			{
				return it;
			}
			//不存在
			else
			{
				it = it->_next;
			}
		}

		// 此时，CentralCache对应大小的span自由链表不存在非空span
		//需要向PageCache申请span

		//此时，把CentralCacahe桶锁解开，则其他线程释放内存可以不阻塞
		//并把PageCache上锁，则其他进程申请内存内存时，会阻塞在大锁这里
		//都阻塞在大锁，代表可能会申请到多个span，可以在大锁后加一个判断条件，仍不存在span时，再去申请
		list.GetMutex().unlock();

		PageCache::GetInStance()->GetPageMtx().lock();
		//线程少的情况下，很难并发申请span，手动并发
		//Sleep(1);
		Span* span = PageCache::GetInStance()->NewSpan(SizeClass::NumMovePage(size));
		span->_isUse = true;
		span->_objSize = size;
		PageCache::GetInStance()->GetPageMtx().unlock();


		//切分后挂到span自由链表
		//计算span的大块内存的起始地址和其字节数
		char* start = (char*)(span->_pageID << PAGE_SHIFT);
		size_t bytes = span->_n << PAGE_SHIFT;
		char* end = start + bytes;
		//将大块内存切分好并挂到span自由链表
		// 1. 先切一块， 方便做尾插
		span->_freeList = start;
		start += size;
		void* tail = span->_freeList;
		//尾插切分
		while (start < end)
		{
			ObjNext(tail) = start;
			tail = start;
			start += size;
		}
		ObjNext(tail) = nullptr;

		//测试
		/*int ii = 0;
		void* cur = span->_freeList;
		while (cur)
		{
			cur = ObjNext(cur);
			++ii;
		}

		if (bytes / size != ii)
		{
			int x = 0;
		}*/

		//2. 将span放到list里
		//需要对桶加锁
		list.GetMutex().lock();
		list.PushFront(span);

		return span;
	}

	//从span自由链表中获取span，并分配span的空间给ThreadCache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
	{
		size_t index = SizeClass::Index(size);

		//临界区
		_spanLists[index].GetMutex().lock();

		//获取span
		Span* span = GetOneSpan(_spanLists[index], size);
		assert(span);
		assert(span->_freeList);

		//从span中获取batchNum个对象
		//若不够，有多少拿多少
		start = span->_freeList;
		end = start;
		size_t i = 0;
		size_t actualNum = 1;
		while (i < batchNum - 1 && ObjNext(end))
		{
			end = ObjNext(end);
			++ actualNum;
			++i;
		}
		span->_freeList = ObjNext(end);
		ObjNext(end) = nullptr;
		span->_useCount += actualNum;

		//测试
		/*int ii = 0;
		void* cur = start;
		while (cur)
		{
			cur = ObjNext(cur);
			++ii;
		}

		if (actualNum != ii)
		{
			int x = 0;
		}*/

		_spanLists[index].GetMutex().unlock();
		//解锁
		return actualNum;
	}

	//将小块内存从自由链表回收到CentralCache的桶的span里
	void ReleaseListToSpans(void* start, size_t size)
	{
		size_t index = SizeClass::Index(size);

		_spanLists[index].GetMutex().lock();

		while (start)
		{
			void* next = ObjNext(start);

			Span* span = PageCache::GetInStance()->MapObjectToSpan(start);

			ObjNext(start) = span->_freeList;
			span->_freeList = start;
			-- span->_useCount;

			//回收到PageCache
			//若引用计数为0，代表该页空间未被使用，可以回收到PageCache
			if (span->_useCount == 0)
			{
				//析构span对象
				//此时_isUse应该为真，因为在多线程环境下，span从CentralCache里剥离出来，但还未添加到PageCache中，是这俩组件间的游离态
				// 这个span被分配到CentralCache，代表着这个节点已经建立了 页号-节点 映射关系
				// 代表我们在后面合并span时，是能够找到这个节点的
				// 若这个节点对应的页号的前一个节点被回收到PageCache，正在进行添加前的合并工作
				// 若通过map+页号找到了这个节点，会判断isUse，为真代表在CentralCache，为假代表在PageCache，只有为假且在PageCache时，才能合并
				// 如果额外判断是否在PageCache，需要遍历，增加工作量
				// 我们不妨使节点的isUse暂时为真，避免上述情况
				// 进入合并的节点我们能保证其是不被使用的，在合并工作完成后再使其isUse为假
				_spanLists[index].Erase(span);
				span->_next = nullptr;
				span->_prev = nullptr;
				span->_freeList = nullptr;
				

				//此时，空span不再在同理，可以解锁
				//其他线程可以返回或者申请内存
				_spanLists[index].GetMutex().unlock();

				//上大锁
				PageCache::GetInStance()->GetPageMtx().lock();
				PageCache::GetInStance()->ReleaseSpanToPageCache(span);
				PageCache::GetInStance()->GetPageMtx().unlock();

				//桶锁
				_spanLists[index].GetMutex().lock();
			}

			start = next;
		}

		_spanLists[index].GetMutex().unlock();


	}

private:
	SpanList _spanLists[NFREELISTS];//Span桶

private:
	//私有化构造函数，禁止默认拷贝和赋值重载生成
	CentralCache()
	{

	}
	CentralCache(const CentralCache& cen) = delete;
	CentralCache operator=(const CentralCache& cen) = delete;

	static CentralCache _sInst;//声明
};