#pragma once

#include "Common.hpp"
#include "PageCache.hpp"

//����ģʽ--����
//���ļ�
class CentralCache
{
public:
	static CentralCache* GetInStance()
	{
		return &_sInst;
	}
public:

	//��ȡspan
	//��CentralCache�Ķ�Ӧ��С��span������������пռ��span�����ض�Ӧspan
	//�������ڣ���PageCache����sapn����ռ�
	Span* GetOneSpan(SpanList& list, size_t size)//��ʱlist�Ѿ��Ƕ�Ӧ��С����������
	{
		Span* it = list.Begin();
		//����Span���������ҷǿ�span
		while (it != list.End())
		{
			//���ڿռ�
			if (it->_freeList != nullptr)
			{
				return it;
			}
			//������
			else
			{
				it = it->_next;
			}
		}

		// ��ʱ��CentralCache��Ӧ��С��span�����������ڷǿ�span
		//��Ҫ��PageCache����span

		//��ʱ����CentralCacaheͰ���⿪���������߳��ͷ��ڴ���Բ�����
		//����PageCache���������������������ڴ��ڴ�ʱ���������ڴ�������
		//�������ڴ�����������ܻ����뵽���span�������ڴ������һ���ж��������Բ�����spanʱ����ȥ����
		list.GetMutex().unlock();

		PageCache::GetInStance()->GetPageMtx().lock();
		//�߳��ٵ�����£����Ѳ�������span���ֶ�����
		//Sleep(1);
		Span* span = PageCache::GetInStance()->NewSpan(SizeClass::NumMovePage(size));
		span->_isUse = true;
		span->_objSize = size;
		PageCache::GetInStance()->GetPageMtx().unlock();


		//�зֺ�ҵ�span��������
		//����span�Ĵ���ڴ����ʼ��ַ�����ֽ���
		char* start = (char*)(span->_pageID << PAGE_SHIFT);
		size_t bytes = span->_n << PAGE_SHIFT;
		char* end = start + bytes;
		//������ڴ��зֺò��ҵ�span��������
		// 1. ����һ�飬 ������β��
		span->_freeList = start;
		start += size;
		void* tail = span->_freeList;
		//β���з�
		while (start < end)
		{
			ObjNext(tail) = start;
			tail = start;
			start += size;
		}
		ObjNext(tail) = nullptr;

		//����
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

		//2. ��span�ŵ�list��
		//��Ҫ��Ͱ����
		list.GetMutex().lock();
		list.PushFront(span);

		return span;
	}

	//��span���������л�ȡspan��������span�Ŀռ��ThreadCache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
	{
		size_t index = SizeClass::Index(size);

		//�ٽ���
		_spanLists[index].GetMutex().lock();

		//��ȡspan
		Span* span = GetOneSpan(_spanLists[index], size);
		assert(span);
		assert(span->_freeList);

		//��span�л�ȡbatchNum������
		//���������ж����ö���
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

		//����
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
		//����
		return actualNum;
	}

	//��С���ڴ������������յ�CentralCache��Ͱ��span��
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

			//���յ�PageCache
			//�����ü���Ϊ0�������ҳ�ռ�δ��ʹ�ã����Ի��յ�PageCache
			if (span->_useCount == 0)
			{
				//����span����
				//��ʱ_isUseӦ��Ϊ�棬��Ϊ�ڶ��̻߳����£�span��CentralCache��������������δ��ӵ�PageCache�У�����������������̬
				// ���span�����䵽CentralCache������������ڵ��Ѿ������� ҳ��-�ڵ� ӳ���ϵ
				// ���������ں���ϲ�spanʱ�����ܹ��ҵ�����ڵ��
				// ������ڵ��Ӧ��ҳ�ŵ�ǰһ���ڵ㱻���յ�PageCache�����ڽ������ǰ�ĺϲ�����
				// ��ͨ��map+ҳ���ҵ�������ڵ㣬���ж�isUse��Ϊ�������CentralCache��Ϊ�ٴ�����PageCache��ֻ��Ϊ������PageCacheʱ�����ܺϲ�
				// ��������ж��Ƿ���PageCache����Ҫ���������ӹ�����
				// ���ǲ���ʹ�ڵ��isUse��ʱΪ�棬�����������
				// ����ϲ��Ľڵ������ܱ�֤���ǲ���ʹ�õģ��ںϲ�������ɺ���ʹ��isUseΪ��
				_spanLists[index].Erase(span);
				span->_next = nullptr;
				span->_prev = nullptr;
				span->_freeList = nullptr;
				

				//��ʱ����span������ͬ�����Խ���
				//�����߳̿��Է��ػ��������ڴ�
				_spanLists[index].GetMutex().unlock();

				//�ϴ���
				PageCache::GetInStance()->GetPageMtx().lock();
				PageCache::GetInStance()->ReleaseSpanToPageCache(span);
				PageCache::GetInStance()->GetPageMtx().unlock();

				//Ͱ��
				_spanLists[index].GetMutex().lock();
			}

			start = next;
		}

		_spanLists[index].GetMutex().unlock();


	}

private:
	SpanList _spanLists[NFREELISTS];//SpanͰ

private:
	//˽�л����캯������ֹĬ�Ͽ����͸�ֵ��������
	CentralCache()
	{

	}
	CentralCache(const CentralCache& cen) = delete;
	CentralCache operator=(const CentralCache& cen) = delete;

	static CentralCache _sInst;//����
};