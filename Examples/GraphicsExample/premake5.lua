project "GraphicsExample"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	debugdir "../"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}

	defines
	{
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"GLM_FORCE_LEFT_HANDED",
	}

	includedirs
	{
		"%{wks.location}/Hog-Core/vendor/spdlog/include",
		"%{wks.location}/Hog-Core/src",
		"%{wks.location}/Hog-Core/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.vma}",
		"%{IncludeDir.tinyobjloader}",
		"%{IncludeDir.cgltf}",
		"%{IncludeDir.optick}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.volk}",
		"%{IncludeDir.VulkanSDK}",
	}

	links
	{
		"Hog-Core",
		"Volk",
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "HG_DEBUG"
		runtime "Debug"
		symbols "on"
		
		postbuildcommands
		{
			"{COPY} \"%{SharedLibrary.shaderc_Debug}\" \"%{cfg.targetdir}\""
		}

	filter "configurations:Asan"
		defines "HG_ASAN"
		defines "HG_DEBUG"
		runtime "Debug"
		symbols "on"
		flags { "NoRuntimeChecks" }
		editAndContinue "Off"
		buildoptions { "/Zi /DEBUG:FULL /Ob0 /Oy-" }
		
		postbuildcommands
		{
			"{COPY} \"%{SharedLibrary.shaderc_Debug}\" \"%{cfg.targetdir}\""
		}

	filter "configurations:Release"
		defines "HG_RELEASE"
		runtime "Release"
		optimize "on"

		postbuildcommands
		{
			"{COPY} \"%{SharedLibrary.shaderc_Release}\" \"%{cfg.targetdir}\"",
		}

	filter "configurations:Dist"
		defines "HG_DIST"
		runtime "Release"
		optimize "on"
		
		postbuildcommands
		{
			"{COPY} \"%{SharedLibrary.shaderc_Release}\" \"%{cfg.targetdir}\"",
		}

	filter "configurations:Profile"
		defines "HG_PROFILE"
		runtime "Release"
		optimize "on"
		
		postbuildcommands
		{
			"{COPY} \"%{SharedLibrary.optick}\" \"%{cfg.targetdir}\"",
			"{COPY} \"%{SharedLibrary.shaderc_Release}\" \"%{cfg.targetdir}\""
		}