#pragma once

#include"Common.hpp"
#include"CentralCache.hpp"


class ThreadCache
{
public:

	//������������ռ� -- ��λ������
	void* Allocate(size_t size)
	{
		//�����ڴ����ΪMAX_BYTES
		assert(size <= MAX_BYTES);

		size_t alignSize = SizeClass::RoundUp(size);
		size_t index = SizeClass::Index(size);

		//������������пռ�
		if (!_freeLists[index].Empty())
		{
			return _freeLists[index].Pop();
		}
		//û�ռ���CentralCache����ռ�
		else
		{
			return FetchFromCentralCache(index, alignSize);
		}

	}

	
	//���տռ䵽��������
	void Deallocate(void* ptr, size_t size)
	{
		assert(ptr);
		assert(size <= MAX_BYTES);

		//������������ ��ͷ��
		size_t index = SizeClass::Index(size);
		_freeLists[index].Push(ptr);

		//����
		//����ǰ��������ĳ��� ���� �´�����Ľڵ�ĸ����� �ͻ���
		if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
		{
			ListTooLong(_freeLists[index], size);
		}

	}


	//��central cache����������ռ�
	void* FetchFromCentralCache(size_t index, size_t size)
	{

		//����ʼ���������㷨
		//1. �ʼ������central cache һ��Ҫ̫��
		//2. ���һֱ��size��С�ڴ����Ҫ����ôbatchNum�ͻ᲻������
		size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
		if (_freeLists[index].MaxSize() == batchNum)
		{
			_freeLists[index].MaxSize() += 1; //�޸���������ÿ������Ķ������
		}

		void* start = nullptr;
		void* end = nullptr;
		size_t actualNum = CentralCache::GetInStance()->FetchRangeObj(start, end, batchNum, size);
		assert(actualNum > 0);

		//ֻ��һ����ֱ�ӷ���
		if (actualNum == 1)
		{
			assert(start == end);
			return start;
		}
		//����һ������ʣ�µķŵ���������
		else
		{
			_freeLists[index].PushRange(ObjNext(start), end, actualNum - 1);
			return start;
		}
	}

	//���տռ佻��CentralCache
	void ListTooLong(FreeList& list, size_t size)
	{
		void* start = nullptr;
		void* end = nullptr;
		list.PopRange(start, end, list.MaxSize());

		CentralCache::GetInStance()->ReleaseListToSpans(start, size);
	}

private:

	FreeList _freeLists[NFREELISTS];//��������
};

//��̬ȫ�ֱ���
//��̬TLS ThreadCache �̶߳�ռ���߳���ȫ��
static _declspec(thread)ThreadCache* pTLSThreadCache = nullptr;
