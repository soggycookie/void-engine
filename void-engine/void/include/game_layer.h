#pragma once
#include "layer.h"
#include "resource.h"
#include "ecs.h"


namespace VoidEngine
{
    struct Position
    {
        uint32_t x, y;
    };
    
    ECS_COMPONENT(Position)

    struct Velocity
    {
        float x, y;
    };
    ECS_COMPONENT(Velocity)

    struct Rotation
    {
        float x, y;
    };
    ECS_COMPONENT(Rotation)

    struct NPC
    {
    };
    ECS_COMPONENT(NPC);

    class GameLayer : public Layer
    {
    public:
        GameLayer():
            m_gameTime(0)
        {
        }

        void OnInit() override;
        void OnDetach() override;
        void OnAttach() override;
        void OnUpdate(double dt) override;
        void OnEvent(const Event& e) override;

    private:
        ECS::World* world;
        size_t m_gameTime;
        double elapsedTime;
        MeshResource* mesh;
        MaterialResource* material;
    };
}