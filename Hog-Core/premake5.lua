project "Hog-Core"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "hgpch.h"
	pchsource "src/hgpch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",
		"vendor/stb_image/**.h",
		"vendor/stb_image/**.cpp",
		"vendor/vma/**.h",
		"vendor/vma/**.cpp",
		"vendor/tinyobjloader/**.h",
		"vendor/tinyobjloader/**.cpp",
		"vendor/glm/glm/**.hpp",
		"vendor/glm/glm/**.inl",
		"vendor/cgltf/**.h",
		"vendor/cgltf/**.cpp",
		"vendor/spirv-reflect/spirv_reflect.cpp",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_VULKAN",
		"VK_VERSION_1_3"
	}

	includedirs
	{
		"src",
		"vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.cgltf}",
		"%{IncludeDir.vma}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.tinyobjloader}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.optick}",
		"%{IncludeDir.SPIRV_Reflect}",
	}

	links
	{
		"GLFW",
		"yaml-cpp",
		"ImGui"
	}

	filter "system:windows"
		systemversion "latest"

		defines
		{
		}

	filter "configurations:Debug"
		defines "HG_DEBUG"
		runtime "Debug"
		symbols "on"
		-- editAndContinue "Off"

		links
		{
			"%{Library.ShaderC_Debug}",
			"%{Library.SPIRV_Cross_Debug}",
			"%{Library.SPIRV_Cross_GLSL_Debug}"
		}

	filter "configurations:Release"
		defines "HG_RELEASE"
		runtime "Release"
		optimize "on"

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}

	filter "configurations:Dist"
		defines "HG_DIST"
		runtime "Release"
		optimize "on"

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}

	filter "configurations:Profile"
		defines "HG_PROFILE"
		runtime "Release"
		optimize "on"

		links
		{
			"OptickCore",
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}
