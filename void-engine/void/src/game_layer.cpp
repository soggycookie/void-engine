#include "game_layer.h"
#include "profiler.h"
#include "renderer.h"
#include "resource_system.h"

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
    world->Progress(0);

    Renderer::NewFrame();
    Renderer::Draw(mesh, material);
    Renderer::EndFrame();
}

void GameLayer::OnDetach()
{
    SIMPLE_LOG("detach!");

    world->Each<ECS::EcsName>(
        +[](ECS::QueryIterator it, const ECS::EcsName &pos)
        {
            std::cout << it.GetEntity().GetFullId() << ": " << pos.name
                      << std::endl;
            // std::cout << "Count " << it.archetype->count << " of " <<
            // it.archetype->id << std::endl; std::cout <<
            // it.GetEntity().GetFullId() << std::endl; std::cout << pos.x << ",
            // " << pos.y << std::endl;
        });

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
    // std::cout << "Entity id: " << e.GetId() << " , gen count: " <<
    // e.GetGenCount() << std::endl;

    world = ECS::CreateWorld();
    world->Component<Position>().Register();
    world->Component<Velocity>().Singleton().Register();
    world->Tag<NPC>().Register();

    // uint32_t x = 0, y = 0;
    // for(uint32_t i = 0; i < 5; i++)
    //{
    //     world->CreateEntity(0).AddComponent<Position>().AddTag<NPC>().Set<Position>({++x,
    //     ++y}).AddComponent<Velocity>();
    // }
    ECS::Entity e0 = world->CreateEntity("Test subject", 0);
    // e0.AddComponent<Velocity>().Set<Velocity>(Velocity{0.5f, 0.5f});
    //  std::cout << e0.GetLowId() << std::endl;

    // world->RemoveEntity(e0.GetFullId());

    ECS::Entity e = world->CreateEntity("First", e0.GetFullId());
    e.AddComponent<Position>().Set<Position>({1, 1});

    ECS::Entity e1 = world->CreateEntity("Second ", e.GetFullId());
    e1.AddComponent<Position>().
        // AddComponent<Velocity>().
        // AddPair<ECS::ChildOf>(e.GetFullId()).
        Set<Position>({555, 123123});

    auto q =
        world->Query<Position>()
            .With<Velocity>()
            .TraverseAny<ECS::EcsChildOf>()
            .Through(ECS::UP)
            .Each(
                +[](const ECS::QueryIter &iter, const Velocity &parentVel,
                    const Position &e)
                {
                    std::cout << "Parent Vel x: " << parentVel.x << std::endl;
                    std::cout << "Parent Vel y: " << parentVel.y << std::endl;
                    std::cout << "Entity x: " << e.x << std::endl;
                    std::cout << "Entity y: " << e.y << std::endl;
                },
                nullptr);
    q.Execute();
    // q.Destroy();
    std::cout << world->GetSingleton<Velocity>().x << std::endl;
    std::cout << world->GetSingleton<Velocity>().y << std::endl;

    world->System<Position, ECS::EcsChildOf>(+[](Position &pos)
                                             {
                                                 ++pos.x;
                                                 ++pos.y;
                                             });
}

void GameLayer::OnEvent(const InputEvent &e)
{
    switch (e.Type())
    {

    case InputEventType::KEY_PRESSED:
    {
        std::cout << "Key Pressed Game Layer" << std::endl;
        break;
    }
    case InputEventType::KEY_RELEASED:
    {
        std::cout << "Key Released Game Layer" << std::endl;
        break;
    }
    default:
    {
        break;
    }
    }
}
} // namespace VoidEngine
