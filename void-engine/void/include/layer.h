#pragma once
#include "pch.h"
#include "input_event.h"

namespace VoidEngine
{
class Layer
{
public:
    static constexpr const uint32_t MaxInputBuffer = 16;

    Layer() = default;

    virtual ~Layer() = default;
    virtual void OnInit() = 0;
    virtual void OnDetach() = 0;
    virtual void OnAttach() = 0;

    virtual void OnUpdate(double dt) = 0;
    virtual void OnEvent(const InputEvent &e) = 0;

protected:
    // Event m_inputBuffer[MaxInputBuffer];
};

} // namespace VoidEngine
