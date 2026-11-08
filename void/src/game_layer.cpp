#include "game_layer.h"
#include "application.h"
#include "arena.h"
#include "logger/log.h"
#include "str.h"

namespace VoidEngine
{
void GameLayer::on_attach()
{
}

void GameLayer::on_update(double dt)
{
    const auto &input = Layer::get_input();
    EcsInputManager &ecs_input = world->get_singleton<EcsInputManager>();
    ecs_input.set(input.prev_mask(), input.cur_mask(), input.get_mouse_pos().mouseX,
                  input.get_mouse_pos().mouseY);

    if (input.is_btn_released(KeyCode::P))
    {
        std::cout << "Pause" << std::endl;
        paused = !paused;
    }

    if (ecs_input.is_btn_released(KeyCode::SPACE))
    {
        std::cout << "SPACE RELEASED" << std::endl;
    }

    if (!paused)
    {
        ECS::Time &time = world->get_singleton<ECS::Time>();
        time.delta_time = app->get_delta_time();
        time.application_time = app->get_app_time();

        world->tick();
        // std::cout << "Frame " << frameCount << std::endl;

        // Application::get_app().get_vulkand_backend().DrawTest();
    }
    FLUSH_LOG();
}

void GameLayer::on_detach()
{
    ECS::destroy_world(app->get_allocator(), world);
}

void GameLayer::on_init()
{
    world = ECS::create_world(app->get_allocator());
    world->set_worker_count(6);
    world->component<EcsInputManager>().id(1000).singleton().register_type();
    world->component<Position>().id(1001).register_type();
    world->component<Velocity>().id(1002).register_type();
    world->tag<NPC>().id(1004).register_type();
    world->tag<Observer>().id(1005).register_type();
    world->setup_default_pipeline();

    // ECS::Entity e = world->create_entity("First").build();
    // world->assign_component<Position>(e.get_eid(), Position{100, 100});

    // world->entity_builder().add_component<Velocity>().build();
    // world->entity_builder().add_component<Velocity>().build();
    // world->entity_builder().add_component<Velocity>().build();
    // world->entity_builder().add_component<Position>().build();
    // world->entity_builder().add_component<Position>().build();
    // world->entity_builder().add_component<Position>().build();

    ECS::Entity observer = world->create_entity("Observer").add_tag<Observer>().build();

    world->system_builder<Position>()
        .phase(ECS::ON_UPDATE_ID)
        .each(
            +[](const ECS::QueryIter &iter, Position &pos)
            {
                ECS::Time &time = iter.world->get_singleton<ECS::Time>();
                pos.x += time.delta_time;
                pos.y -= time.delta_time;
            },
            nullptr, "UpdatePos");

    world->system_builder<Velocity>()
        .phase(ECS::ON_UPDATE_ID)
        .each(
            +[](const ECS::QueryIter &iter, Velocity &vel)
            {
                ECS::Time &time = iter.world->get_singleton<ECS::Time>();
                vel.x += time.delta_time * 2;
                vel.y -= time.delta_time * 2;
            },
            nullptr, "UpdateVel");

    world->system_builder<Position>()
        .with<ECS::Name>()
        .phase(ECS::ON_UPDATE_ID)
        .each(
            +[](const ECS::QueryIter &iter, Position &pos, ECS::Name &name)
            { std::cout << name.name << " has Position X: " << pos.x << ", Y: " << pos.y << std::endl; },
            nullptr, "PrintPos");

    world->system_builder<Velocity>()
        .with<ECS::Name>()
        .phase(ECS::ON_UPDATE_ID)
        .each(
            +[](const ECS::QueryIter &iter, Velocity &vel, ECS::Name &name)
            { std::cout << name.name << " has Velocity X: " << vel.x << ", Y: " << vel.y << std::endl; },
            nullptr, "PrintVel");

    world->system_builder<Observer>()
        .with<ECS::Name>()
        .phase(ECS::ON_POST_UPDATE_ID)
        .each(
            +[](const ECS::QueryIter &iter)
            {
                EcsInputManager &input = iter.world->get_singleton<EcsInputManager>();
                ECS::Time &time = iter.world->get_singleton<ECS::Time>();
                // std::cout << "==== Observer ====" << std::endl;
                static size_t c = 1;
                if (input.is_btn_released(KeyCode::SPACE))
                {
                    char name[32];
                    std::snprintf(name, 32, "Fucker no %u", c);
                    iter.world->create_entity(name)
                        .assign_component<Position>(
                            Position{time.elapsed_time * 10, time.elapsed_time * 5})
                        .build();
                    char name_b[32];
                    std::snprintf(name_b, 32, "Moron no %u", c);
                    iter.world->create_entity(name_b)
                        .assign_component<Velocity>(
                            Velocity{(float)time.elapsed_time * 10, (float)time.elapsed_time * 5})
                        .build();
                    std::cout << "SPACE " << c++ << std::endl;
                }
            },
            nullptr, "Observer");
    //
    // // FileHandle handle = FileSystem::Get().Open(u8"resource/shader/slang/triangle.slang");
    // // bool result = FileSystem::Get().AsyncRead(handle);
    // // ENGINE_ASSERT(result, "Async Read fail miserably!");
}

void GameLayer::on_event(InputEvent &e)
{
    Layer::on_event(e);
}

void GameLayer::on_post_update()
{
    Layer::on_post_update();
}

} // namespace VoidEngine
