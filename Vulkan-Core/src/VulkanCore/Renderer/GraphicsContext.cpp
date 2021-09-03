#include "vkcpch.h"
#include "GraphicsContext.h"

#include "vk_mem_alloc.h"
#include "VulkanCore/Core/Application.h"
#include "VulkanCore/Utils/RendererUtils.h"

namespace VulkanCore {
	

	static bool CheckPhysicalDeviceExtensionSupport(GraphicsContext::GPUInfo* gpu, std::vector<const char*>& extensions)
	{
		bool supported = true;
		for (auto extension : extensions)
		{
			bool found = false;

			for (auto props : gpu->ExtensionProperties)
			{
				if (std::strcmp(props.extensionName, extension) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				supported = false;
				break;
			}
		}

		return supported;
	}

	static VkSurfaceFormatKHR ChooseSurfaceFormat(std::vector<VkSurfaceFormatKHR>& formats)
	{
		VkSurfaceFormatKHR result;

		// If Vulkan returned an unknown format, then just force what we want.
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) 
		{
			result.format = VK_FORMAT_B8G8R8A8_UNORM;
			result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			return result;
		}

		// Favor 32 bit rgba and srgb nonlinear colorspace
		for (int i = 0; i < formats.size(); ++i) 
		{
			VkSurfaceFormatKHR& fmt = formats[i];
			if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
			{
				return fmt;
			}
		}

		// If all else fails, just return what's available
		return formats[0];
	}

	static VkPresentModeKHR ChoosePresentMode(std::vector<VkPresentModeKHR>& modes)
	{
		const VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;

		// Favor looking for mailbox mode.
		for (int i = 0; i < modes.size(); ++i) 
		{
			if (modes[i] == desiredMode) 
			{
				return desiredMode;
			}
		}

		// If we couldn't find mailbox, then default to FIFO which is always available.
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static VkExtent2D ChooseSurfaceExtent(VkSurfaceCapabilitiesKHR& caps)
	{
		if (caps.currentExtent.width != UINT32_MAX) {
			return caps.currentExtent;
		}
		else {
			int width = Application::Get().GetWindow().GetFrameBufferWidth(),
				height = Application::Get().GetWindow().GetFrameBufferHeight();
			

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

			return actualExtent;
		}
	}

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	void GraphicsContext::InitializeImpl()
	{
		CreateInstance();
		SetupDebugMessenger();
		Application::Get().GetWindow().CreateSurface(m_Context.Instance, nullptr, &(m_Context.Surface));
		EnumeratePhysicalDevices();
		SelectPhysicalDevice();
		CreateLogicalDeviceAndQueues();
		InitializeAllocator();
		CreateSemaphores();
		CreateCommandPool();
		CreateCommandBuffers();
		CreateSwapChain();
		CreateRenderPass();
		CreateFrameBuffers();
	}

	void GraphicsContext::DeinitializeImpl()
	{
		vkFreeCommandBuffers(m_Context.Device, m_Context.CommandPool, (uint32_t)(m_Context.CommandBuffers.size()), m_Context.CommandBuffers.data());

		for (auto framebuffer : m_Context.FrameBuffers) {
			vkDestroyFramebuffer(m_Context.Device, framebuffer, nullptr);
		}

		vkDestroyRenderPass(m_Context.Device, m_Context.RenderPass, nullptr);

		for (auto image : m_Context.SwapchainImages)
		{
			vkDestroyImageView(m_Context.Device, image.View, nullptr);
		}

		vkDestroySwapchainKHR(m_Context.Device, m_Context.Swapchain, nullptr);

		for (auto fence : m_Context.CommandBufferFences)
		{
			vkDestroyFence(m_Context.Device, fence, nullptr);
		}

		vkDestroyCommandPool(m_Context.Device, m_Context.CommandPool, nullptr);

		for (int i = 0; i < m_Context.FrameCount; ++i)
		{
			vkDestroySemaphore(m_Context.Device, m_Context.AcquireSemaphores[i], nullptr);
			vkDestroySemaphore(m_Context.Device, m_Context.RenderCompleteSemaphores[i], nullptr);
		}

		vmaDestroyAllocator(m_Context.Allocator);

		vkDestroyDevice(m_Context.Device, nullptr);
		vkDestroySurfaceKHR(m_Context.Instance, m_Context.Surface, nullptr);

		if (m_Context.EnableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(m_Context.Instance, m_Context.DebugMessenger, nullptr);
		}

		DestroyInstance();
	}

	void GraphicsContext::RecreateSwapChainImpl()
	{
		CleanupSwapChain();

		CheckVKResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Context.GPU->Device, m_Context.Surface, &m_Context.GPU->SurfaceCapabilities));
		CreateSwapChain();
		CreateRenderPass();
		CreateFrameBuffers();
		CreateCommandBuffers();
	}

	void GraphicsContext::CreateInstance()
	{
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &(m_Context.ApplicationInfo);

		if (m_Context.EnableValidationLayers) {
			m_Context.InstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);

		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		for (auto instanceExtension : m_Context.InstanceExtensions)
		{
			bool exists = false;
			for (const auto& extension : extensions) 
			{
				if (std::strcmp(instanceExtension, extension.extensionName) == 0)
				{
					exists = true;
				}
			}

			VKC_ASSERT(exists, "Extension not supported");
		}

		if (m_Context.EnableValidationLayers)
		{
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			for (auto validationLayer : m_Context.ValidationLayers)
			{
				bool exists = false;
				for (const auto& layer : availableLayers)
				{
					if (std::strcmp(validationLayer, layer.layerName) == 0)
					{
						exists = true;
					}
				}

				VKC_ASSERT(exists, "Extension not supported");
			}

			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&m_Context.DebugMessengerCreateInfo;
		}
		else
		{
			m_Context.ValidationLayers.clear();
		}

		// Give all the extensions/layers to the create info.
		createInfo.enabledExtensionCount = (uint32_t)m_Context.InstanceExtensions.size();
		createInfo.ppEnabledExtensionNames = m_Context.InstanceExtensions.data();
		createInfo.enabledLayerCount = (uint32_t)m_Context.ValidationLayers.size();
		createInfo.ppEnabledLayerNames = m_Context.ValidationLayers.data();

		CheckVKResult(vkCreateInstance(&createInfo, nullptr, &m_Context.Instance));
	}

	void GraphicsContext::DestroyInstance()
	{
		vkDestroyInstance(m_Context.Instance, nullptr);
	}

	void GraphicsContext::SetupDebugMessenger()
	{
		if (!m_Context.EnableValidationLayers) return;
		
		CheckVKResult(CreateDebugUtilsMessengerEXT(m_Context.Instance, &m_Context.DebugMessengerCreateInfo, nullptr, &m_Context.DebugMessenger));
	}

	void GraphicsContext::EnumeratePhysicalDevices()
	{
		// CheckVKResult and VKC_ASSERT are simply macros for checking return values,
		// and then taking action if necessary.
	
		// First just get the number of devices.
		uint32_t numDevices = 0;
		CheckVKResult(vkEnumeratePhysicalDevices(m_Context.Instance, &numDevices, nullptr));
		VKC_ASSERT(numDevices > 0, "vkEnumeratePhysicalDevices returned zero devices.")

		std::vector<VkPhysicalDevice> devices(numDevices);

		// Now get the actual devices
		CheckVKResult(vkEnumeratePhysicalDevices(m_Context.Instance, &numDevices, devices.data()));
		VKC_ASSERT(numDevices > 0, "vkEnumeratePhysicalDevices returned zero devices.")

		// GPU is a VkNeo struct which stores details about the physical device.
		// We'll use various API calls to get the necessary information.
		m_Context.GPUs.resize(numDevices);

		for (uint32_t i = 0; i < numDevices; ++i) 
		{
			GPUInfo& gpu = m_Context.GPUs[i];
			gpu.Device = devices[i];

			{
				// First let's get the Queues from the device.  
				// DON'T WORRY I'll explain this in a bit.
				uint32_t numQueues = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(gpu.Device, &numQueues, nullptr);
				VKC_ASSERT(numQueues > 0, "vkGetPhysicalDeviceQueueFamilyProperties returned zero queues.");

				gpu.QueueFamilyProperties.resize(numQueues);
				vkGetPhysicalDeviceQueueFamilyProperties(gpu.Device, &numQueues, gpu.QueueFamilyProperties.data());
				VKC_ASSERT(numQueues > 0, "vkGetPhysicalDeviceQueueFamilyProperties returned zero queues.");
			}

			{
				// Next let's get the extensions supported by the device.
				uint32_t numExtension;
				CheckVKResult(vkEnumerateDeviceExtensionProperties(gpu.Device, nullptr, &numExtension, nullptr));
				VKC_ASSERT(numExtension > 0, "vkEnumerateDeviceExtensionProperties returned zero extensions.");

				gpu.ExtensionProperties.resize(numExtension);
				CheckVKResult(vkEnumerateDeviceExtensionProperties(gpu.Device, nullptr, &numExtension, gpu.ExtensionProperties.data()));
				VKC_ASSERT(numExtension > 0, "vkEnumerateDeviceExtensionProperties returned zero extensions.");
			}

			// Surface capabilities basically describes what kind of image you can render to the user.
			// Look up VkSurfaceCapabilitiesKHR in the Vulkan documentation.
			CheckVKResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.Device, m_Context.Surface, &gpu.SurfaceCapabilities));

			{
				// Get the supported surface formats.  This includes image format and color space.
				// A common format is VK_FORMAT_R8G8B8A8_UNORM which is 8 bits for red, green, blue, alpha making for 32 total
				uint32_t numFormats;
				CheckVKResult(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.Device, m_Context.Surface, &numFormats, nullptr));
				VKC_ASSERT(numFormats > 0, "vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats.");

				gpu.SurfaceFormats.resize(numFormats);
				CheckVKResult(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.Device, m_Context.Surface, &numFormats, gpu.SurfaceFormats.data()));
				VKC_ASSERT(numFormats > 0, "vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats.");
			}

			{
				// Vulkan supports multiple presentation modes, and I'll linkn to some good documentation on that in just a bit.
				uint32_t numPresentModes;
				CheckVKResult(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.Device, m_Context.Surface, &numPresentModes, nullptr));
				VKC_ASSERT(numPresentModes > 0, "vkGetPhysicalDeviceSurfacePresentModesKHR returned zero present modes.");

				gpu.PresentModes.resize(numPresentModes);
				CheckVKResult(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.Device, m_Context.Surface, &numPresentModes, gpu.PresentModes.data()));
				VKC_ASSERT(numPresentModes > 0, "vkGetPhysicalDeviceSurfacePresentModesKHR returned zero present modes.");
			}

			// Almost done! Up next wee get the memory types supported by the device.
			// This will be needed later once we start allocating memory for buffers, images, etc.
			vkGetPhysicalDeviceMemoryProperties(gpu.Device, &gpu.MemoryProperties);

			// Lastly we get the actual device properties.
			// Of note this includes a MASSIVE struct (VkPhysicalDeviceLimits) which outlines 
			// all possible limits you could run into when attemptin to render.
			vkGetPhysicalDeviceProperties(gpu.Device, &gpu.DeviceProperties);
		}
	}

	void GraphicsContext::SelectPhysicalDevice()
	{
		// Let's pick a GPU!
		for (uint32_t i = 0; i < m_Context.GPUs.size(); i++)
		{
			GPUInfo* gpu = &(m_Context.GPUs[i]);
			// This is again related to queues.  Don't worry I'll get there soon.
			int graphicsIdx = -1;
			int presentIdx = -1;

			// Remember when we created our instance we got all those device extensions?
			// Now we need to make sure our physical device supports them.
			if (!CheckPhysicalDeviceExtensionSupport(gpu, m_Context.DeviceExtensions)) 
			{
				continue;
			}

			// No surface formats? =(
			if (gpu->SurfaceFormats.empty()) 
			{
				continue;
			}

			// No present modes? =(
			if (gpu->PresentModes.empty()) 
			{
				continue;
			}

			// Now we'll loop through the queue family properties looking
			// for both a graphics and a present queue.
			// The index could actually end up being the same, and from 
			// my experience they are.  But via the spec, you're not 
			// guaranteed that luxury.  So best be on the safe side.

			// Find graphics queue family
			for (uint32_t j = 0; j < gpu->QueueFamilyProperties.size(); ++j) 
			{
				VkQueueFamilyProperties& props = gpu->QueueFamilyProperties[j];

				if (props.queueCount == 0) 
				{
					continue;
				}

				if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
				{
					// Got it!
					graphicsIdx = j;
					break;
				}
			}

			// Find present queue family
			for (int j = 0; j < gpu->QueueFamilyProperties.size(); ++j) 
			{
				VkQueueFamilyProperties& props = gpu->QueueFamilyProperties[j];

				if (props.queueCount == 0) 
				{
					continue;
				}

				// A rather perplexing call in the Vulkan API, but
				// it is a necessity to call.
				VkBool32 supportsPresent = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(gpu->Device, j, m_Context.Surface, &supportsPresent);
				if (supportsPresent) 
				{
					// Got it!
					presentIdx = j;
					break;
				}
			}

			// Did we find a device supporting both graphics and present.
			if (graphicsIdx >= 0 && presentIdx >= 0) 
			{
				m_Context.GraphicsFamilyIndex = (uint32_t)graphicsIdx;
				m_Context.PresentFamilyIndex = (uint32_t)presentIdx;
				m_Context.PhysicalDevice = gpu->Device;
				m_Context.GPU = gpu;
				return;
			}
		}

		// If we can't render or present, just bail.
		// DIAF
		VKC_CORE_ERROR("Could not find a physical device which fits our desired profile");
		VKC_CORE_ASSERT(false)
	}

	void GraphicsContext::CreateLogicalDeviceAndQueues()
	{
		// Add each family index to a list.
		// Don't do duplicates
		std::vector<uint32_t> uniqueIdx;
		uniqueIdx.push_back(m_Context.GraphicsFamilyIndex);
		if (std::find(uniqueIdx.begin(), uniqueIdx.end(), m_Context.PresentFamilyIndex) == uniqueIdx.end()) {
			uniqueIdx.push_back(m_Context.PresentFamilyIndex);
		}

		std::vector<VkDeviceQueueCreateInfo> devqInfo;

		const float priority = 1.0f;
		for (int i = 0; i < uniqueIdx.size(); ++i) 
		{
			VkDeviceQueueCreateInfo qinfo = {};
			qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qinfo.queueFamilyIndex = uniqueIdx[i];
			qinfo.queueCount = 1;

			// Don't worry about priority
			qinfo.pQueuePriorities = &priority;

			devqInfo.push_back(qinfo);
		}

		// Put it all together.
		VkDeviceCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		info.queueCreateInfoCount = (uint32_t)devqInfo.size();
		info.pQueueCreateInfos = devqInfo.data();
		info.pEnabledFeatures = &m_Context.DeviceFeatures;
		info.enabledExtensionCount = (uint32_t)m_Context.DeviceExtensions.size();
		info.ppEnabledExtensionNames = m_Context.DeviceExtensions.data();

		// If validation layers are enabled supply them here.
		if (m_Context.EnableValidationLayers) {
			info.enabledLayerCount = (uint32_t)m_Context.ValidationLayers.size();
			info.ppEnabledLayerNames = m_Context.ValidationLayers.data();
		}
		else {
			info.enabledLayerCount = 0;
		}

		// Create the device
		CheckVKResult(vkCreateDevice(m_Context.PhysicalDevice, &info, nullptr, &m_Context.Device));

		// Now get the queues from the devie we just created.
		vkGetDeviceQueue(m_Context.Device, m_Context.GraphicsFamilyIndex, 0, &m_Context.GraphicsQueue);
		vkGetDeviceQueue(m_Context.Device, m_Context.PresentFamilyIndex, 0, &m_Context.PresentQueue);
	}

	void GraphicsContext::InitializeAllocator()
	{
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
		allocatorInfo.physicalDevice = m_Context.PhysicalDevice;
		allocatorInfo.device = m_Context.Device;
		allocatorInfo.instance = m_Context.Instance;

		vmaCreateAllocator(&allocatorInfo, &m_Context.Allocator);
	}

	void GraphicsContext::CreateSemaphores()
	{
		m_Context.AcquireSemaphores.resize(m_Context.FrameCount);
		m_Context.RenderCompleteSemaphores.resize(m_Context.FrameCount);

		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		for (int i = 0; i < m_Context.FrameCount; ++i) {
			CheckVKResult(vkCreateSemaphore(m_Context.Device, &semaphoreCreateInfo, nullptr, &(m_Context.AcquireSemaphores[i])));
			CheckVKResult(vkCreateSemaphore(m_Context.Device, &semaphoreCreateInfo, nullptr, &(m_Context.RenderCompleteSemaphores[i])));
		}
	}

	void GraphicsContext::CreateCommandPool()
	{
		// Because command buffers can be very flexible, we don't want to be 
		// doing a lot of allocation while we're trying to render.
		// For this reason we create a pool to hold allocated command buffers.
		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

		// This allows the command buffer to be implicitly reset when vkBeginCommandBuffer is called.
		// You can also explicitly call vkResetCommandBuffer.  
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		// We'll be building command buffers to send to the graphics queue
		commandPoolCreateInfo.queueFamilyIndex = m_Context.GraphicsFamilyIndex;

		CheckVKResult(vkCreateCommandPool(m_Context.Device, &commandPoolCreateInfo, nullptr, &m_Context.CommandPool));
	}

	void GraphicsContext::CreateCommandBuffers()
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

		// Don't worry about this
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		// The command pool we created above
		commandBufferAllocateInfo.commandPool = m_Context.CommandPool;

		// We'll have two command buffers.  One will be in flight
		// while the other is being built.
		commandBufferAllocateInfo.commandBufferCount = m_Context.FrameCount;

		m_Context.CommandBuffers.resize(m_Context.FrameCount);

		// You can allocate multiple command buffers at once.
		CheckVKResult(vkAllocateCommandBuffers(m_Context.Device, &commandBufferAllocateInfo, m_Context.CommandBuffers.data()));

		// We create fences that we can use to wait for a 
		// given command buffer to be done on the GPU.
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		m_Context.CommandBufferFences.resize(m_Context.FrameCount);
		for (int i = 0; i < m_Context.FrameCount; ++i)
		{
			CheckVKResult(vkCreateFence(m_Context.Device, &fenceCreateInfo, nullptr, &m_Context.CommandBufferFences[i]));
		}
	}

	void GraphicsContext::CreateSwapChain()
	{
		GPUInfo& gpu = *m_Context.GPU;

		// Take our selected gpu and pick three things.
		// 1.) Surface format as described earlier.
		// 2.) Present mode. Again refer to documentation I shared.
		// 3.) Surface extent is basically just the size ( width, height ) of the image.
		VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(gpu.SurfaceFormats);
		VkPresentModeKHR presentMode = ChoosePresentMode(gpu.PresentModes);
		VkExtent2D extent = ChooseSurfaceExtent(gpu.SurfaceCapabilities);

		VkSwapchainCreateInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.surface = m_Context.Surface;

		// double buffer again!
		info.minImageCount = m_Context.FrameCount;

		info.imageFormat = surfaceFormat.format;
		info.imageColorSpace = surfaceFormat.colorSpace;
		info.imageExtent = extent;
		info.imageArrayLayers = 1;

		// Aha! Something new.  There are only 8 potential bits that can be used here
		// and I'm using two.  Essentially this is what they mean.
		// VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT - This is a color image I'm rendering into.
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT - I'll be copying this image somewhere. ( screenshot, postprocess )
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		// Moment of truth.  If the graphics queue family and present family don't match
		// then we need to create the swapchain with different information.
		if (m_Context.GraphicsFamilyIndex != m_Context.PresentFamilyIndex) {
			uint32_t indices[] = { m_Context.GraphicsFamilyIndex, m_Context.PresentFamilyIndex };

			// There are only two sharing modes.  This is the one to use
			// if images are not exclusive to one queue.
			info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			info.queueFamilyIndexCount = 2;
			info.pQueueFamilyIndices = indices;
		}
		else {
			// If the indices are the same, then the queue can have exclusive
			// access to the images.
			info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		// We just want to leave the image as is.
		info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.presentMode = presentMode;

		// Is Vulkan allowed to discard operations outside of the renderable space?
		info.clipped = VK_TRUE;

		info.oldSwapchain = m_Context.Swapchain;

		// Create the swapchain
		CheckVKResult(vkCreateSwapchainKHR(m_Context.Device, &info, nullptr, &m_Context.Swapchain));

		// Save off swapchain details
		m_Context.SwapchainFormat = surfaceFormat.format;
		m_Context.PresentMode = presentMode;
		m_Context.SwapchainExtent = extent;

		// Retrieve the swapchain images from the device.
		// Note that VkImage is simply a handle like everything else.

		// First call gets numImages.
		uint32_t numImages = 0;
		std::vector<VkImage> swapchainImages(m_Context.FrameCount);
		CheckVKResult(vkGetSwapchainImagesKHR(m_Context.Device, m_Context.Swapchain, &numImages, nullptr));
		VKC_ASSERT(numImages > 0, "vkGetSwapchainImagesKHR returned a zero image count.")

		// Second call uses numImages
		CheckVKResult(vkGetSwapchainImagesKHR(m_Context.Device, m_Context.Swapchain, &numImages, swapchainImages.data()));
		VKC_ASSERT(numImages > 0, "vkGetSwapchainImagesKHR returned a zero image count.");

		m_Context.SwapchainImages.resize(m_Context.FrameCount);

		// New concept - Image Views
		// Much like the logical device is an interface to the physical device,
		// image views are interfaces to actual images.  Think of it as this.
		// The image exists outside of you.  But the view is your personal view 
		// ( how you perceive ) the image.
		for (int i = 0; i < m_Context.FrameCount; ++i) {
			VkImageViewCreateInfo imageViewCreateInfo = {};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

			// Just plug it in
			imageViewCreateInfo.image = swapchainImages[i];

			// These are 2D images
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

			// The selected format
			imageViewCreateInfo.format = m_Context.SwapchainFormat;

			// We don't need to swizzle ( swap around ) any of the 
			// color channels
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;

			// There are only 4x aspect bits.  And most people will only use 3x.
			// These determine what is affected by your image operations.
			// VK_IMAGE_ASPECT_COLOR_BIT
			// VK_IMAGE_ASPECT_DEPTH_BIT
			// VK_IMAGE_ASPECT_STENCIL_BIT
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			// For beginners - a base mip level of zero is par for the course.
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;

			// Level count is the # of images visible down the mip chain.
			// So basically just 1...
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			// We don't have multiple layers to these images.
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.flags = 0;

			// Create the view
			VkImageView imageView;
			CheckVKResult(vkCreateImageView(m_Context.Device, &imageViewCreateInfo, nullptr, &imageView));

			// Now store this off in an idImage so we can take advantage
			// of that class's API
			VKImage image(
				swapchainImages[i],
				imageView,
				m_Context.SwapchainFormat,
				m_Context.SwapchainExtent
			);
			image.IsSwapChainImage = true;
			m_Context.SwapchainImages[i] = image;
		}
	}

	VkFormat GraphicsContext::ChooseSupportedFormat(VkFormat* formats, int numFormats, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (int i = 0; i < numFormats; ++i)
		{
			VkFormat format = formats[i];

			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_Context.PhysicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) 
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) 
			{
				return format;
			}
		}

		VKC_CORE_ERROR("Failed to find a supported format.");

		return VK_FORMAT_UNDEFINED;
	}

	void GraphicsContext::CreateRenderTargets()
	{
		// Select Depth Format, prefer as high a precision as we can get.
		{
			VkFormat formats[] = {
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT
			};

			// Make sure to check it supports optimal tiling and is a depth/stencil format.
			m_Context.DepthFormat = ChooseSupportedFormat(
					formats, 2,
					VK_IMAGE_TILING_OPTIMAL,
					VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		}

		// idTech4.5 does have an independent idea of a depth attachment
		// So now that the context contains the selected format we can simply
		// create the internal one.

		m_Context.DepthImage.Options.Format = TextureFormat::FORMAT_DEPTH;
		m_Context.DepthImage.Options.Width = Application::Get().GetWindow().GetFrameBufferWidth();
		m_Context.DepthImage.Options.Height = Application::Get().GetWindow().GetFrameBufferHeight();
		m_Context.DepthImage.Options.LevelCount = 1;
	}

	void GraphicsContext::CreateRenderPass()
	{
		std::vector<VkAttachmentDescription> attachments;

		// VkNeo uses a single renderpass, so I just create it on startup.
		// Attachments act as slots in which to insert images.

		// For the color attachment, we'll simply be using the swapchain images.
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_Context.SwapchainFormat;
		// Sample count goes from 1 - 64
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// I don't care what you do with the image memory when you load it for use.
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		// Just store the image when you go to store it.
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// I don't care what the initial layout of the image is.
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// It better be ready to present to the user when we're done with the renderpass.
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachments.push_back(colorAttachment);

		// For the depth attachment, we'll be using the _viewDepth we just created.
		// VkAttachmentDescription depthAttachment = {};
		// depthAttachment.format = m_Context.DepthFormat;
		// depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		// depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		// attachments.push_back(depthAttachment);

		// Now we enumerate the attachments for a subpass.  We have to have at least one subpass.
		VkAttachmentReference colorRef = {};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// VkAttachmentReference depthRef = {};
		// depthRef.attachment = 1;
		// depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Basically is this graphics or compute
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		// subpass.pDepthStencilAttachment = &depthRef;

		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = (uint32_t)attachments.size();
		renderPassCreateInfo.pAttachments = attachments.data();
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;
		renderPassCreateInfo.dependencyCount = 0;

		CheckVKResult(vkCreateRenderPass(m_Context.Device, &renderPassCreateInfo, nullptr, &m_Context.RenderPass));
	}

	void GraphicsContext::CreateFrameBuffers()
	{
		m_Context.FrameBuffers.resize(m_Context.FrameCount);

		VkImageView attachments[1];

		// Depth attachment is the same
		// We never show the depth buffer, so we only ever need one.
		// idImage* depthImg = globalImages->GetImage("_viewDepth");
		// if (depthImg == NULL) {
		// 	idLib::FatalError("CreateFrameBuffers: No _viewDepth image.");
		// }
		//
		// attachments[1] = depthImg->GetView();

		// VkFrameBuffer is what maps attachments to a renderpass.  That's really all it is.
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		// The renderpass we just created.
		frameBufferCreateInfo.renderPass = m_Context.RenderPass;
		// The color and depth attachments
		frameBufferCreateInfo.attachmentCount = 1;
		frameBufferCreateInfo.pAttachments = attachments;
		// Current render size
		frameBufferCreateInfo.width = m_Context.SwapchainExtent.width;
		frameBufferCreateInfo.height = m_Context.SwapchainExtent.height;
		frameBufferCreateInfo.layers = 1;

		// Because we're double buffering, we need to create the same number of framebuffers.
		// The main difference again is that both of them use the same depth image view.
		for (int i = 0; i < m_Context.FrameCount; ++i) {
			attachments[0] = m_Context.SwapchainImages[i].View;
			CheckVKResult(vkCreateFramebuffer(m_Context.Device, &frameBufferCreateInfo, NULL, &m_Context.FrameBuffers[i]));
		}
	}

	void GraphicsContext::CleanupSwapChain()
	{
		for (size_t i = 0; i < m_Context.FrameBuffers.size(); i++) {
			vkDestroyFramebuffer(m_Context.Device, m_Context.FrameBuffers[i], nullptr);
		}

		vkFreeCommandBuffers(m_Context.Device, m_Context.CommandPool, (uint32_t)(m_Context.CommandBuffers.size()), m_Context.CommandBuffers.data());
		for (auto fence : m_Context.CommandBufferFences)
		{
			vkDestroyFence(m_Context.Device, fence, nullptr);
		}

		vkDestroyRenderPass(m_Context.Device, m_Context.RenderPass, nullptr);

		for (size_t i = 0; i < m_Context.SwapchainImages.size(); i++) {
			vkDestroyImageView(m_Context.Device, m_Context.SwapchainImages[i].View, nullptr);
		}

		// vkDestroySwapchainKHR(m_Context.Device, m_Context.Swapchain, nullptr);
	}
}
