#pragma once
#include <cstdlib>
#include <vector>
#include <mutex>
#include <memory>
#include <array>
#include <optional>
#include <list>
#include <type_traits>
#include <typeinfo>

namespace Templated
{
	struct PoolSizeConstructor
	{
		constexpr PoolSizeConstructor(size_t poolSize, size_t poolCount) : kPoolSize(poolSize), kPoolCount(poolCount)
		{

		}
		const size_t kPoolSize = 0;
		const size_t kPoolCount = 0;
	};
	struct CPPAllocator
	{
	public:
		enum class Type
		{
			Array,
			Class,
			Other
		};
		using Size = std::size_t;
		using Memory = void*;
		static constexpr Memory kMemoryDefault = nullptr;
		static constexpr Size kBlockCountSmallestAllocation = 1024;
		static constexpr Size kMinAllocationSizeBytes = 256;
		static constexpr Size kAlignment = 256;

		static constexpr Size kMaxAllocationSize = 1024 * 1024 * 128;
		static constexpr Size kMaxAllocationCount = 1;

		//NOTE TO SELF USE SELF DEFINED ARRAYS
		static constexpr PoolSizeConstructor kPoolSizes[] = 
		{ 
			{1024*1024		,32},
			{1024 * 1024 * 2,32},


		};
	public:
		Memory Allocate(Size memorySize, Size memoryAlignment)
		{
			return malloc(memorySize);
		}
		inline Memory Offset(Memory memoryIn, Size blockSize)
		{
			return ((char*)memoryIn) + blockSize;
		}
		void Free(Memory pMemory)
		{
			free(pMemory);
		}
	};

	template<typename T_ALLOCATOR>
	class MemoryAllocator
	{
	public:
		struct PoolBase
		{
			virtual void Deallocate(size_t blockIdx) = 0;
		};

		struct LocalAllocation
		{
		public:
			typename T_ALLOCATOR::Memory m_platformMemory = T_ALLOCATOR::kMemoryDefault;

			~LocalAllocation()
			{
				if (m_poolAllocatedFrom)
					m_poolAllocatedFrom->Deallocate(blockIdx);
			}
			size_t blockIdx = ~0;
			std::shared_ptr<PoolBase> m_poolAllocatedFrom;
		};
		using Memory = std::shared_ptr<LocalAllocation>;
		
		MemoryAllocator(T_ALLOCATOR& platformAllocator) : m_allocator(platformAllocator), m_firstPool(platformAllocator) {	}
		~MemoryAllocator() { }

		Memory Allocate(typename T_ALLOCATOR::Size memorySize, typename T_ALLOCATOR::Type memoryType)
		{
			return m_firstPool.Allocate(memorySize, memoryType);
		}

		template<typename T>
		void DebugPrint(T& dbgPrint)
		{
			dbgPrint << "Memory Allocator Info:" << "\n";
			m_firstPool.DebugPrint(1, dbgPrint);

		}

	private:
		template<typename POOL_ALLOCATOR, typename T_ALLOCATOR::Size T_BLOCK_SIZE, typename T_ALLOCATOR::Size T_BLOCK_COUNT, bool T_BOOL>
		struct PoolList
		{
			static constexpr auto kPoolSizeBytes = T_BLOCK_SIZE * T_BLOCK_COUNT;
			static constexpr auto kBlockSize = T_BLOCK_SIZE;
			static constexpr auto kBlockCount = T_BLOCK_COUNT;
			PoolList(T_ALLOCATOR& platformAllocator) : m_platformAllocator(platformAllocator), m_nextPool(platformAllocator)
			{

			}

			inline Memory Allocate(typename T_ALLOCATOR::Size memorySize, typename T_ALLOCATOR::Type memoryType)
			{
				if (memorySize <= T_BLOCK_SIZE)
				{
					Memory newMem = std::make_shared<LocalAllocation>();

					if (m_pools.size() == 0)
						AddNewPool();

					for (auto& pool : m_pools)
					{
						auto allocation = pool->Allocate(memoryType);
						if (allocation)
						{
							newMem->blockIdx = *allocation;
							newMem->m_poolAllocatedFrom = std::static_pointer_cast<PoolBase>(pool);
							newMem->m_platformMemory = m_platformAllocator.Offset(pool->m_platformMemory, newMem->blockIdx * T_BLOCK_SIZE);
							return newMem;
						}
					}

					AddNewPool();
					newMem->blockIdx = m_pools.back()->Allocate(memoryType).value_or(~0);

					return newMem;
				}
				else
				{
					return m_nextPool.Allocate(memorySize, memoryType);
				}
			}

			inline auto AddNewPool()
			{
				m_pools.push_back(std::make_shared<Pool>());
				auto& newPool = m_pools.back();
				newPool->m_platformMemory = m_platformAllocator.Allocate(T_BLOCK_SIZE * T_BLOCK_COUNT, POOL_ALLOCATOR::kAlignment);
				return newPool;
			}

			template<typename T>
			inline void DebugPrint(size_t poolNumber, T& dbgPrint)
			{
				dbgPrint.precision(4);
				dbgPrint << "#" << poolNumber << "  ";
				dbgPrint << (size_t)T_BLOCK_SIZE;
				dbgPrint << "(" << static_cast<float>(T_BLOCK_SIZE) / 1024.0f / 1024.0f << "mb)";
				dbgPrint << "x";
				dbgPrint << (size_t)T_BLOCK_COUNT;
				dbgPrint << "=" << static_cast<size_t>(T_BLOCK_SIZE * T_BLOCK_COUNT);
				dbgPrint << "(" << static_cast<float>(T_BLOCK_SIZE * T_BLOCK_COUNT) / 1024.0f / 1024.0f << "mb)";
				dbgPrint << "\n";

				dbgPrint << "Pool Count:" << m_pools.size() << "\n";
				m_nextPool.DebugPrint<T>(poolNumber + 1, dbgPrint);
			}

			struct Pool : public PoolBase
			{
				Pool()
				{
					for (size_t i = 0; i < T_BLOCK_COUNT; i++)
						m_allocationList.push_back(i);					
				}

				std::array<typename T_ALLOCATOR::Type, T_BLOCK_COUNT> m_typeList = {};
				std::list<size_t> m_allocationList = {};
				typename T_ALLOCATOR::Memory m_platformMemory = T_ALLOCATOR::kMemoryDefault;

				virtual void Deallocate(size_t blockIdx) override
				{
					m_activeAllocationCount--;
					m_allocationList.push_back(blockIdx);
				}
				std::optional<size_t> Allocate(typename T_ALLOCATOR::Type memoryType)
				{
					if (m_activeAllocationCount == T_BLOCK_COUNT)
						return {};

					if (m_allocationList.size() == 0)
						return {};

					auto front = m_allocationList.front();
					m_typeList[front] = memoryType;
					m_activeAllocationCount++;
					m_allocationList.pop_front();
					return front;
				}
			private:
				size_t m_activeAllocationCount = 0;
			};

			std::vector<std::shared_ptr<Pool>> m_pools;
			T_ALLOCATOR& m_platformAllocator;

			static constexpr size_t kNEXT_BLOCK_SIZE = T_BLOCK_SIZE / 2;
			static constexpr size_t kNEXT_BLOCK_COUNT = (T_BLOCK_COUNT * 2);
			static constexpr bool kLAST_VALID_POOL = kNEXT_BLOCK_SIZE > POOL_ALLOCATOR::kMinAllocationSizeBytes;

			PoolList<POOL_ALLOCATOR, kNEXT_BLOCK_SIZE, kNEXT_BLOCK_COUNT, kLAST_VALID_POOL> m_nextPool;
		};

		template<typename POOL_ALLOCATOR, typename T_ALLOCATOR::Size T_BLOCK_SIZE, typename T_ALLOCATOR::Size T_BLOCK_COUNT>
		struct PoolList<POOL_ALLOCATOR, T_BLOCK_SIZE, T_BLOCK_COUNT, false>
		{
			PoolList(T_ALLOCATOR& platformAllocator)
			{
			}

			template<typename T>
			inline void DebugPrint(size_t poolNumber, T& dbgPrint)
			{
			}

			Memory Allocate(typename T_ALLOCATOR::Size /*memorySize*/, typename POOL_ALLOCATOR::Type /*memoryType*/)
			{
				//Error, allocation too large.
				return std::make_shared<LocalAllocation>();
			}
		};

		//Specialised Pool to prevent infinite recursive template creation
		//template<>
		//struct PoolList<T_ALLOCATOR, 0, 0>
		//{
		//
		//};
		
		T_ALLOCATOR&		m_allocator;
		PoolList<T_ALLOCATOR, T_ALLOCATOR::kMaxAllocationSize, T_ALLOCATOR::kMaxAllocationCount, true> m_firstPool;
		std::mutex			m_mutex;
	};
}

