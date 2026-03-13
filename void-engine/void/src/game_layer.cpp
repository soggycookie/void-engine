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
    std::cout << "Test" << std::endl;
    // std::cout << "Entity id: " << e.GetId() << " , gen count: " <<
    // e.GetGenCount() << std::endl;

    world = ECS::CreateWorld();
    world->Component<Position>().Register();
    world->Component<Velocity>().Register();
    world->Tag<NPC>().Register();

    // uint32_t x = 0, y = 0;
    // for(uint32_t i = 0; i < 5; i++)
    //{
    //     world->CreateEntity(0).AddComponent<Position>().AddTag<NPC>().Set<Position>({++x,
    //     ++y}).AddComponent<Velocity>();
    // }

    ECS::Entity e = world->CreateEntity("First", 0);
    e.AddComponent<Position>().
        // AddTag<NPC>().
        // AddComponent<Velocity>().
        Set<Position>({1, 1});

    // ECS::Entity e2 = world->CreateEntity(0);
    // e2.AddComponent<Position>().
    //     AddTag<NPC>().
    //     AddComponent<Velocity>().
    //     Set<Position>({1, 1});

    ECS::Entity e1 = world->CreateEntity("Second ", e.GetFullId());
    e1.AddComponent<Position>().
        // AddTag<NPC>().
        // AddComponent<Velocity>().
        // AddPair<ECS::ChildOf>(e.GetFullId()).
        Set<Position>({1, 1});

    // std::cout << ECS::ComponentTypeId<ECS::EcsChildOf>::Id() << std::endl;

    auto a = world->m_componentIndex.GetValue(
        ECS::MakeRelationship(ECS::EcsChildOfId, e.GetFullId()));

    // std::cout << a.archetypeStore.count << std::endl;
    // std::cout <<
    // world->m_componentIndex.GetValue(ECS::MakePair(ECS::ChildOfId,
    // e.GetFullId())).name << std::endl;
    world->System<Position, ECS::EcsChildOf>(+[](Position &pos)
                                             {
                                                 ++pos.x;
                                                 ++pos.y;
                                             });
}

void GameLayer::OnEvent(const Event &e)
{
    switch (e.GetEventType())
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
    default:
    {
        break;
    }
    }
}
} // namespace VoidEngine
