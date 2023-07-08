#pragma once

#include"Common.hpp"
#include"ObjectPool.hpp"
#include"PageMap.h"

class PageCache
{

public:
	//提供一个全局访问点
	static PageCache* GetInStance()
	{
		return &_sInst;
	}

	std::mutex& GetPageMtx()
	{
		return _pageMtx;
	}

	//根据地址算页号，并从map找到对应的span节点
	Span* MapObjectToSpan(void* obj)
	{
		size_t id = (PAGE_ID)obj >> PAGE_SHIFT;

		//std::unique_lock<std::mutex> lock(PageCache::GetInStance()->GetPageMtx());//读map时，加锁

		//auto ret = _idSpanMap.find(id);
		//if (ret != _idSpanMap.end())
		//{
		//	return ret->second;
		//}
		//else
		//{
		//	assert(false);//必须存在
		//	return nullptr;
		//}
		auto ret = (Span*)_idSpanMap.get(id);
		assert(ret);

		return ret;
	}


public:

	//获取一个K页的span
	Span* NewSpan(size_t k)
	{

		assert(k > 0);

		if (k > NPAGES - 1)
		{
			//Span* span = new Span();
			Span* span = _spanPool.New();
			void* ptr = SystemAlloc(k);

			span->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;
			span->_n = k;

			//方便回收
			//映射 -- 大页空间作为整体存在， 返回时也是用内存的头指针，只需要将页号建立即可
			//_idSpanMap[span->_pageID] = span;
			_idSpanMap.set(span->_pageID, span);

			return span;
		}


		//1. 先检查第k个桶里面有没有span
		if (!_spanLists[k].Empty())
		{
			Span* span = _spanLists[k].PopFront();

			//映射
			for (PAGE_ID i = 0; i < span->_n; ++i)
			{
				//_idSpanMap[span->_pageID + i] = span;
				_idSpanMap.set(span->_pageID + i, span);

			}

			return span;//返回给CentralCache
		}
		//2. 检查后面的桶里面有没有sapn，如果有，切分
		for (size_t i = k + 1; i < NPAGES; ++i)
		{
			if (!_spanLists[i].Empty())
			{
				//i = k + j

				//找到的span
				Span* nSpan = _spanLists[i].PopFront();
				//分配的CentralCache的span
				//Span* kSpan = new Span();
				Span* kSpan = _spanPool.New();


				//需要的
				kSpan->_pageID = nSpan->_pageID;
				kSpan->_n = k;


				//剩下的
				nSpan->_pageID += k;
				nSpan->_n -= k;
				_spanLists[nSpan->_n].PushFront(nSpan);//挂到PageCache的对应的桶
				// 存储nSpan的首尾页号跟nSpan的映射， 方便PageCache回收内存时
				// 进行的合并查找
				//_idSpanMap[nSpan->_pageID] = nSpan;
				//_idSpanMap[nSpan->_pageID + nSpan->_n - 1] = nSpan;
				_idSpanMap.set(nSpan->_pageID, nSpan);
				_idSpanMap.set(nSpan->_pageID + nSpan->_n - 1, nSpan);


				//建立页号和span的映射，方便回收内存时根据页号查找对应的span
				for (PAGE_ID i = 0; i < kSpan->_n; ++i)
				{
					//_idSpanMap[kSpan->_pageID + i] = kSpan;
					_idSpanMap.set(kSpan->_pageID + i, kSpan);
				}

				//交给CentralCache
				return kSpan;
			}
		}

		//3. 找不到，向OS申请
		//Span* bigSpan = new Span();
		Span* bigSpan = _spanPool.New();

		void* ptr = SystemAlloc(NPAGES - 1);
		//a. 初始化
		bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;
		bigSpan->_n = NPAGES - 1;
		_spanLists[bigSpan->_n].PushFront(bigSpan);
		//递归调用自己 -- 减少代码
		return NewSpan(k);
	}

	//回收空闲span，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span)
	{
		//大于128页的直接还给堆
		if (span->_n > NPAGES - 1)
		{
			void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
			SystemFree(ptr);

			_spanPool.Delete(span);

			return ;
		}

		//向前合并
		while (true)
		{
			//判断是否能合并为大页
			PAGE_ID prevID = span->_pageID - 1;

			//auto ret = _idSpanMap.find(prevID);
			////不存在前面的页空间
			//if (ret == _idSpanMap.end())
			//{
			//	break;
			//}
			// 
			////存在， 但被使用了
			//Span* prevSpan = ret->second;
			//if(prevSpan->_isUse)
			//{
			//	break;
			//}
			////超过了最大页数
			//if (prevSpan->_n + span->_n > NPAGES - 1)
			//{
			//	break;
			//}
			//
			//span->_pageID = prevSpan->_pageID;//变成前面的
			//span->_n += prevSpan->_n;

			//_spanLists[prevSpan->_n].Erase(prevSpan);

			////把原来的span大页节点给释放
			////delete prevSpan;
			//_spanPool.Delete(prevSpan);

			////继续向前合并

			auto ret = (Span*)_idSpanMap.get(prevID);
			if (ret == nullptr)
			{
				break;
			}
			Span* prevSpan = ret;
			if (prevSpan->_isUse)
			{
				break;
			}

			span->_pageID = prevSpan->_pageID;//变成前面的
			span->_n += prevSpan->_n;

			_spanLists[prevSpan->_n].Erase(prevSpan);

			//把原来的span大页节点给释放
			//delete prevSpan;
			_spanPool.Delete(prevSpan);

			//继续向前合并

		}
		//向后合并
		while (true)
		{
			//判断是否能合并为大页
			PAGE_ID nextID = span->_pageID + span->_n;
			//auto ret = _idSpanMap.find(nextID);
			////不存在后面的页空间
			//if (ret == _idSpanMap.end())
			//{
			//	break;
			//}
			////存在， 但被使用了
			//Span* nextSpan = ret->second;
			//if (nextSpan->_isUse)
			//{
			//	break;
			//}
			////超过了最大页数
			//if (nextSpan->_n + span->_n > NPAGES - 1)
			//{
			//	break;
			//}

			//span->_pageID;//不变
			//span->_n += nextSpan->_n;

			////从桶里面删掉
			//_spanLists[nextSpan->_n].Erase(nextSpan);

			////把原来的span大页节点给释放
			////delete nextSpan;
			//_spanPool.Delete(nextSpan);

			////继续向后合并


			auto ret = (Span*)_idSpanMap.get(nextID);
			if (ret == nullptr)
			{
				break;
			}
			Span* nextSpan = ret;
			if (nextSpan->_isUse)
			{
				break;
			}

			span->_n += nextSpan->_n;

			_spanLists[nextSpan->_n].Erase(nextSpan);

			//把原来的span大页节点给释放
			//delete prevSpan;
			_spanPool.Delete(nextSpan);

			//继续向前合并

		}

		_spanLists[span->_n].PushFront(span);
		span->_isUse = false;//为假入桶
		
		//对新的span，重新建立映射
		//_idSpanMap[span->_pageID] = span;
		//_idSpanMap[span->_pageID + span->_n - 1] = span;
		_idSpanMap.set(span->_pageID, span);
		_idSpanMap.set(span->_pageID + span->_n - 1, span);

		
	}

	

private:
	SpanList _spanLists[NPAGES];
	std::mutex _pageMtx; //大锁

	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;//页号与span节点的映射
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	ObjectPool<Span> _spanPool;
private:
	PageCache() //构造函数私有
	{}
	PageCache(const PageCache&) = delete; //防拷贝
	PageCache operator=(const PageCache&) = delete;

	static PageCache _sInst;
};