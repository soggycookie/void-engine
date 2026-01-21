#pragma once
#include "event.h"

namespace VoidEngine
{
    class ApplicationClosedEvent : public Event
    {
    public:
        ApplicationClosedEvent()
        {
        }

        EVENT_CATEGORY(EventCategory::APPLICATION)
        EVENT_TYPE(EventType::APP_CLOSED)
    };

    class ApplicationResizingEvent : public Event
    {
    public:
        ApplicationResizingEvent(ClientDimension dimension)
            : m_dimension(dimension)
        {
        }
        
        ClientDimension GetDimension() const
        {
            return m_dimension;
        }

        EVENT_CATEGORY(EventCategory::APPLICATION)
        EVENT_TYPE(EventType::APP_RESIZING)
            
    private:
        ClientDimension m_dimension;
    };

    class ApplicationEnterResizeEvent : public Event
    {
    public:
        ApplicationEnterResizeEvent()
        {
        }

        EVENT_CATEGORY(EventCategory::APPLICATION)
        EVENT_TYPE(EventType::APP_ENTER_RESIZE)
    };

    class ApplicationExitResizeEvent : public Event
    {
    public:
        ApplicationExitResizeEvent(ClientDimension dimension)
            : m_dimension(dimension)
        {
        }

        ClientDimension GetDimension() const
        {
            return m_dimension;
        }

        EVENT_CATEGORY(EventCategory::APPLICATION)
        EVENT_TYPE(EventType::APP_EXIT_RESIZE)

    private:
        ClientDimension m_dimension;
    };
}