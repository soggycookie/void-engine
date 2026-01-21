#pragma once
#include "void/pch.h"
#include "void/ds/flat_hash_map.h"
#include "void/allocator/free_list_allocator.h"

void inline TestMap()
{
    using namespace VoidEngine;

    FreeListAllocator al(KB(1));
    FlatHashMap<std::string, size_t> map(&al, 4);

    std::cout << "Bucket Count" << std::endl;
    std::cout << map.GetBucketCount() << std::endl;
    std::cout << map.GetCount() << std::endl;
    //map["gggs"] = 2233;
    //map["domixi"] = 968;
    //map["ashd"] = 2;
    //map["22"] = 82;
    //map["mina"] = 82;
    map.Insert("gggs", 2233);
    map.Insert("domixi", 968);
    map.Insert("ashd", 2);
    map.Insert("22", 82);
    map.Insert("mina", 232);

    std::cout << "Bucket Count" << std::endl;
    std::cout << map.GetBucketCount() << std::endl;
    std::cout << map.GetCount() << std::endl;
    for(auto it = map.Begin(); it != map.End(); it++)
    {
        if(it.IsValid())
        {
            std::cout << it.GetKey() << std::endl;
            std::cout << it.GetValue() << std::endl;
        }
    }
    std::cout << "Map Contains" << std::endl;
    std::cout << map.ContainsKey("domixi") << std::endl;
    std::cout << map.ContainsKey("gggs") << std::endl;
    std::cout << map.ContainsKey("ashd") << std::endl;
    std::cout << map.ContainsKey("22") << std::endl;
    std::cout << map.ContainsKey("mina") << std::endl;
    std::cout << map.ContainsKey("hello") << std::endl;
    map.Remove("gggs");
    std::cout << "Map Remove" << std::endl;
    std::cout << map.ContainsKey("gggs") << std::endl;
    
}
