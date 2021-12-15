#include "base.hpp"
#include "model_viewer.hpp"

void ApplicationEntryPoint(Platform::lDevice device)
{
    const auto permanentCapacity = MegaBytes(64);
    const auto transientCapacity = MegaBytes(512);

    Platform::SetupWindow(device, "3D model viewer");

    auto app = model_viewer(device);

    float dt = 0.001f;
    auto flags = Platform::QueryFlags(device);
    while (flags & Platform::FLAG_RUNNING){
        auto input = Platform::PollEvents(device);

        if(Platform::IsKeyDown(KeyCode::ALT)){
            if(input->keyPressEvents & KEY_EVENT_F4){
                Platform::Terminate(device);
                break;
            }

            if(input->keyPressEvents & KEY_EVENT_F)
                Platform::ToggleFullscreen(device);
        }

        app.run(input, flags, dt);

        flags = Platform::QueryFlags(device);
        dt = Platform::GetTimestep(device);
    }
}