#pragma once

#include"Common.hpp"
#include"ThreadCache.hpp"
#include"PageCache.hpp"

//��װ -- ����ռ�

static void* ConcurrentAlloc(size_t size)
{

	
	if (size > MAX_BYTES)
	{
		//����
		size_t alignSize = SizeClass::RoundUp(size);
		size_t k = alignSize >> PAGE_SHIFT;
		
		PageCache::GetInStance()->GetPageMtx().lock();
		Span* span = PageCache::GetInStance()->NewSpan(k);
		span->_isUse = true;//���Ϊʹ��
		span->_objSize = alignSize;
		PageCache::GetInStance()->GetPageMtx().unlock();

		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;
	}
	//�̵߳�һ������ռ�ʱ������ �̶߳�ռ�� ȫ�ֵ� ��̬TLS����
	else
	{
		if (pTLSThreadCache == nullptr)
		{
			//pTLSThreadCache = new ThreadCache;
			static ObjectPool<ThreadCache> tcPool;//��̬ȫ��
			pTLSThreadCache = tcPool.New();
		}

		//std::cout << std::this_thread::get_id() << " : " << pTLSThreadCache << std::endl;//ID : ��ַ

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