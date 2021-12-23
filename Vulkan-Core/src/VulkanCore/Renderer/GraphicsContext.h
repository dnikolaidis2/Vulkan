#pragma once

#include <vulkan/vulkan.h>

#include "Image.h"
#include "vk_mem_alloc.h"
#include "VulkanCore/Core/Base.h"

namespace VulkanCore {

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		switch(messageSeverity)
		{
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			{
				VKC_CORE_INFO(pCallbackData->pMessage);
			} break;

			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			{
				VKC_CORE_WARN(pCallbackData->pMessage);
			} break;

			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			{
				VKC_CORE_ERROR(pCallbackData->pMessage);
			} break;
		}

		return VK_FALSE;
	}

	class GraphicsContext
	{
	public:

		struct GPUInfo
		{
			VkPhysicalDevice Device;
			std::vector<VkQueueFamilyProperties> QueueFamilyProperties;
			std::vector<VkExtensionProperties> ExtensionProperties;
			VkSurfaceCapabilitiesKHR SurfaceCapabilities;
			std::vector<VkSurfaceFormatKHR> SurfaceFormats;
			std::vector<VkPresentModeKHR> PresentModes;
			VkPhysicalDeviceMemoryProperties MemoryProperties;
			VkPhysicalDeviceProperties DeviceProperties;
			VkPhysicalDeviceFeatures PhysicalDeviceFeatures;
		};

	public:
		static GraphicsContext& Get()
		{
			static GraphicsContext instance;

			return instance;
		}

		~GraphicsContext() { if (Initialized == true) Get().Deinitialize(); }

		static void Initialize() { Get().InitializeImpl(); }
		static void Deinitialize() { Get().DeinitializeImpl(); }
		static void RecreateSwapChain() { Get().RecreateSwapChainImpl(); }
		static const VmaAllocator& GetAllocator() { return Get().Allocator; }
		static const VkDevice& GetDevice() { return Get().Device; }
		static const VkExtent2D& GetExtent() { return Get().SwapchainExtent; }
		static const VkSwapchainKHR& GetSwapchain() { return Get().Swapchain; }
		static const VkQueue& GetGraphicsQueue() { return Get().GraphicsQueue; }
		static int GetCurrentFrame() { return Get().CurrentFrame; }
		static int GetFrameCount() { return Get().FrameCount; }
		static void SetCurrentFrame(int frameNumber) { Get().CurrentFrame = frameNumber; }
		static const VkCommandBuffer& GetCurrentCommandBuffer() { return Get().CommandBuffers[GetCurrentFrame()]; }
		static const VkFramebuffer& GetCurrentFrameBuffer() { return Get().FrameBuffers[GetCurrentFrame()]; }
		static const VkRenderPass& GetRenderPass() { return Get().RenderPass; }
		static const VkFence& GetCurrentCommandBufferFence() { return Get().CommandBufferFences[GetCurrentFrame()]; }
		static const VkSemaphore& GetCurrentAcquireSemaphore() { return Get().AcquireSemaphores[GetCurrentFrame()]; }
		static const VkSemaphore& GetCurrentRenderCompleteSemaphore() { return Get().RenderCompleteSemaphores[GetCurrentFrame()]; }

		static void ImmediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function) { return Get().ImmediateSubmitImpl(std::move(function)); }
	public:
		GraphicsContext(GraphicsContext const&) = delete;
		void operator=(GraphicsContext const&) = delete;
	private:
		GraphicsContext() = default;

		void InitializeImpl();
		void DeinitializeImpl();
		void RecreateSwapChainImpl();
		void ImmediateSubmitImpl(std::function<void(VkCommandBuffer commandBuffer)>&& function);

	public:
		void CreateInstance();
		void DestroyInstance();
		void SetupDebugMessenger();
		void EnumeratePhysicalDevices();
		void SelectPhysicalDevice();
		void CreateLogicalDeviceAndQueues();
		void InitializeAllocator();
		void CreateSemaphores();
		void CreateCommandPools();
		void CreateCommandBuffers();
		void CreateSwapChain();
		VkFormat ChooseSupportedFormat(VkFormat* formats, int numFormats, VkImageTiling tiling, VkFormatFeatureFlags features);
		void CreateRenderTargets();
		void CreateRenderPass();
		void CreateFrameBuffers();

		void CleanupSwapChain();
	public:
		bool Initialized = false;
		int const FrameCount = 2;
		int CurrentFrame = 0;

		bool EnableValidationLayers = true;

		VkInstance Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT DebugMessenger = VK_NULL_HANDLE;

		GPUInfo* GPU = nullptr;
		VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
		VkDevice Device = VK_NULL_HANDLE;

		uint32_t GraphicsFamilyIndex;
		uint32_t PresentFamilyIndex;

		VkQueue GraphicsQueue = VK_NULL_HANDLE;
		VkQueue PresentQueue = VK_NULL_HANDLE;

		VkSurfaceKHR Surface = VK_NULL_HANDLE;

		VmaAllocator Allocator = VK_NULL_HANDLE;

		VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
		VkFormat SwapchainFormat = VK_FORMAT_UNDEFINED;

		VkPresentModeKHR PresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
		VkExtent2D SwapchainExtent = { 0 };

		std::vector<Ref<Image>> SwapchainImages;

		std::vector<VkSemaphore> AcquireSemaphores;
		std::vector<VkSemaphore> RenderCompleteSemaphores;

		std::vector<VkCommandPool> CommandPools;
		VkCommandPool UploadCommandPool;

		std::vector<VkCommandBuffer> CommandBuffers;
		std::vector<VkFence> CommandBufferFences;

		VkFence UploadFence;

		Ref<Image> DepthImage;

		VkRenderPass RenderPass = VK_NULL_HANDLE;

		std::vector<VkFramebuffer> FrameBuffers;

		VkApplicationInfo ApplicationInfo =
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Test app",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "Hazel",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_MAKE_VERSION(1, 2, VK_HEADER_VERSION)
		};

		VkDebugUtilsMessengerCreateInfoEXT DebugMessengerCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
			.pUserData = nullptr // Optional
		};

		// These are some features that are enabled for VkNeo
		// If you try to make an API call down the road which 
		// requires something be enabled, you'll more than likely
		// get a validation message telling you what to enable.
		// Thanks Vulkan!
		VkPhysicalDeviceFeatures DeviceFeatures =
		{
			.imageCubeArray = VK_TRUE,
			.depthClamp = VK_TRUE,
			.depthBiasClamp = VK_TRUE,
			.fillModeNonSolid = VK_TRUE,
			.depthBounds = VK_TRUE,
			.textureCompressionBC = VK_TRUE
		};

		std::vector<const char*> InstanceExtensions = { VK_KHR_MAINTENANCE1_EXTENSION_NAME };
		std::vector<const char*> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		std::vector<const char*> ValidationLayers = { "VK_LAYER_KHRONOS_validation" };

		std::vector<GPUInfo> GPUs;
	};
}