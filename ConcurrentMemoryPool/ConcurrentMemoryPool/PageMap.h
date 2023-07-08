#pragma once

#include"Common.hpp"

//BITS �洢ҳ����Ҫ����λ

// ���������
template <int BITS>
class TCMalloc_PageMap1
{
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap1()
	{
		size_t size = sizeof(void*) << BITS; //��Ҫ��������Ĵ�С
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT); //��ҳ�����Ĵ�С
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT); //�������ҳ�ռ�
		memset(array_, 0, size); //�����뵽���ڴ��������
	}
	void* get(Number k) const
	{
		if ((k >> BITS) > 0) //k�ķ�Χ����[0, 2^BITS-1]
		{
			return NULL;
		}
		return array_[k]; //���ظ�ҳ�Ŷ�Ӧ��span
	}
	void set(Number k, void* v)
	{
		assert((k >> BITS) == 0); //k�ķ�Χ������[0, 2^BITS-1]
		array_[k] = v; //����ӳ��
	}
private:
	void** array_; //�洢ӳ���ϵ������
	static const int LENGTH = 1 << BITS; //ҳ����Ŀ
};

//���������
template <int BITS>
class TCMalloc_PageMap2
{
private:
	static const int ROOT_BITS = 5;                //��һ���Ӧҳ�ŵ�ǰ5������λ
	static const int ROOT_LENGTH = 1 << ROOT_BITS; //��һ��洢Ԫ�صĸ���
	static const int LEAF_BITS = BITS - ROOT_BITS; //�ڶ����Ӧҳ�ŵ��������λ
	static const int LEAF_LENGTH = 1 << LEAF_BITS; //�ڶ���洢Ԫ�صĸ���
	//��һ�������д洢��Ԫ������
	struct Leaf
	{
		void* values[LEAF_LENGTH];
	};
	Leaf* root_[ROOT_LENGTH]; //��һ������
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap2()
	{
		memset(root_, 0, sizeof(root_)); //����һ��Ŀռ��������
		PreallocateMoreMemory(); //ֱ�ӽ��ڶ���ȫ������
	}
	void* get(Number k) const
	{
		const Number i1 = k >> LEAF_BITS;        //��һ���Ӧ���±�
		const Number i2 = k & (LEAF_LENGTH - 1); //�ڶ����Ӧ���±�
		if ((k >> BITS) > 0 || root_[i1] == NULL) //ҳ��ֵ���ڷ�Χ��û�н�����ӳ��
		{
			return NULL;
		}
		return root_[i1]->values[i2]; //���ظ�ҳ�Ŷ�Ӧspan��ָ��
	}
	void set(Number k, void* v)
	{
		const Number i1 = k >> LEAF_BITS;        //��һ���Ӧ���±�
		const Number i2 = k & (LEAF_LENGTH - 1); //�ڶ����Ӧ���±�
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v; //������ҳ�����Ӧspan��ӳ��
	}
	//ȷ��ӳ��[start,start_n-1]ҳ�ŵĿռ��ǿ��ٺ��˵�
	bool Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1;)
		{
			const Number i1 = key >> LEAF_BITS;
			if (i1 >= ROOT_LENGTH) //ҳ�ų�����Χ
				return false;
			if (root_[i1] == NULL) //��һ��i1�±�ָ��Ŀռ�δ����
			{
				//���ٶ�Ӧ�ռ�
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = (Leaf*)leafPool.New();
				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //�����������
		}
		return true;
	}
	void PreallocateMoreMemory()
	{
		Ensure(0, 1 << BITS); //���ڶ���Ŀռ�ȫ�����ٺ�
	}
};

//���������
template <int BITS>
class TCMalloc_PageMap3
{
private:
	static const int INTERIOR_BITS = (BITS + 2) / 3;       //��һ�������Ӧҳ�ŵı���λ����
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS; //��һ������洢Ԫ�صĸ���
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS; //�������Ӧҳ�ŵı���λ����
	static const int LEAF_LENGTH = 1 << LEAF_BITS;         //������洢Ԫ�صĸ���
	struct Node
	{
		Node* ptrs[INTERIOR_LENGTH];
	};
	struct Leaf
	{
		void* values[LEAF_LENGTH];
	};
	Node* NewNode()
	{
		static ObjectPool<Node> nodePool;
		Node* result = nodePool.New();
		if (result != NULL)
		{
			memset(result, 0, sizeof(*result));
		}
		return result;
	}
	Node* root_;
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap3()
	{
		root_ = NewNode();
	}
	void* get(Number k) const
	{
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);         //��һ���Ӧ���±�
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //�ڶ����Ӧ���±�
		const Number i3 = k & (LEAF_LENGTH - 1);                    //�������Ӧ���±�
		//ҳ�ų�����Χ����ӳ���ҳ�ŵĿռ�δ����
		if ((k >> BITS) > 0 || root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL)
		{
			return NULL;
		}
		return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3]; //���ظ�ҳ�Ŷ�Ӧspan��ָ��
	}
	void set(Number k, void* v)
	{
		assert(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);         //��һ���Ӧ���±�
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //�ڶ����Ӧ���±�
		const Number i3 = k & (LEAF_LENGTH - 1);                    //�������Ӧ���±�
		Ensure(k, 1); //ȷ��ӳ���kҳҳ�ŵĿռ��ǿ��ٺ��˵�
		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v; //������ҳ�����Ӧspan��ӳ��
	}
	//ȷ��ӳ��[start,start+n-1]ҳ�ŵĿռ��ǿ��ٺ��˵�
	bool Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1;)
		{
			const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);         //��һ���Ӧ���±�
			const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //�ڶ����Ӧ���±�
			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH) //�±�ֵ������Χ
				return false;
			if (root_->ptrs[i1] == NULL) //��һ��i1�±�ָ��Ŀռ�δ����
			{
				//���ٶ�Ӧ�ռ�
				Node* n = NewNode();
				if (n == NULL) return false;
				root_->ptrs[i1] = n;
			}
			if (root_->ptrs[i1]->ptrs[i2] == NULL) //�ڶ���i2�±�ָ��Ŀռ�δ����
			{
				//���ٶ�Ӧ�ռ�
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = leafPool.New();
				if (leaf == NULL) return false;
				memset(leaf, 0, sizeof(*leaf));
				root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
			}
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //�����������
		}
		return true;
	}
	void PreallocateMoreMemory()
	{}
};