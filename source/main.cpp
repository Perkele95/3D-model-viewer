#include "base.hpp"
#include "platform/platform.hpp"
#include "model_viewer.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
int WINAPI WinMain(_In_ HINSTANCE hInstance,
                   _In_opt_ HINSTANCE hPrevInstance,
                   _In_ PSTR lpCmdLine,
                   _In_ INT nCmdShow)
#else
int main(int argc, char **argv)
#endif
{
    auto app = ModelViewer();
    app.run();
    return 0;
}