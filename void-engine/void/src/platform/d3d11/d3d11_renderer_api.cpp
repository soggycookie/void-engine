#include "pch.h"
#include "d3d11_renderer_api.h"
#include "memory_system.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace VoidEngine
{
    struct ScreenVertex
    {
        float pos[4];   // x, y, z, w  (clip space)
        float uv[2];    // texture coords
    };

    D3D11_RendererAPI::D3D11_RendererAPI()
        : m_inputLayouts(MemorySystem::GeneralAllocator())
    {    
    }

    void D3D11_RendererAPI::NewFrame()
    {
        using namespace DirectX;
        XMVECTOR clearColor = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        m_context.deviceContext->ClearRenderTargetView(m_context.renderTargetView.Get(), reinterpret_cast<float*>(&clearColor));
        m_context.deviceContext->ClearDepthStencilView(m_context.depthStencilView.Get(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
    
        m_context.deviceContext->OMSetRenderTargets(
            1,
            m_context.renderTargetView.GetAddressOf(),
            m_context.depthStencilView.Get()
        );        
    }
    
    void D3D11_RendererAPI::EndFrame()
    {
        ASSERT_HR(m_context.swapchain->Present(1, 0));
    }

    //TODO: handle different adapters, output monitors, renderer res, output res, version fallback
    bool D3D11_RendererAPI::Init(int width, int height, void* outputWindow)
    {
        using Microsoft::WRL::ComPtr;

        HRESULT hr;
        UINT deviceFlag = 0;

#ifdef VOID_DEBUG
        //deviceFlag = D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL featureLevels[] = 
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            deviceFlag,
            featureLevels,
            _countof(featureLevels),
            D3D11_SDK_VERSION,
            m_context.device.GetAddressOf(),
            &m_context.featureLevel,
            m_context.deviceContext.GetAddressOf()
        );

        HR(hr);

        ComPtr<IDXGIDevice> dxgiDevice = nullptr;
        ComPtr<IDXGIAdapter> adapter = nullptr;
        ComPtr<IDXGIFactory2> factory = nullptr;

        HR(m_context.device.As(&dxgiDevice));
        HR(dxgiDevice->GetAdapter(&adapter));
        HR(adapter->GetParent(__uuidof(IDXGIFactory2), (void**) factory.GetAddressOf()));

        DXGI_SWAP_CHAIN_DESC1 scDesc = {};
        scDesc.Width = width;
        scDesc.Height = height;
        scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.BufferCount = 2;                  
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; 
        scDesc.Scaling = DXGI_SCALING_STRETCH;
        scDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        
        ComPtr<IDXGISwapChain1> swapChain1 = nullptr;

        HR(factory->CreateSwapChainForHwnd(
            m_context.device.Get(),
            static_cast<HWND>(outputWindow),
            &scDesc,
            nullptr,
            nullptr,
            swapChain1.GetAddressOf()
        ));

        swapChain1.As(&m_context.swapchain);

        //TODO: return more explicit error
        if(m_context.device == NULL)
        {
            return false;
        }

        if(m_context.deviceContext == NULL)
        {
            return false;
        }

        HR(hr);

        IDXGIOutput* output;
        m_context.swapchain->GetContainingOutput(&output);
        DXGI_OUTPUT_DESC outputDesc;
        DXGI_MODE_DESC modeDesc;
        output->GetDesc(&outputDesc);
        UINT numModes = 0;
        output->GetDisplayModeList(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            0,
            &numModes,     // Give me the count
            nullptr        // pDesc = NULL, don't fill anything yet
        );

        std::vector<DXGI_MODE_DESC> modes(numModes);  // Allocate array
        output->GetDisplayModeList(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            0,
            &numModes,     // Now it returns how many were ACTUALLY written
            modes.data()   // pDesc != NULL, fill this array please!
        );    
        //draw simple cube

        ComPtr<ID3D11Texture2D> backBuffer;
        hr = m_context.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(backBuffer.GetAddressOf()));

        HR(hr);
        
        hr = m_context.device->CreateRenderTargetView(backBuffer.Get(), NULL, &m_context.renderTargetView);

        HR(hr);

        D3D11_TEXTURE2D_DESC depthStencilBufferDesc;
        ZeroMemory(&depthStencilBufferDesc, sizeof(D3D11_TEXTURE2D_DESC));

        depthStencilBufferDesc.Width = width;
        depthStencilBufferDesc.Height = height;
        depthStencilBufferDesc.MipLevels = 1;
        depthStencilBufferDesc.ArraySize = 1;
        depthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthStencilBufferDesc.CPUAccessFlags = 0;
        depthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        depthStencilBufferDesc.SampleDesc.Count = 1;
        depthStencilBufferDesc.SampleDesc.Quality = 0;

        ComPtr<ID3D11Texture2D> depthStencilTexture;

        hr = m_context.device->
            CreateTexture2D(&depthStencilBufferDesc, NULL, &depthStencilTexture);

        HR(hr);

        hr = m_context.device->
            CreateDepthStencilView(depthStencilTexture.Get(), NULL, &m_context.depthStencilView);
        
        HR(hr);
        
        ZeroMemory(&m_context.viewport, sizeof(D3D11_VIEWPORT));

        m_context.viewport.Width = width;
        m_context.viewport.Height = height;
        m_context.viewport.TopLeftX = 0.0f;
        m_context.viewport.TopLeftY = 0.0f;
        m_context.viewport.MinDepth = 0.0f;
        m_context.viewport.MaxDepth = 1.0f;
        
        m_context.deviceContext->RSSetViewports(1, &m_context.viewport);

        //D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
        //ID3D11DepthStencilState* depthStencilState; 
        //
        //ZeroMemory(&depthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

        //depthStencilStateDesc.DepthEnable = true;
        //depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        //depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
        //depthStencilStateDesc.StencilEnable = false;

        //hr = m_context.device->
        //    CreateDepthStencilState(&depthStencilStateDesc, &depthStencilState);

        //D3D11_RASTERIZER_DESC rasterizerDesc;
        //ID3D11RasterizerState* rasterizerState;
        //ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

        //rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        //rasterizerDesc.CullMode = D3D11_CULL_BACK;
        //rasterizerDesc.FrontCounterClockwise = FALSE;
        //rasterizerDesc.DepthBias = 0;
        //rasterizerDesc.DepthBiasClamp = 0.0f;
        //rasterizerDesc.DepthClipEnable = true;
        //rasterizerDesc.SlopeScaledDepthBias = 0.0f;
        //rasterizerDesc.ScissorEnable = false;
        //rasterizerDesc.MultisampleEnable = false;
        //rasterizerDesc.AntialiasedLineEnable = false;

        //hr = m_context.device->
        //    CreateRasterizerState(&rasterizerDesc, &rasterizerState);
        //
        //HR(hr);

        return true;
    }

    void* D3D11_RendererAPI::CreateAndSubmitBuffer(void* const data, size_t byteSize, BufferType type)
    {
        D3D11_SUBRESOURCE_DATA subrsrc;
        ZeroMemory(&subrsrc, sizeof(D3D11_SUBRESOURCE_DATA));
        subrsrc.pSysMem = data;  

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = byteSize;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        ID3D11Buffer* buffer = nullptr;
        switch(type)
        {
            case BufferType::VERTEX_BUFFER:
            {
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                break;
            }
            case BufferType::INDEX_BUFFER:
            {
                desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                break;
            }
            case BufferType::CONSTANT_BUFFER:
            {
                //vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                break;
            }
            default:
            {
                assert(0 && "Unkown buffer type! [D3D11_RendererAPI]");
                return nullptr;
            }
        }

        ASSERT_HR(m_context.device->CreateBuffer(&desc, &subrsrc, &buffer));

        return buffer;
    }

    void D3D11_RendererAPI::DestroyBuffer(GraphicBuffer& buffer)
    {
        auto buf = buffer.As<ID3D11Buffer*>();
        
        if(buf)
        {
            buf->Release();
        }
    }

    void D3D11_RendererAPI::Clear()
    {
        m_context.swapchain.Reset();
        m_context.device.Reset();
        m_context.deviceContext.Reset();
        m_context.renderTargetView.Reset();
        m_context.depthStencilView.Reset();
    }

    void* D3D11_RendererAPI::CompileShader(const wchar_t* file, const char* entry, const char* target)
    {
        ID3DBlob* compiledShader = nullptr;
        ID3DBlob* error = nullptr;

        HRESULT hr = D3DCompileFromFile(
            file, nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry, target,
#ifdef VOID_DEBUG
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
#else
            0,
#endif
            0,
            &compiledShader,
            &error
        );

        if(error)
        {
            SIMPLE_LOG((char*) error->GetBufferPointer());
            error->Release();
        }

        return SUCCEEDED(hr) ? compiledShader : nullptr;
    }

    void* D3D11_RendererAPI::CreateShader(void** compiledSrc, ShaderType type)
    {
        ID3DBlob* src = static_cast<ID3DBlob*>(*compiledSrc);
        
        switch(type)
        {
            case ShaderType::VERTEX:
            {
                ID3D11VertexShader* vertexShader;

                m_context.device->CreateVertexShader(
                    src->GetBufferPointer(),
                    src->GetBufferSize(),
                    nullptr,
                    &vertexShader
                );   

                return vertexShader;
            }
            case ShaderType::PIXEL:
            {
                ID3D11PixelShader* pixelShader;

                m_context.device->CreatePixelShader(
                    src->GetBufferPointer(),
                    src->GetBufferSize(),
                    nullptr,
                    &pixelShader
                );        

                src->Release();
                *compiledSrc = nullptr;

                return pixelShader;            
            }
            default:
            {
                src->Release();
                *compiledSrc = nullptr;

                return nullptr;
            }
        }

        return nullptr;
    }

    void D3D11_RendererAPI::DestroyShader(GraphicShader& shader)
    {
        switch(shader.GetShaderType())
        {
            case ShaderType::VERTEX:
            {
                auto s = shader.As<ID3D11VertexShader*>();
        
                if(s)
                {
                    s->Release();
                }        
                break;
            }
            case ShaderType::PIXEL:
            {
                auto s = shader.As<ID3D11PixelShader*>();
        
                if(s)
                {
                    s->Release();
                }        
                break;            
            }
            default:
            {
                
                SIMPLE_LOG("[D3D11_RendererAPI] Destroy unknown shader type!");
                break;
            }
        }

        auto cs = shader.CompiledSrcAs<ID3DBlob*>();
        
        if(cs)
        {
            cs->Release();
        }
    }

    static const char* ConvertToD3D11Semantic(VertexSemantic semantic)
    {
        switch(semantic)
        {
            case VertexSemantic::POSITION:
                return "POSITION";
            case VertexSemantic::TEXCOORD:
                return "TEXCOORD";
            default:
                return nullptr;
        }

        return nullptr;
    }

    static DXGI_FORMAT ConvertToD3D11Format(TypeFormat format)
    {
        switch(format)
        {
            case TypeFormat::FORMAT_R32G32B32A32_FLOAT:
                return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case TypeFormat::FORMAT_R32G32B32_FLOAT:
                return DXGI_FORMAT_R32G32B32_FLOAT;
            case TypeFormat::FORMAT_R32G32_FLOAT:
                return DXGI_FORMAT_R32G32_FLOAT;
            case TypeFormat::FORMAT_R32_FLOAT:
                return DXGI_FORMAT_R32_FLOAT;

            case TypeFormat::FORMAT_R32G32B32A32_INT:
                return DXGI_FORMAT_R32G32B32A32_SINT;
            case TypeFormat::FORMAT_R32G32B32_INT:
                return DXGI_FORMAT_R32G32B32_SINT;
            case TypeFormat::FORMAT_R32G32_INT:
                return DXGI_FORMAT_R32G32_SINT;
            case TypeFormat::FORMAT_R32_INT:
                return DXGI_FORMAT_R32_SINT;

            case TypeFormat::FORMAT_R32G32B32A32_UINT:
                return DXGI_FORMAT_R32G32B32A32_UINT;
            case TypeFormat::FORMAT_R32G32B32_UINT:
                return DXGI_FORMAT_R32G32B32_UINT;
            case TypeFormat::FORMAT_R32G32_UINT:
                return DXGI_FORMAT_R32G32_UINT;
            case TypeFormat::FORMAT_R32_UINT:
                return DXGI_FORMAT_R32_UINT;
        }
    }

    ID3D11InputLayout* D3D11_RendererAPI::CreateInputLayout(const VertexDescriptor* vd, size_t count, ID3DBlob* compiledVertexSrc)
    {
        D3D11_INPUT_ELEMENT_DESC* inputDesc = static_cast<D3D11_INPUT_ELEMENT_DESC*>(MemorySystem::GeneralAllocator()->Alloc(count * sizeof(D3D11_INPUT_ELEMENT_DESC)));
        
        for(size_t i = 0; i < count;i++)
        {
            D3D11_INPUT_ELEMENT_DESC desc;
            desc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
            desc.InstanceDataStepRate = 0;
            desc.SemanticName = ConvertToD3D11Semantic(vd[i].semanticName);
            desc.SemanticIndex = vd[i].semanticIndex;
            desc.InputSlot = vd[i].inputSlot;
            desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            desc.Format = ConvertToD3D11Format(vd[i].format);

            inputDesc[i] = desc;
        }

        ID3D11InputLayout* inputLayout = nullptr;

        HRESULT hr = m_context.device->CreateInputLayout(
            inputDesc, count,
            compiledVertexSrc->GetBufferPointer(),
            compiledVertexSrc->GetBufferSize(),
            &inputLayout
        );
        
        MemorySystem::GeneralAllocator()->Free(inputDesc);

        return inputLayout;
    }

    void D3D11_RendererAPI::Draw(MeshResource* mesh, MaterialResource* material)
    {
        auto shader = material->GetShader();
        auto vertexShader = shader->GetVertexShader().As<ID3D11VertexShader*>();
        auto pixelShader = shader->GetPixelShader().As<ID3D11PixelShader*>();
        auto compiledVertexSrc = shader->GetVertexShader().CompiledSrcAs<ID3DBlob*>();

        InputLayoutKey key = {mesh->GetVertexDescHash(), material->GetGUID()};
        ID3D11InputLayout* layout = nullptr;
        if(m_inputLayouts.ContainsKey(key))
        {
            layout = m_inputLayouts[key];
        }
        else
        {
            layout = CreateInputLayout(mesh->GetVertexDesc(), mesh->GetVertexDescCount(), compiledVertexSrc);
            m_inputLayouts.Insert(key, layout);
        }

        //should never be here
        if(!layout)
        {
            assert(0 && "Layout is null!");
        }

        m_context.deviceContext->IASetInputLayout(layout);
        m_context.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        auto& vb =  mesh->GetVertexGraphicBuffer();
        auto& ib =  mesh->GetIndexGraphicBuffer();

        uint32_t stride = sizeof(Vertex);
        uint32_t offset = 0;

        auto vbPtr = vb.As<ID3D11Buffer*>();

        m_context.deviceContext->IASetVertexBuffers(0, 1, &vbPtr, &stride, &offset);
        m_context.deviceContext->IASetIndexBuffer(ib.As<ID3D11Buffer*>(), DXGI_FORMAT_R32_UINT, 0);
        

        m_context.deviceContext->VSSetShader(vertexShader, nullptr, 0);
        m_context.deviceContext->PSSetShader(pixelShader , nullptr, 0);

        m_context.deviceContext->DrawIndexed(mesh->GetIndexCount(), 0, 0);
    }
}

