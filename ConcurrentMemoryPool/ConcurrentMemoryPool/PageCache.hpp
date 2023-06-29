#pragma once

#include"Common.hpp"
#include"ObjectPool.hpp"
#include"PageMap.h"

class PageCache
{

public:
	//�ṩһ��ȫ�ַ��ʵ�
	static PageCache* GetInStance()
	{
		return &_sInst;
	}

	std::mutex& GetPageMtx()
	{
		return _pageMtx;
	}

	//���ݵ�ַ��ҳ�ţ�����map�ҵ���Ӧ��span�ڵ�
	Span* MapObjectToSpan(void* obj)
	{
		size_t id = (PAGE_ID)obj >> PAGE_SHIFT;

		//std::unique_lock<std::mutex> lock(PageCache::GetInStance()->GetPageMtx());//��mapʱ������

		//auto ret = _idSpanMap.find(id);
		//if (ret != _idSpanMap.end())
		//{
		//	return ret->second;
		//}
		//else
		//{
		//	assert(false);//�������
		//	return nullptr;
		//}
		auto ret = (Span*)_idSpanMap.get(id);
		assert(ret);

		return ret;
	}


public:

	//��ȡһ��Kҳ��span
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

			//�������
			//ӳ�� -- ��ҳ�ռ���Ϊ������ڣ� ����ʱҲ�����ڴ��ͷָ�룬ֻ��Ҫ��ҳ�Ž�������
			//_idSpanMap[span->_pageID] = span;
			_idSpanMap.set(span->_pageID, span);

			return span;
		}


		//1. �ȼ���k��Ͱ������û��span
		if (!_spanLists[k].Empty())
		{
			Span* span = _spanLists[k].PopFront();

			//ӳ��
			for (PAGE_ID i = 0; i < span->_n; ++i)
			{
				//_idSpanMap[span->_pageID + i] = span;
				_idSpanMap.set(span->_pageID + i, span);

			}

			return span;//���ظ�CentralCache
		}
		//2. �������Ͱ������û��sapn������У��з�
		for (size_t i = k + 1; i < NPAGES; ++i)
		{
			if (!_spanLists[i].Empty())
			{
				//i = k + j

				//�ҵ���span
				Span* nSpan = _spanLists[i].PopFront();
				//�����CentralCache��span
				//Span* kSpan = new Span();
				Span* kSpan = _spanPool.New();


				//��Ҫ��
				kSpan->_pageID = nSpan->_pageID;
				kSpan->_n = k;


				//ʣ�µ�
				nSpan->_pageID += k;
				nSpan->_n -= k;
				_spanLists[nSpan->_n].PushFront(nSpan);//�ҵ�PageCache�Ķ�Ӧ��Ͱ
				// �洢nSpan����βҳ�Ÿ�nSpan��ӳ�䣬 ����PageCache�����ڴ�ʱ
				// ���еĺϲ�����
				//_idSpanMap[nSpan->_pageID] = nSpan;
				//_idSpanMap[nSpan->_pageID + nSpan->_n - 1] = nSpan;
				_idSpanMap.set(nSpan->_pageID, nSpan);
				_idSpanMap.set(nSpan->_pageID + nSpan->_n - 1, nSpan);


				//����ҳ�ź�span��ӳ�䣬��������ڴ�ʱ����ҳ�Ų��Ҷ�Ӧ��span
				for (PAGE_ID i = 0; i < kSpan->_n; ++i)
				{
					//_idSpanMap[kSpan->_pageID + i] = kSpan;
					_idSpanMap.set(kSpan->_pageID + i, kSpan);
				}

				//����CentralCache
				return kSpan;
			}
		}

		//3. �Ҳ�������OS����
		//Span* bigSpan = new Span();
		Span* bigSpan = _spanPool.New();

		void* ptr = SystemAlloc(NPAGES - 1);
		//a. ��ʼ��
		bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;
		bigSpan->_n = NPAGES - 1;
		_spanLists[bigSpan->_n].PushFront(bigSpan);
		//�ݹ�����Լ� -- ���ٴ���
		return NewSpan(k);
	}

	//���տ���span�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span)
	{
		//����128ҳ��ֱ�ӻ�����
		if (span->_n > NPAGES - 1)
		{
			void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
			SystemFree(ptr);

			_spanPool.Delete(span);

			return ;
		}

		//��ǰ�ϲ�
		while (true)
		{
			//�ж��Ƿ��ܺϲ�Ϊ��ҳ
			PAGE_ID prevID = span->_pageID - 1;

			//auto ret = _idSpanMap.find(prevID);
			////������ǰ���ҳ�ռ�
			//if (ret == _idSpanMap.end())
			//{
			//	break;
			//}
			// 
			////���ڣ� ����ʹ����
			//Span* prevSpan = ret->second;
			//if(prevSpan->_isUse)
			//{
			//	break;
			//}
			////���������ҳ��
			//if (prevSpan->_n + span->_n > NPAGES - 1)
			//{
			//	break;
			//}
			//
			//span->_pageID = prevSpan->_pageID;//���ǰ���
			//span->_n += prevSpan->_n;

			//_spanLists[prevSpan->_n].Erase(prevSpan);

			////��ԭ����span��ҳ�ڵ���ͷ�
			////delete prevSpan;
			//_spanPool.Delete(prevSpan);

			////������ǰ�ϲ�

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

			span->_pageID = prevSpan->_pageID;//���ǰ���
			span->_n += prevSpan->_n;

			_spanLists[prevSpan->_n].Erase(prevSpan);

			//��ԭ����span��ҳ�ڵ���ͷ�
			//delete prevSpan;
			_spanPool.Delete(prevSpan);

			//������ǰ�ϲ�

		}
		//���ϲ�
		while (true)
		{
			//�ж��Ƿ��ܺϲ�Ϊ��ҳ
			PAGE_ID nextID = span->_pageID + span->_n;
			//auto ret = _idSpanMap.find(nextID);
			////�����ں����ҳ�ռ�
			//if (ret == _idSpanMap.end())
			//{
			//	break;
			//}
			////���ڣ� ����ʹ����
			//Span* nextSpan = ret->second;
			//if (nextSpan->_isUse)
			//{
			//	break;
			//}
			////���������ҳ��
			//if (nextSpan->_n + span->_n > NPAGES - 1)
			//{
			//	break;
			//}

			//span->_pageID;//����
			//span->_n += nextSpan->_n;

			////��Ͱ����ɾ��
			//_spanLists[nextSpan->_n].Erase(nextSpan);

			////��ԭ����span��ҳ�ڵ���ͷ�
			////delete nextSpan;
			//_spanPool.Delete(nextSpan);

			////�������ϲ�


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

			//��ԭ����span��ҳ�ڵ���ͷ�
			//delete prevSpan;
			_spanPool.Delete(nextSpan);

			//������ǰ�ϲ�

		}

		_spanLists[span->_n].PushFront(span);
		span->_isUse = false;//Ϊ����Ͱ
		
		//���µ�span�����½���ӳ��
		//_idSpanMap[span->_pageID] = span;
		//_idSpanMap[span->_pageID + span->_n - 1] = span;
		_idSpanMap.set(span->_pageID, span);
		_idSpanMap.set(span->_pageID + span->_n - 1, span);

		
	}

	

private:
	SpanList _spanLists[NPAGES];
	std::mutex _pageMtx; //����

	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;//ҳ����span�ڵ��ӳ��
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	ObjectPool<Span> _spanPool;
private:
	PageCache() //���캯��˽��
	{}
	PageCache(const PageCache&) = delete; //������
	PageCache operator=(const PageCache&) = delete;

	static PageCache _sInst;
};