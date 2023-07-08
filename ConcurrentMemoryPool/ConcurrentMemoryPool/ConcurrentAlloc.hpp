#pragma once

#include"Common.hpp"
#include"ThreadCache.hpp"
#include"PageCache.hpp"

//封装 -- 申请空间

static void* ConcurrentAlloc(size_t size)
{

	
	if (size > MAX_BYTES)
	{
		//对齐
		size_t alignSize = SizeClass::RoundUp(size);
		size_t k = alignSize >> PAGE_SHIFT;
		
		PageCache::GetInStance()->GetPageMtx().lock();
		Span* span = PageCache::GetInStance()->NewSpan(k);
		span->_isUse = true;//标记为使用
		span->_objSize = alignSize;
		PageCache::GetInStance()->GetPageMtx().unlock();

		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;
	}
	//线程第一次申请空间时，创建 线程独占的 全局的 静态TLS对象
	else
	{
		if (pTLSThreadCache == nullptr)
		{
			//pTLSThreadCache = new ThreadCache;
			static ObjectPool<ThreadCache> tcPool;//静态全局
			pTLSThreadCache = tcPool.New();
		}

		//std::cout << std::this_thread::get_id() << " : " << pTLSThreadCache << std::endl;//ID : 地址

		return pTLSThreadCache->Allocate(size);
	}
	
	
}

static void ConcurrentFree(void* ptr)
{

	Span* span = PageCache::GetInStance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;

	if (size > MAX_BYTES)
	{

		PageCache::GetInStance()->GetPageMtx().lock();
		PageCache::GetInStance()->ReleaseSpanToPageCache(span);
		PageCache::GetInStance()->GetPageMtx().unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		assert(ptr);
		pTLSThreadCache->Deallocate(ptr, size);
	}
	
}