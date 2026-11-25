#include "layer.h"
#include "application.h"

namespace VoidEngine
{
const InputManager &Layer::input() const
{
    return app_->get_input();
}
} // namespace VoidEngine
