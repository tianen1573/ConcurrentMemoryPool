#pragma once

#include<iostream>
#include<vector>
#include<time.h>

#include"Common.hpp"



template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* ptr = nullptr;

		//����ʹ����������Ŀռ�
		if (_freeListPtr)
		{
			void* next = *(void**)_freeListPtr;//�����������һ���ڵ��ָ����ڵ�ǰ�ڵ��ǰ�棬���ַ�ʽ����ȡ��ǰָ���С���ڴ�
			ptr = (T*)_freeListPtr;
			_freeListPtr = next;
		}
		else
		{
			//ʣ���ڴ治��һ��ʹ�ã����¿��ٴ���ڴ�
			if (_remainBytes < sizeof(T))
			{
				//_remainBytes = 128 * 1024;//�ֽ���
				//_memoryPtr = (char*)malloc(_remainBytes);

				//�ֽ��� -- ��Ҫ���� �����С ����
				//���Կ����ڹ��캯��ʱ��ָ���������
				_remainBytes = 128 * 1024;
				_memoryPtr = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);

				//����ҳ�ռ�
				if (_memoryPtr == nullptr)
				{
					throw std::bad_alloc();//���쳣
				}
			}

			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);//������ڴ����ٿ��Դ洢ָ��
			ptr = (T*)_memoryPtr;
			_memoryPtr += objSize;
			_remainBytes -= objSize;
		}

		// ���Ǹ����󡰷��䡱�����Ѿ����ڵ��ڴ棬��Ҫʹ�ö�λnew��ʼ��
		// ��ʽ���ù��캯����ʼ�� -- ��λnew
		new(ptr)T;
		//ptr->T();

		return ptr;
	}
	void Delete(T* ptr)
	{
		// ��ʾ������������������󣬲��ͷ��ڴ�
		ptr->~T();
		//delete(ptr)T;//���ͷ��ڴ�

		// ͷ��
		*(void**)ptr = _freeListPtr;
		_freeListPtr = ptr;
	}

private:
	char* _memoryPtr = nullptr;// ָ�����ڴ��ָ��
	size_t _remainBytes = 0;// ��ǰ����ڴ��ʣ���ֽ���
	void* _freeListPtr = nullptr;//�ͷŵ���������ͷָ��
};


//struct TreeNode
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};
//
//void TestPool()
//{
//	// �����ͷŵ��ִ�
//	const size_t Rounds = 5;
//
//	// ÿ�������ͷŶ��ٴ�
//	const size_t N = 100000;
//
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//
//	size_t begin1 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new TreeNode);
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i];
//		}
//		v1.clear();
//	}
//
//	size_t end1 = clock();
//
//
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//
//	std::cout << "new cost time:" << end1 - begin1 << std::endl;
//	std::cout << "object pool cost time:" << end2 - begin2 << std::endl;
//
//}