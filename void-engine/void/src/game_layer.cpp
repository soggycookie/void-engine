#include "game_layer.h"
#include "renderer.h"
#include "resource_system.h"
#include "profiler.h"

namespace VoidEngine
{
    void GameLayer::OnAttach()
    {
        SIMPLE_LOG("attach!");
    }

    void GameLayer::OnUpdate(double dt)
    {     
        Renderer::NewFrame();
        Renderer::Draw(mesh, material);
        Renderer::EndFrame();
    }

    void GameLayer::OnDetach()
    {
        SIMPLE_LOG("detach!");
    }

    void GameLayer::OnInit()
    {
        Vertex quadVertices[] =
        {
            //   position (x, y, z, w)       uv
            { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }, // top-left
            { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } }, // top-right
            { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } }, // bottom-right
            { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } }  // bottom-left
        };

        uint32_t quadIndices[] =
        {
            0, 1, 2,
            0, 2, 3
        };

        mesh = ResourceSystem::Create<MeshResource>(123, false);
        mesh->SetVertexData(quadVertices, 4);
        mesh->SetIndexData(quadIndices, 6);
        mesh->SubmitMeshToGpu();

        auto shader = ResourceSystem::Load<ShaderResource>(L"asset//shader//square_demo.hlsl");
        material = ResourceSystem::Create<MaterialResource>(ResourceSystem::GenerateGUID(), shader->GetGUID());

        //std::cout << "Entity id: " << e.GetId() << " , gen count: " << e.GetGenCount() << std::endl;
        
        world = ECS::CreateWorld();
        world->RegisterComponent<Position>();
        world->RegisterComponent<Velocity>();

        ECS::Entity e = world->CreateEntity(1, "The first");
        ECS::Entity e1 = world->CreateEntity(1, "The Second");
        ECS::Entity e2 = world->CreateEntity("The third");

        e.AddComponent<Position>();
        e.Set<Position>({1,2});
 
        e1.AddComponent<Position>();
        e1.Set<Position>({3,4});
        
        e2.AddComponent<Position>();
        e2.Set<Position>({10,12});

        std::cout << e.Get<Position>().x <<", " << e.Get<Position>().y << std::endl;
        std::cout << e1.Get<Position>().x <<", " << e1.Get<Position>().y << std::endl;
        std::cout << e2.Get<Position>().x <<", " << e2.Get<Position>().y << std::endl;
        
        e.AddComponent<Velocity>();
        e.Set<Velocity>({0.0f, 0.5f});
        std::cout << e.Get<Position>().x <<", " << e.Get<Position>().y << std::endl;
        std::cout << e1.Get<Position>().x <<", " << e1.Get<Position>().y << std::endl;
        std::cout << e2.Get<Position>().x <<", " << e2.Get<Position>().y << std::endl;
        
        std::cout << e.Get<Velocity>().x <<", " << e.Get<Velocity>().y << std::endl;

        ECS::DestroyWorld(world);
    }   

    void GameLayer::OnEvent(const Event& e)
    {
        switch(e.GetEventType())
        {

            case EventType::KEY_PRESSED:
            {
                std::cout << "Key Pressed Game Layer" << std::endl;
                break;
            }
            case EventType::KEY_RELEASED:
            {
                std::cout << "Key Released Game Layer" << std::endl;
                break;
            }
        }
    }
}