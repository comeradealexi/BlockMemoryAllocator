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
		constexpr PoolSizeConstructor(size_t poolSize, size_t poolCount) : kPoolSize(poolSize), kPoolCount(poolCount), kBlockTotalSize(poolSize * poolCount)
		{

		}
		const size_t kPoolSize = 0;
		const size_t kPoolCount = 0;
		const size_t kBlockTotalSize = 0;
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
			//Size, Count
			{256, 1024},
			{512, 1024},
			{768, 1024},
			{1024, 1024},
			{1536, 1024},
			{1024 * 1024,		32},
			{1024 * 1024 * 2,	32},
			{1024 * 1024 * 3,	32},
			{1024 * 1024 * 4,	32},
			{1024 * 1024 * 5,	32},
			{1024 * 1024 * 6,	32},
			{1024 * 1024 * 7,	32},
			{1024 * 1024 * 8,	32},
			{1024 * 1024 * 9,	32},
			{1024 * 1024 * 10,	32},
			{1024 * 1024 * 12,	16},
			{1024 * 1024 * 24,	8},
			{1024 * 1024 * 32,	8},
			{1024 * 1024 * 36,	8},
			{1024 * 1024 * 42,	8},
			{1024 * 1024 * 48,	8},
			{1024 * 1024 * 52,	8},
			{1024 * 1024 * 56,	4},
			{1024 * 1024 * 60,	2},
			{1024 * 1024 * 64,	2},
			{1024 * 1024 * 68,	2},
			{1024 * 1024 * 72,	2},
		};
		static constexpr auto kArrayTotalSize = sizeof(kPoolSizes) / sizeof(kPoolSizes[0]);

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
		void DebugPrint(T& dbgPrint, bool bOnlyPrintActivePools)
		{
			dbgPrint << "Memory Allocator Info:" << "\n";
			m_firstPool.DebugPrint(1, dbgPrint, bOnlyPrintActivePools);

		}

	private:
		template<typename POOL_ALLOCATOR, size_t T_ARRAY_IDX, bool T_BOOL>
		struct PoolList
		{
			static constexpr auto kBlockSize = POOL_ALLOCATOR::kPoolSizes[T_ARRAY_IDX].kPoolSize;
			static constexpr auto kBlockCount = POOL_ALLOCATOR::kPoolSizes[T_ARRAY_IDX].kPoolCount;
			static constexpr auto kPoolSizeBytes = kBlockSize * kBlockCount;

			PoolList(T_ALLOCATOR& platformAllocator) : m_platformAllocator(platformAllocator), m_nextPool(platformAllocator)
			{

			}

			inline Memory Allocate(typename T_ALLOCATOR::Size memorySize, typename T_ALLOCATOR::Type memoryType)
			{
				if (memorySize <= kBlockSize)
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
							newMem->m_platformMemory = m_platformAllocator.Offset(pool->m_platformMemory, newMem->blockIdx * kBlockSize);
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
				newPool->m_platformMemory = m_platformAllocator.Allocate(kBlockSize * kBlockCount, POOL_ALLOCATOR::kAlignment);
				return newPool;
			}

			template<typename T>
			inline void DebugPrint(size_t poolNumber, T& dbgPrint, bool bOnlyPrintActivePools)
			{
				if (!bOnlyPrintActivePools || (bOnlyPrintActivePools && m_pools.size() > 0))
				{
					dbgPrint.precision(4);
						dbgPrint << "#" << poolNumber << "  ";
						dbgPrint << (size_t)kBlockSize;
						dbgPrint << "(" << static_cast<float>(kBlockSize) / 1024.0f / 1024.0f << "mb)";
						dbgPrint << "x";
						dbgPrint << (size_t)kBlockCount;
						dbgPrint << "=" << static_cast<size_t>(kBlockSize * kBlockCount);
						dbgPrint << "(" << static_cast<float>(kBlockSize * kBlockCount) / 1024.0f / 1024.0f << "mb)";
						dbgPrint << "\n";
						dbgPrint << "Pool Count:" << m_pools.size() << "\n";
				}
				m_nextPool.DebugPrint<T>(poolNumber + 1, dbgPrint, bOnlyPrintActivePools);
			}

			struct Pool : public PoolBase
			{
				Pool()
				{
					for (size_t i = 0; i < kBlockCount; i++)
						m_allocationList.push_back(i);					
				}

				std::array<typename T_ALLOCATOR::Type, kBlockCount> m_typeList = {};
				std::list<size_t> m_allocationList = {};
				typename T_ALLOCATOR::Memory m_platformMemory = T_ALLOCATOR::kMemoryDefault;

				virtual void Deallocate(size_t blockIdx) override
				{
					m_activeAllocationCount--;
					m_allocationList.push_back(blockIdx);
				}
				std::optional<size_t> Allocate(typename T_ALLOCATOR::Type memoryType)
				{
					if (m_activeAllocationCount == kBlockCount)
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

			static constexpr bool kLAST_VALID_POOL = (T_ARRAY_IDX + 1) < POOL_ALLOCATOR::kArrayTotalSize;

			PoolList<POOL_ALLOCATOR, T_ARRAY_IDX + 1, kLAST_VALID_POOL> m_nextPool;
		};

		template<typename POOL_ALLOCATOR, size_t T_ARRAY_IDX>
		struct PoolList<POOL_ALLOCATOR, T_ARRAY_IDX, false>
		{
			PoolList(T_ALLOCATOR& platformAllocator)
			{
			}

			template<typename T>
			inline void DebugPrint(size_t poolNumber, T& dbgPrint, bool bOnlyPrintActivePools)
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
		PoolList<T_ALLOCATOR, 0, true> m_firstPool;
		std::mutex			m_mutex;
	};
}

