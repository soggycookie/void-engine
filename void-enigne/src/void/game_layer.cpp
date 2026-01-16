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

        auto shader = ResourceSystem::Load<ShaderResource>(L"src//asset//shader//square_demo.hlsl");
        material = ResourceSystem::Create<MaterialResource>(ResourceSystem::GenerateGUID(), shader->GetGUID());

        //std::cout << "Entity id: " << e.GetId() << " , gen count: " << e.GetGenCount() << std::endl;
        //std::cout << "Entity id: " << e.GetId() << " , gen count: " << e.GetGenCount() << std::endl;
        
        world.Register<Position>();
        world.Register<Velocity>();
        world.Register<Rotation>();


        ECS::Entity e = world.CreateEntity();
        ECS::Entity e1 = world.CreateEntity();
        ECS::Entity e2 = world.CreateEntity();
        ECS::Entity e3 = world.CreateEntity();
        ECS::Entity e4 = world.CreateEntity();
        
        e.Add<Position>({1,1});
        e1.Add<Position>({200, 973});
        e2.Add<Position>({24800, 8973});
        e3.Add<Position>({920, 72});
        e4.Add<Position>({39, 2});

        world.Each<Position>(
            [](ECS::Entity e, Position& pos)
            {
                std::cout << "Position " << std::endl;
                std::cout << "x: " << pos.x << std::endl;
                std::cout << "y: " << pos.y << std::endl;
            }
        );
        std::cout << "============================"  << std::endl;
        e.Remove<Position>();
        world.Each<Position>(
            [](ECS::Entity e, Position& pos)
            {
                std::cout << "Position " << std::endl;
                std::cout << "x: " << pos.x << std::endl;
                std::cout << "y: " << pos.y << std::endl;
            }
        );

        //Position* eP = e.Get<Position>();
        //std::cout << "x: " << eP->x << std::endl;
        //std::cout << "y: " << eP->y << std::endl;
        //e1.Add<Velocity>({0.5f, 0.5f});
        //e2.Add<Velocity>({1.3f, 2.25f});
        //world.Each<Velocity>(
        //    [](ECS::Entity e, Velocity& pos)
        //    {
        //        std::cout << "velocity " << std::endl;
        //        std::cout << "x: " << pos.x << std::endl;
        //        std::cout << "y: " << pos.y << std::endl;
        //    }
        //);

        //e2.Add<Rotation>({24.3f, 28.0f});
        //world.Each<Rotation>(
        //    [](ECS::Entity e, Rotation& pos)
        //    {
        //        std::cout << "Rotation " << std::endl;
        //        std::cout << "x: " << pos.x << std::endl;
        //        std::cout << "y: " << pos.y << std::endl;
        //    }
        //);
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