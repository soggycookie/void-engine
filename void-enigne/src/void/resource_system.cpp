#include "resource_system.h"
#include "resource_cache.h"
#include "resource.h"
#include "renderer.h"

namespace VoidEngine
{


    void ResourceSystem::StartUp(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAlloc, FreeListAllocator* streamAlloc)
    {
        if(!resourceLookUpAlloc || !resourceAlloc || !streamAlloc)
        {
            assert(0 && "Allocators can not be null! [ResourceSystem]");
            return;
        }

        //ResourceStream::Init(streamAlloc);
        ResourceCache::Init(resourceLookUpAlloc, resourceAlloc);
    }

    void ResourceSystem::ShutDown()
    {
        ResourceCache::DestroyAll();
    }

    void ResourceSystem::Load(const std::wstring_view file)
    {
        size_t extPos = file.find_last_of('.');

        if(extPos == std::string_view::npos)
        {
            std::cerr << "[ResourceSystem] File does not have extension!" << std::endl;
            return;
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

                ShaderResource* shader = Create<ShaderResource>(GenerateGUID());
                shader->SetVertexShaderCompiledSrc(vertexCompiledSrc);
                shader->SetPixelShaderCompiledSrc(pixelCompiledSrc);
                shader->SubmitShaderToGpu();
            }
        }
        else
        {
            SIMPLE_LOG("[ResourceSystem] Extension type is unknown or not supported!");
        }
    }
}
