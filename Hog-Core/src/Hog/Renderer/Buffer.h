#pragma once

#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

namespace Hog {

	enum class BufferType
	{
		CPUWritableVertexBuffer,
		GPUOnlyVertexBuffer,
		TransferSourceBuffer,
		CPUWritableIndexBuffer,
		UniformBuffer,
		ReadbackStorageBuffer,
	};

	static VmaMemoryUsage BufferTypeToVmaMemoryUsage(BufferType type);

	static VmaAllocationCreateFlags BufferTypeToVmaAllocationCreateFlags(BufferType type);

	static VkBufferUsageFlags BufferTypeToVkBufferUsageFlags(BufferType type);

	VkDescriptorType BufferTypeToVkDescriptorType(BufferType type);
	
	static VkSharingMode BufferTypeToVkSharingMode(BufferType type);

	static bool IsPersistentlyMapped(BufferType type);

	static bool IsTypeGPUOnly(BufferType type);

	enum class DataType
	{
		None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool,
		Depth32, Depth32Stencil8, Depth24Stencil8, RGBA8, BGRA8
	};

	inline static VkFormat DataTypeToVkFormat(DataType type)
	{
		switch (type)
		{
			case DataType::Float:            return VK_FORMAT_R32_SFLOAT;
			case DataType::Float2:           return VK_FORMAT_R32G32_SFLOAT;
			case DataType::Float3:           return VK_FORMAT_R32G32B32_SFLOAT;
			case DataType::Float4:           return VK_FORMAT_R32G32B32A32_SFLOAT;
			case DataType::Mat3:             return VK_FORMAT_R32G32B32_SFLOAT;
			case DataType::Mat4:             return VK_FORMAT_R32G32B32A32_SFLOAT;
			case DataType::Int:              return VK_FORMAT_R32_SINT;
			case DataType::Int2:             return VK_FORMAT_R32G32_SINT;
			case DataType::Int3:             return VK_FORMAT_R32G32B32_SINT;
			case DataType::Int4:             return VK_FORMAT_R32G32B32A32_SINT;
			case DataType::Bool:             return VK_FORMAT_R8_UINT;
			case DataType::Depth32:          return VK_FORMAT_D32_SFLOAT;
			case DataType::Depth24Stencil8:  return VK_FORMAT_D24_UNORM_S8_UINT;
			case DataType::Depth32Stencil8:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
			case DataType::BGRA8:            return VK_FORMAT_B8G8R8A8_UNORM;
			case DataType::RGBA8:            return VK_FORMAT_R8G8B8A8_SRGB;
		}

		HG_CORE_ASSERT(false, "Unknown DataType!");
		return VK_FORMAT_UNDEFINED;
	}

	inline static DataType VkFormatToDataType(VkFormat format)
	{
		switch (format)
		{
			case VK_FORMAT_R32_SFLOAT:          return DataType::Float;
			case VK_FORMAT_R32G32_SFLOAT:       return DataType::Float2;
			case VK_FORMAT_R32G32B32_SFLOAT:    return DataType::Float3;
			case VK_FORMAT_R32G32B32A32_SFLOAT: return DataType::Float4;
			case VK_FORMAT_R32_SINT:            return DataType::Int;
			case VK_FORMAT_R32G32_SINT:         return DataType::Int2;
			case VK_FORMAT_R32G32B32_SINT:      return DataType::Int3;
			case VK_FORMAT_R32G32B32A32_SINT:   return DataType::Int4;
			case VK_FORMAT_R8_UINT:             return DataType::Bool;
			case VK_FORMAT_D32_SFLOAT:          return DataType::Depth32;
			case VK_FORMAT_D24_UNORM_S8_UINT:   return DataType::Depth24Stencil8;
			case VK_FORMAT_D32_SFLOAT_S8_UINT:  return DataType::Depth32Stencil8;
			case VK_FORMAT_B8G8R8A8_SRGB:      return DataType::BGRA8;
			case VK_FORMAT_R8G8B8A8_SRGB:         return DataType::RGBA8;
		}

		HG_CORE_ASSERT(false, "Unknown VkFormat!");
		return DataType::None;
	}

	static uint32_t ShaderDataTypeSize(DataType type)
	{
		switch (type)
		{
			case DataType::Float:    return 4;
			case DataType::Float2:   return 4 * 2;
			case DataType::Float3:   return 4 * 3;
			case DataType::Float4:   return 4 * 4;
			case DataType::Mat3:     return 4 * 3 * 3;
			case DataType::Mat4:     return 4 * 4 * 4;
			case DataType::Int:      return 4;
			case DataType::Int2:     return 4 * 2;
			case DataType::Int3:     return 4 * 3;
			case DataType::Int4:     return 4 * 4;
			case DataType::Bool:     return 1;
		}

		HG_CORE_ASSERT(false, "Unknown DataType!");
		return 0;
	}

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		int32_t MaterialIndex;
	};

	class Buffer
	{
	public:
		static Ref<Buffer> Create(BufferType type, uint32_t size);
	public:
		Buffer(BufferType type, uint32_t size);
		virtual ~Buffer();

		void SetData(void* data, uint32_t size);
		void TransferData(uint32_t size, const Ref<Buffer>& src);
		const VkBuffer& GetHandle() const { return m_Handle; }
		uint32_t GetSize() const { return m_Size; }
		BufferType GetBufferType() const { return m_Type; }
		void LockAfterWrite(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage);
		void LockBeforeRead(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage);

		operator void* () { return m_AllocationInfo.pMappedData; }
	private:
		VkBuffer m_Handle;
		VmaAllocation m_Allocation;
		VmaAllocationInfo m_AllocationInfo;

		VkBufferCreateInfo m_BufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};

		VmaAllocationCreateInfo m_AllocationCreateInfo = {};

		BufferType m_Type;
		uint32_t m_Size;
	};

	class VertexBuffer : public Buffer
	{
	public:
		static Ref<VertexBuffer> Create(uint32_t size);
	public:
		VertexBuffer(uint32_t size);
		~VertexBuffer() = default;
	private:
	};
}
