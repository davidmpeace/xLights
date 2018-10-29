#ifndef SNOWFLAKESEFFECT_H
#define SNOWFLAKESEFFECT_H

#include "RenderableEffect.h"

#define SNOWFLAKES_COUNT_MIN 1
#define SNOWFLAKES_COUNT_MAX 100

#define SNOWFLAKES_SPEED_MIN 0
#define SNOWFLAKES_SPEED_MAX 50

class SnowflakesEffect : public RenderableEffect
{
    public:
        SnowflakesEffect(int id);
        virtual ~SnowflakesEffect();
        virtual void SetDefaultParameters() override;
        virtual void Render(Effect *effect, SettingsMap &settings, RenderBuffer &buffer) override;
protected:
        virtual wxPanel *CreatePanel(wxWindow *parent) override;
        virtual bool needToAdjustSettings(const std::string &version) override;
        virtual void adjustSettings(const std::string &version, Effect *effect, bool removeDefaults = true) override;
    private:
};

#endif // SNOWFLAKESEFFECT_H
