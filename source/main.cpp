#include "base.hpp"
#include "model_viewer.hpp"

void EntryPoint(plt::device d)
{
    plt::SpawnWindow(d, "3D model viewer");

    auto app = model_viewer(d);

    float dt = 0.001f;

    while(plt::GetFlag(d, plt::core_flag::running)){
        plt::PollEvents(d);

        if(plt::IsKeyDown(plt::key_code::alt)){
            if(plt::KeyEvent(d, plt::key_event::f4))
                plt::Terminate(d);
            else if(plt::KeyEvent(d, plt::key_event::f))
                plt::ToggleFullScreen(d);
        }

        app.run(d, dt);
        dt = plt::GetTimestep(d);
    }
}