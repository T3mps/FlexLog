workspace "FlexLog"
	architecture "x64"
	startproject "FlexLog"
	configurations { "Debug", "Release" }
	flags { "MultiProcessorCompile" }

	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

	IncludeDir = {}
	IncludeDir["FlexLog"] = "FlexLog/src"

project "FlexLog"
	location "FlexLog"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files {
		"%{IncludeDir.FlexLog}/**.h",
		"%{IncludeDir.FlexLog}/**.cpp",
		"%{IncludeDir.FlexLog}/**.hpp"
	}

	includedirs {
		"%{IncludeDir.FlexLog}"
	}

	defines { "_CRT_SECURE_NO_WARNINGS" }

	filter "configurations:Debug"
		defines { "LOGGER_DEBUG" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines { "LOGGER_RELEASE" }
		runtime "Release"
		optimize "on"
