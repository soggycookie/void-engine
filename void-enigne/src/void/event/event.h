#pragma once
#include "void/pch.h"
#include "void/common_type.h"

namespace VoidEngine
{
    enum class EventCategory
    {
        KEYBOARD,
        MOUSE,
        APPLICATION
    };

    enum class EventType
    {
        KEY_PRESSED,
        KEY_RELEASED,
        
        APP_CLOSED,
        APP_RESIZING,
        APP_ENTER_RESIZE,
        APP_EXIT_RESIZE,

        MOUSE_PRESSED,
        MOUSE_RELEASED,
        MOUSE_WHEEL_ROTATED,
        MOUSE_MOVE
    };


#define EVENT_CATEGORY(category) EventCategory GetEventCategory() const override { return category;}
#define EVENT_TYPE(event) EventType GetEventType() const override { return event;}

    //TODO: centralize into event queue
    class Event
    {
    public:
        Event() = default;
        virtual ~Event() = default;

        virtual EventCategory GetEventCategory() const = 0;
        virtual EventType GetEventType() const = 0;
    };

}