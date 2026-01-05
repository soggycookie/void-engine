#pragma once
#include "void/pch.h"
#include "void/renderer_api.h"
#include "void/common_type.h"
#include "void/ds/flat_hash_map.h"

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#include <directxmath.h>
#include <DirectXPackedVector.h>

namespace VoidEngine
{

#define ASSERT_HR(hr) if(FAILED(hr)) { assert(0);}
#define HR(hr) if(FAILED(hr)) { return false; }
    template<typename T>
    inline void SafeRelease( T& ptr )
    {
        if ( ptr != NULL )
        {
            ptr->Release();
            ptr = NULL;
        }
    }

    struct D3D11_Context
    {
        IDXGISwapChain* swapchain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* deviceContext = nullptr;
        ID3D11RenderTargetView* renderTargetView = nullptr;
        ID3D11DepthStencilView* depthStencilView = nullptr;
        D3D_FEATURE_LEVEL featureLevel;
        D3D11_VIEWPORT viewport;
    };

    class D3D11_RendererAPI : public RendererAPI
    {
    public:
        struct InputLayoutKey
        {
            size_t vertexFormatHash;
            ResourceGUID shader;

            bool operator==(const InputLayoutKey& other) const
            {
                return (vertexFormatHash == other.vertexFormatHash)
                    && (shader == other.shader);
            }

            bool operator!=(const InputLayoutKey& other) const
            {
                return (vertexFormatHash != other.vertexFormatHash)
                    || (shader != other.shader);
            }
        };

    public:
        D3D11_RendererAPI();

        void Clear() override;
        bool Init(int width, int height, void* outputWindow) override;
        void Update() override;

        void NewFrame() override;

        void EndFrame() override;
    
        void* GetContext() override
        {
            return &m_context;
        }

        void* CreateAndSubmitBuffer(void* const data, size_t byteSize, BufferType type) override;

        void DestroyBuffer(GraphicBuffer& buffer) override;
        
        void* CompileShader(const wchar_t* file, const char* entry, const char* target) override;
        void* CreateShader(void* compiledSrc, ShaderType type) override;
        void DestroyShader(GraphicShader& shader) override;

        void Draw(MeshResource* mesh, MaterialResource* material) override;
    private:
        void SetUpDemo();

        
        ID3D11PixelShader* m_pixelShader;
        ID3D11VertexShader* m_vertexShader;
        ID3D11Buffer* m_boxVertexBuffer;
        ID3D11Buffer* m_boxIndexBuffer;
        ID3D11InputLayout* m_inputLayout;

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
            size_t h2 = key.vertexFormatHash;

            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}