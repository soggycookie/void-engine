#include "game_layer.h"
#include "profiler.h"
#include "renderer.h"
#include "resource_system.h"
#include "application.h"

namespace VoidEngine
{
void GameLayer::OnAttach()
{
    SIMPLE_LOG("attach!");

    // std::cout << "Test: " <<
    // std::is_trivially_constructible_v<ECS::EcsSystem>
    // << std::endl;
}

void GameLayer::OnUpdate(double dt)
{

    ECS::EcsInputManager &input = world->GetSingleton<ECS::EcsInputManager>();
    input.Set(Input().PrevMask(), Input().CurrMask(),
              Input().GetMousePos().mouseX, Input().GetMousePos().mouseY);
    
    ECS::EcsTime &time = world->GetSingleton<ECS::EcsTime>();
    time.deltaTime = m_app->GetDeltaTime();
    time.applicationTime = m_app->GetApplicationTime();

    world->Tick();
    // std::cout << "Mouse Pos ECS: " << input.GetMousePos().mouseX <<
    // std::endl;

    Renderer::NewFrame();
    Renderer::Draw(mesh, material);
    Renderer::EndFrame();
}

void GameLayer::OnDetach()
{
    SIMPLE_LOG("detach!");

    ECS::DestroyWorld(world);
}

void GameLayer::OnInit()
{
    Vertex quadVertices[] = {
        //   position (x, y, z, w)       uv
        {{-1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // top-left
        {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // top-right
        {{1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // bottom-right
        {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}} // bottom-left
    };

    uint32_t quadIndices[] = {0, 1, 2, 0, 2, 3};

    mesh = ResourceSystem::Create<MeshResource>(123, false);
    mesh->SetVertexData(quadVertices, 4);
    mesh->SetIndexData(quadIndices, 6);
    mesh->SubmitMeshToGpu();

    auto shader = ResourceSystem::Load<ShaderResource>(
        L"asset//shader//square_demo.hlsl");
    material = ResourceSystem::Create<MaterialResource>(
        ResourceSystem::GenerateGUID(), shader->GetGUID());


    world = ECS::CreateWorld();
    world->Component<Position>().Register();
    world->Component<Velocity>().Register();
    world->Tag<NPC>().Register();

    world->BootstrapPhase().DependedPhase(300, "OnStart");
    world->LoopPhase().DependedPhase(301, "OnUpdate").DependedPhase(302, "OnPostUpdate");

    ECS::Entity e0 = world->CreateEntity("Test subject", 0);
    // e0.AddComponent<Velocity>().Set<Velocity>(Velocity{0.5f, 0.5f});
    //  std::cout << e0.GetLowId() << std::endl;

    // world->RemoveEntity(e0.GetFullId());

    ECS::Entity e = world->CreateEntity("First", e0.GetFullId());
    e.AddComponent<Position>().Set<Position>({1, 1});

    ECS::Entity e1 = world->CreateEntity("Second ", e0.GetFullId());
    e1.AddComponent<Position>().Set<Position>({555, 123});


     //ECS::QueryHandle q = world->CreateQuery<ECS::EcsName, ECS::EcsPhase>().
     //   Each(
     //   +[](const ECS::QueryIter &iter, const ECS::EcsName& name)
     //   {
     //       std::cout << "Name: " << name.name << std::endl;

     //   },
     //   nullptr);   
     //q.Execute();

    world->CreateSystem<Position>().
        DependOn(301).
        Each(+[](const ECS::QueryIter& iter ,Position& pos)
             {
                ECS::EcsTime& time = iter.world->GetSingleton<ECS::EcsTime>();
                pos.x += time.deltaTime;
                pos.y -= time.deltaTime;
             });

    world->CreateSystem<Position>().
        DependOn(302).
        Each(+[](const Position& pos)
             {
                std::cout << "Position: x = " << pos.x << ", y = " << pos.y << std::endl; 
             });

    //ECS::QueryHandle q = world->CreateQuery<Position>().Cache(0).
    //    Filter(+[](const Position&p){ return p.x > 20 && p.y < 300;}, nullptr).
    //    Each(
    //    +[](const ECS::QueryIter &iter, const Position &e)
    //    {
    //        std::cout << "Entity x: " << e.x << std::endl;
    //        std::cout << "Entity y: " << e.y << std::endl;
    //    },
    //    nullptr);

    //q.Execute();

    //e.AddComponent<Position>().Set<Position>(Position{21, 200});
    //q.Execute();



    // q.Destroy();

}

void GameLayer::OnEvent(InputEvent &e) { Layer::OnEvent(e); }

void GameLayer::OnEndFrame() { Layer::OnEndFrame(); }

} // namespace VoidEngine
