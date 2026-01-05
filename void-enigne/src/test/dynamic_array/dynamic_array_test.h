#pragma once
#include "void/ds/dynamic_array.h"

void inline TestArray()
{
    using namespace VoidEngine;

    FreeListAllocator al(KB(1));

    DynamicArray<size_t> firstArr(&al, 4);

    std::cout << "Capacity" << std::endl;
    std::cout << firstArr.GetCapacity() << std::endl;

    firstArr.PushBack(10);
    firstArr.EmplaceBack(20);
    auto it = firstArr.Find(10);
    firstArr.Emplace(it, 40);

    it = firstArr.Find(20);
    firstArr.Push(it ,65);
    firstArr.PushBack(284);

    for(it = firstArr.Begin(); it != firstArr.End(); it++)
    {
        std::cout << *it << std::endl;
    }
    std::cout << "Capacity" << std::endl;
    std::cout << firstArr.GetCapacity() << std::endl;



}
