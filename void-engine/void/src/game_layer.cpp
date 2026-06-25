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

    EcsInputManager &input = world->GetSingleton<EcsInputManager>();
    input.Set(Input().PrevMask(), Input().CurrMask(),
              Input().GetMousePos().mouseX, Input().GetMousePos().mouseY);
    
    if(input.IsBtnReleased(KeyCode::SPACE))
    {
        std::cout << "SPACE RELEASED" << std::endl;
    }

    ECS::EcsTime &time = world->GetSingleton<ECS::EcsTime>();
    time.deltaTime = m_app->GetDeltaTime();
    time.applicationTime = m_app->GetApplicationTime();

    //std::cout << "--------------- Frame " << frameCount << " -----------------" << std::endl;

    world->Tick();
    // std::cout << "Mouse Pos ECS: " << input.GetMousePos().mouseX <<
    // std::endl;

    Renderer::DrawTest();

    frameCount++;

    FLUSH_LOG();
}

void GameLayer::OnDetach()
{
    SIMPLE_LOG("detach!");

    ECS::DestroyWorld(world);
}

void GameLayer::OnInit()
{
    //Vertex quadVertices[] = {
    //    //   position (x, y, z, w)       uv
    //    {{-1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // top-left
    //    {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // top-right
    //    {{1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // bottom-right
    //    {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}} // bottom-left
    //};

    //uint32_t quadIndices[] = {0, 1, 2, 0, 2, 3};

    //mesh = ResourceSystem::Create<MeshResource>(123, false);
    //mesh->SetVertexData(quadVertices, 4);
    //mesh->SetIndexData(quadIndices, 6);
    //mesh->SubmitMeshToGpu();

    //auto shader = ResourceSystem::Load<ShaderResource>(
    //    L"asset//shader//square_demo.hlsl");
    //material = ResourceSystem::Create<MaterialResource>(
    //    ResourceSystem::GenerateGUID(), shader->GetGUID());
     

    world = ECS::CreateWorld();
    world->Component<EcsInputManager>().Singleton().Register();
    world->Component<Position>().Register();
    world->Component<Velocity>().Register();
    world->Tag<NPC>().Register();
    world->InitDefaultPipelinePhase();
    //world->BootstrapPhase().DependedPhase(300, "OnStart");
    //world->LoopPhase().DependedPhase(301, "OnUpdate").DependedPhase(302, "OnPostUpdate");

    ECS::Entity e0 = world->CreateEntity("Test subject", 0).Build();
    // e0.AddComponent<Velocity>().Set<Velocity>(Velocity{0.5f, 0.5f});
    //  std::cout << e0.GetLowId() << std::endl;

    // world->RemoveEntity(e0.GetFullId());

    ECS::Entity e = world->CreateEntity("First", e0.GetFullId()).Build();
    world->AssignComponent<Position>(e.GetFullId(), Position{100, 100});

    ECS::Entity e1 = world->CreateEntity("Second ", e0.GetFullId()).Build();
    
    //world->CreateSystem<Position>().
    //    Phase(ECS::EcsOnUpdateId).
    //    Each(+[](const ECS::QueryIter& iter ,Position& pos)
    //         {
    //            ECS::EcsTime& time = iter.world->GetSingleton<ECS::EcsTime>();
    //            pos.x += time.deltaTime;
    //            pos.y -= time.deltaTime;
    //         }, "UpdatePos");

    //world->CreateSystem<Position>().With<NPC>().
    //    Phase(ECS::EcsOnUpdateId).
    //    Each(+[](const ECS::QueryIter& iter , Position& pos)
    //         {
    //            ECS::EcsTime& time = iter.world->GetSingleton<ECS::EcsTime>();
    //            pos.x += time.deltaTime;
    //            pos.y -= time.deltaTime;
    //         }, "UpdateNpcPos");

    //world->CreateSystem<ECS::EcsSystem>().With<ECS::EcsName>().
    //    Phase(ECS::EcsOnUpdateId).
    //    Each(+[](const ECS::QueryIter& iter, ECS::EcsSystem& sys, const ECS::EcsName& name)
    //         {
    //             std::cout << name.name << std::endl;
    //         }, "DisplayName");
}

void GameLayer::OnEvent(InputEvent &e) { Layer::OnEvent(e); }

void GameLayer::OnEndFrame() { Layer::OnEndFrame(); }

} // namespace VoidEngine
