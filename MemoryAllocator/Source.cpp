#include "MemoryAllocator.h"
#include <iostream>
int main()
{
	Templated::CPPAllocator cppAllocator;
	Templated::MemoryAllocator<Templated::CPPAllocator> memoryPools(cppAllocator);
	memoryPools.DebugPrint(std::cout);
	{
		auto mem1 = memoryPools.Allocate(1024, Templated::CPPAllocator::Type::Other);
		auto mem2 = memoryPools.Allocate(1024, Templated::CPPAllocator::Type::Other);
	}
	return 0;
}