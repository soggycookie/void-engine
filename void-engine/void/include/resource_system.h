#pragma once
#include "resource_type_traits.h"
#include "resource_cache.h"
#include "renderer.h"

#include "allocator/free_list_allocator.h"
#include "allocator/pool_allocator.h"

namespace VoidEngine
{
    class ResourceSystem
    {
    public:
        
        static ResourceGUID GenerateGUID()
        {
            static size_t guid = 0;
            return guid++;
        }

        template<typename T, typename... Args>
        static T* Create(ResourceGUID guid, Args&&... args)
        {
            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "Type and template type mismatch! [ResourceSystem.Create]"
            );

            return ResourceCache::Create<T>(guid, 1, std::forward<Args>(args)...);  
        }

        template<typename T>
        static T* Acquire(const ResourceGUID& guid)
        {

            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "T is not a registered resource type [ResourceSystem.Acquire]"
            );

            auto resourceRef = ResourceCache::Acquire(guid);

            if(resourceRef.type == ResourceType::UNKNOWN)
            {
                return nullptr;
            }

            assert(
                ResourceTypeTraits<T>::type == resourceRef.type &&
                "Resource type and template type mismatch! [ResourceSystem.Acquire]"
            );

            return resourceRef.As<T>();   
        }

        template<typename T>        
        static void Release(ResourceGUID guid)
        {
            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "T is not a registered resource type [ResourceSystem.Release]"
            );

            ResourceCache::Release<T>(guid);
        }

        /// <summary>
        /// This will wipe the resource out of the table, even if there are others refering to it
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="guid"></param>

        template<typename T>
        static void Destroy(ResourceGUID guid)
        {

            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "T is not a registered resource type [ResourceSystem.Destroy]"
            );

            ResourceCache::Destroy(guid);
        }

        template<typename T>
        static T* Load(const std::wstring_view file)
        {
            size_t extPos = file.find_last_of('.');

            if(extPos == std::string_view::npos)
            {
                std::cerr << "[ResourceSystem] File does not have extension!" << std::endl;
                return nullptr;
            }

            extPos++;
            size_t extSize = file.length() - extPos;

            std::wstring_view extension = file.substr(extPos, extSize);

            if(extension == L"hlsl")
            {
                void* vertexCompiledSrc = Renderer::CompileShader(file.data(), "VSMain", "vs_5_0");
                void* pixelCompiledSrc = Renderer::CompileShader(file.data(), "PSMain", "ps_5_0");
            
                if(!vertexCompiledSrc || !pixelCompiledSrc)
                {
                    std::wcerr << "[ResourceSystem] Failed to load shader! Asset: ";
                    std::wcerr.write(file.data(), file.size()) << std::endl;
                }
                else
                {
                    std::wcout << "[ResourceSystem] Load shader successfully! Asset: ";
                    std::wcout.write(file.data(), file.size()) << std::endl;

                    ShaderResource* shader = ResourceCache::Create<ShaderResource>(GenerateGUID(), 1);
                    shader->SetVertexShaderCompiledSrc(vertexCompiledSrc);
                    shader->SetPixelShaderCompiledSrc(pixelCompiledSrc);
                    shader->SubmitShaderToGpu();

                    return shader;
                }
            }
            else
            {
                SIMPLE_LOG("[ResourceSystem] Extension type is unknown or not supported!");
            }

            return nullptr;
        }

#ifdef VOID_DEBUG
        static int32_t InspectRef(ResourceGUID guid);
#endif

        static void LoadBundle(const std::wstring_view file);
    private:
        friend class Application;

        static void StartUp(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAlloc);
        static void ShutDown();


    };
}