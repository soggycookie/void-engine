#include "void/pch.h"
#include "d3d11_renderer_api.h"
#include "void/global_persistant_allocator.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace VoidEngine
{
    struct ScreenVertex
    {
        float pos[4];   // x, y, z, w  (clip space)
        float uv[2];    // texture coords
    };

    void D3D11_RendererAPI::NewFrame()
    {
        using namespace DirectX;
        XMVECTOR clearColor = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
        m_context.deviceContext->ClearRenderTargetView(m_context.renderTargetView, reinterpret_cast<float*>(&clearColor));
        m_context.deviceContext->ClearDepthStencilView(m_context.depthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
    
    void D3D11_RendererAPI::EndFrame()
    {
        ASSERT_HR(m_context.swapchain->Present(0, 0));
    }

    //TODO: handle different adapters, output monitors, renderer res, output res, version fallback
    bool D3D11_RendererAPI::Init(int width, int height, void* outputWindow)
    {
        
        HRESULT hr;
        //IDXGIFactory* factory;
        //
        //HRESULT  hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);

        //ASSERT_HR(hr);

        //IDXGIAdapter* adapter;
        //IDXGIAdapter* usedAdapter;
        //std::vector<IDXGIAdapter*> adapters;
        //int i = 0;
        ////handle multiple adapters and resolutions (monitors if possible)
        //while (factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        //    adapters.push_back(adapter);
        //    DXGI_ADAPTER_DESC desc;
        //    adapter->GetDesc(&desc);

        //    IDXGIOutput * pOutput;
        //    std::vector<IDXGIOutput*> vOutputs;
        //    int j = 0;
        //    while(adapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND)
        //    {
        //        vOutputs.push_back(pOutput);
        //        ++j;
        //        DXGI_OUTPUT_DESC od;
        //        pOutput->GetDesc(&od);
        //        std::cout << "Attach to Desktop: " << od.AttachedToDesktop  << std::endl;

        //    }

        //    std::cout << "Number of adapters output: " << vOutputs.size() << std::endl;

        //    std::wcout << desc.Description << std::endl;
        //    ++i;
        //}

        //usedAdapter = adapters[1];

        //DXGI_ADAPTER_DESC desc;
        //usedAdapter->GetDesc(&desc);

        //std::wcout << desc.Description << std::endl;
        //IDXGIOutput* pOutput;
        //std::vector<IDXGIOutput*> vOutputs;
        //
        //int j = 0;
        //while(usedAdapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND)
        //{
        //    vOutputs.push_back(pOutput);
        //    ++j;
        //}
        //std::cout << "Number of adapters output: " << vOutputs.size() << std::endl;

        //this is the setup for Windowed, not fullscreen
        DXGI_SWAP_CHAIN_DESC swapchainDesc;
        ZeroMemory(&swapchainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

        swapchainDesc.BufferCount = 1;
        swapchainDesc.BufferDesc.Width = width;
        swapchainDesc.BufferDesc.Height = height;
        swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapchainDesc.BufferDesc.RefreshRate = {144, 1};
        swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchainDesc.OutputWindow = static_cast<HWND>(outputWindow);
        //default sampler mode
        swapchainDesc.SampleDesc.Count = 1;
        swapchainDesc.SampleDesc.Quality = 0;
        swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapchainDesc.Windowed = TRUE;

        UINT deviceFlag = 0;

#ifdef VOID_DEBUG
        deviceFlag = D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL featureLevels[] = 
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        hr = 
            D3D11CreateDeviceAndSwapChain(
                NULL,
                D3D_DRIVER_TYPE_HARDWARE,
                NULL,
                deviceFlag,
                featureLevels,
                _countof(featureLevels),
                D3D11_SDK_VERSION,
                &swapchainDesc,
                &m_context.swapchain,
                &m_context.device,
                &m_context.featureLevel,
                &m_context.deviceContext
            );

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

        ID3D11Texture2D* backBuffer;
        hr = m_context.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&backBuffer));

        HR(hr);

        ID3D11RenderTargetView* renderTargetView;
        
        hr = m_context.device->CreateRenderTargetView(backBuffer, NULL, &m_context.renderTargetView);

        HR(hr);

        SafeRelease(backBuffer);

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

        ID3D11Texture2D* depthStencilTexture;

        hr = m_context.device->
            CreateTexture2D(&depthStencilBufferDesc, NULL, &depthStencilTexture);

        HR(hr);

        hr = m_context.device->
            CreateDepthStencilView(depthStencilTexture, NULL, &m_context.depthStencilView);
        
        HR(hr);

        m_context.deviceContext->OMSetRenderTargets(1, &m_context.renderTargetView , m_context.depthStencilView);
        
        ZeroMemory(&m_context.viewport, sizeof(D3D11_VIEWPORT));

        m_context.viewport.Width = width;
        m_context.viewport.Height = height;
        m_context.viewport.TopLeftX = 0.0f;
        m_context.viewport.TopLeftY = 0.0f;
        m_context.viewport.MinDepth = 0.0f;
        m_context.viewport.MaxDepth = 1.0f;
        
        m_context.deviceContext->RSSetViewports(1, &m_context.viewport);

        D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
        ID3D11DepthStencilState* depthStencilState; 
        
        ZeroMemory(&depthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

        depthStencilStateDesc.DepthEnable = true;
        depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
        depthStencilStateDesc.StencilEnable = false;

        hr = m_context.device->
            CreateDepthStencilState(&depthStencilStateDesc, &depthStencilState);

        D3D11_RASTERIZER_DESC rasterizerDesc;
        ID3D11RasterizerState* rasterizerState;
        ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_BACK;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias = 0;
        rasterizerDesc.DepthBiasClamp = 0.0f;
        rasterizerDesc.DepthClipEnable = true;
        rasterizerDesc.SlopeScaledDepthBias = 0.0f;
        rasterizerDesc.ScissorEnable = false;
        rasterizerDesc.MultisampleEnable = false;
        rasterizerDesc.AntialiasedLineEnable = false;

        hr = m_context.device->
            CreateRasterizerState(&rasterizerDesc, &rasterizerState);
        
        HR(hr);

        //SafeRelease(factory);
        //
        //for (auto adapter : adapters) {
        //    SafeRelease(adapter);
        //}
        //m_property.device->CreateInputLayout(vertexLayout1, 1, , , &inputLayout);

        SetUpDemo();

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

    void D3D11_RendererAPI::SetUpDemo()
    {
        ScreenVertex quadVertices[] =
        {
            //   position (x, y, z, w)       uv
            { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }, // top-left
            { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } }, // top-right
            { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } }, // bottom-right
            { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } }  // bottom-left
        };

        uint16_t quadIndices[] =
        {
            0, 1, 2,
            0, 2, 3
        };

        D3D11_BUFFER_DESC vbDesc;
        vbDesc.ByteWidth = sizeof(quadVertices);
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = 0;
        vbDesc.MiscFlags = 0;
        vbDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA vbRsc;
        ZeroMemory(&vbRsc, sizeof(D3D11_SUBRESOURCE_DATA));
        vbRsc.pSysMem = quadVertices;

        ASSERT_HR(m_context.device->CreateBuffer(&vbDesc, &vbRsc, &m_boxVertexBuffer));
        D3D11_BUFFER_DESC ibDesc;
        ibDesc.ByteWidth = sizeof(quadIndices);
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = 0;
        ibDesc.MiscFlags = 0;
        ibDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA ibRsc;
        ZeroMemory(&ibRsc, sizeof(D3D11_SUBRESOURCE_DATA));
        ibRsc.pSysMem = quadIndices;

        ASSERT_HR(m_context.device->CreateBuffer(&ibDesc, &ibRsc, &m_boxIndexBuffer));

        const wchar_t* shaderFilePath = L"src//asset//shader//platform//d3d11//square_demo.hlsl"; 
        ID3DBlob* compiledVertex = static_cast<ID3DBlob*>(CompileShader(shaderFilePath, "VSMain", "vs_5_0"));
        ID3DBlob* compiledPixel = static_cast<ID3DBlob*>(CompileShader(shaderFilePath, "PSMain", "ps_5_0"));

        m_context.device->CreateVertexShader(
            compiledVertex->GetBufferPointer(),
            compiledVertex->GetBufferSize(),
            nullptr,
            &m_vertexShader
        );
        
        m_context.device->CreatePixelShader(
            compiledPixel->GetBufferPointer(),
            compiledPixel->GetBufferSize(),
            nullptr,
            &m_pixelShader
        );

        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0 ,D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        ASSERT_HR(m_context.device->CreateInputLayout(
            layout, 2, 
            compiledVertex->GetBufferPointer(), 
            compiledVertex->GetBufferSize(), 
            &m_inputLayout
        ));
       
        compiledVertex->Release();
        compiledPixel->Release();
    }

    void D3D11_RendererAPI::Update()
    {
        NewFrame();
        
        m_context.deviceContext->IASetInputLayout(m_inputLayout);
        m_context.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        UINT stride = sizeof(float) * 6;
        UINT offset = 0;
        m_context.deviceContext->IASetVertexBuffers(0, 1, &m_boxVertexBuffer, &stride, &offset);
        m_context.deviceContext->IASetIndexBuffer(m_boxIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

        m_context.deviceContext->VSSetShader(m_vertexShader, nullptr, 0);
        m_context.deviceContext->PSSetShader(m_pixelShader , nullptr, 0);

        m_context.deviceContext->DrawIndexed(6, 0, 0);

        EndFrame();
    }

    void D3D11_RendererAPI::Clear()
    {

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

        if (error)
        {
            SIMPLE_LOG((char*)error->GetBufferPointer());
            error->Release();
        }

        return SUCCEEDED(hr) ? compiledShader : nullptr;
     }

    void* D3D11_RendererAPI::CreateShader(void* compiledSrc, ShaderType type)
    {
        ID3DBlob* src = static_cast<ID3DBlob*>(compiledSrc);

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
                
                return pixelShader;            
            }
            default:
            {
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
}

