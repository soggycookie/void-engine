#pragma once
#include "pch.h"
#include "renderer_api.h"
#include "common_type.h"
#include "ds/flat_hash_map.h"

#include <d3d11_2.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

#include <directxmath.h>
#include <wrl/client.h>

namespace VoidEngine
{

#define ASSERT_HR(hr) if(FAILED(hr)) { assert(0);}
#define HR(hr) if(FAILED(hr)) { return false; }

    class D3D11_RendererAPI : public RendererAPI
    {
    public:
        struct D3D11_Context
        {
            
            Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain = nullptr;
            Microsoft::WRL::ComPtr<ID3D11Device> device = nullptr;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext = nullptr;
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView = nullptr;
            Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView = nullptr;
            D3D_FEATURE_LEVEL featureLevel;
            D3D11_VIEWPORT viewport;
        };

        struct InputLayoutKey
        {
            size_t vertexDescHash;
            ResourceGUID shader;

            bool operator==(const InputLayoutKey& other) const
            {
                return (vertexDescHash == other.vertexDescHash)
                    && (shader == other.shader);
            }

            bool operator!=(const InputLayoutKey& other) const
            {
                return (vertexDescHash != other.vertexDescHash)
                    || (shader != other.shader);
            }
        };

    public:
        D3D11_RendererAPI();

        void Clear() override;
        bool Init(int width, int height, void* outputWindow) override;

        void NewFrame() override;

        void EndFrame() override;
    
        void* GetContext() override
        {
            return &m_context;
        }

        void* CreateAndSubmitBuffer(void* const data, size_t byteSize, BufferType type) override;

        void DestroyBuffer(GraphicBuffer& buffer) override;
        
        void* CompileShader(const wchar_t* file, const char* entry, const char* target) override;
        void* CreateShader(void** compiledSrc, ShaderType type) override;
        void DestroyShader(GraphicShader& shader) override;

        void Draw(MeshResource* mesh, MaterialResource* material) override;
    private:

        ID3D11InputLayout* CreateInputLayout(const VertexDescriptor* vd, size_t count, ID3DBlob* compiledVertexSrc);
    
    private:
        D3D11_Context m_context;
        FlatHashMap<InputLayoutKey, ID3D11InputLayout*> m_inputLayouts;
    };
}

namespace std {
    template<>
    struct hash<VoidEngine::D3D11_RendererAPI::InputLayoutKey> {
        size_t operator()(const VoidEngine::D3D11_RendererAPI::InputLayoutKey& key) const
        {
            size_t h1 = hash<size_t>{}(key.shader);
            size_t h2 = key.vertexDescHash;

            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}