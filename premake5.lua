workspace "model_viewer"
    configurations {"Debug", "Release"}
    platforms {"Win64"}

filter {"platforms:Win64"}
    system "Windows"
    architecture "x86_64"

project "model_viewer"
    kind "WindowedApp"
    language "C++"
    cppdialect "c++20"
    targetdir "bin/%{cfg.buildcfg}"

    includedirs{
        "C:/VulkanSDK/1.3.204.0/Include"
    }

    libdirs{
        "C:/VulkanSDK/1.3.204.0/Lib"
    }

    links{
        "kernel32.lib",
        "winmm.lib",
        "user32.lib",
        "vulkan-1.lib"
    }

    files {
        "source/**.hpp",
        "source/**.cpp",
        "source/**.ixx"
    }

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"
        optimize "Off"
        intrinsics "On"
        debugdir "bin/Debug"

    filter "configurations:Release"
        symbols "Off"
        optimize "On"
        intrinsics "On"
        flags {
            "LinkTimeOptimization"
        }