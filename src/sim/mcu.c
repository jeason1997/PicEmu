#include "picemu/sim/mcu.h"

#include <stddef.h>

void sim_mcu_reset(SimMcu *mcu)
{
    if (mcu != NULL && mcu->ops != NULL && mcu->ops->reset != NULL) {
        mcu->ops->reset(mcu);
    }
}

unsigned sim_mcu_step(SimMcu *mcu)
{
    if (mcu == NULL || mcu->ops == NULL || mcu->ops->step == NULL) return 0;
    return mcu->ops->step(mcu);
}

unsigned sim_mcu_pin_count(const SimMcu *mcu)
{
    if (mcu == NULL || mcu->ops == NULL ||
        mcu->ops->pin_count == NULL) return 0;
    return mcu->ops->pin_count(mcu);
}

SimLevel sim_mcu_pin_drive(const SimMcu *mcu, unsigned pin)
{
    if (mcu == NULL || mcu->ops == NULL ||
        mcu->ops->pin_drive == NULL) return SIM_LEVEL_Z;
    return mcu->ops->pin_drive(mcu, pin);
}

void sim_mcu_set_pin_input(SimMcu *mcu, unsigned pin, SimLevel level)
{
    if (mcu != NULL && mcu->ops != NULL &&
        mcu->ops->set_pin_input != NULL) {
        mcu->ops->set_pin_input(mcu, pin, level);
    }
}

bool sim_mcu_stopped(const SimMcu *mcu)
{
    return mcu != NULL && mcu->ops != NULL &&
           mcu->ops->stopped != NULL && mcu->ops->stopped(mcu);
}
