#include "DeferredExample.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <Hog/ImGui/ImGuiHelper.h>

static auto& context = GraphicsContext::Get();

DeferredExample::DeferredExample()
	: Layer("DeferredExample")
{

}

void DeferredExample::OnAttach()
{
	HG_PROFILE_FUNCTION();
	CVarSystem::Get()->SetIntCVar("application.enableImGui", 0);
	CVarSystem::Get()->SetIntCVar("renderer.enableMipMapping", 1);
	CVarSystem::Get()->SetStringCVar("shader.compilation.macros", "MATERIAL_ARRAY_SIZE=128;TEXTURE_ARRAY_SIZE=512");
	CVarSystem::Get()->SetIntCVar("material.array.size", 128);

	GraphicsContext::Initialize();

	HG_PROFILE_GPU_INIT_VULKAN(&(context.Device), &(context.PhysicalDevice), &(context.Queue), &(context.QueueFamilyIndex), 1, nullptr);

	// LoadGltfFile("assets/models/sponza-intel/NewSponza_Main_Blender_glTF.gltf", m_OpaqueMeshes, m_TransparentMeshes, m_Cameras, m_Textures, m_Materials, m_MaterialBuffer);
	LoadGltfFile("assets/models/sponza/sponza.gltf", m_OpaqueMeshes, m_TransparentMeshes, m_Cameras, m_Textures, m_Materials, m_MaterialBuffer);
	// LoadGltfFile("assets/models/cube/cube.gltf", m_OpaqueMeshes, m_TransparentMeshes, m_Cameras, m_Textures, m_Materials, m_MaterialBuffer);

	Ref<Texture> albedoAttachment = Texture::Create({}, Image::Create(ImageDescription::Defaults::SampledColorAttachment, 1));
	Ref<Texture> positionAttachment = Texture::Create({}, Image::Create(ImageDescription::Defaults::SampledPositionAttachment, 1));
	Ref<Texture> normalAttachment = Texture::Create({}, Image::Create(ImageDescription::Defaults::SampledNormalAttachment, 1));
	Ref<Texture> depthAttachment = Texture::Create({}, Image::Create(ImageDescription::Defaults::Depth, 1));

	Ref<Texture> colorAttachment = Texture::Create({}, Image::Create(ImageDescription::Defaults::SampledSwapchainColorAttachment, 1));

	m_ViewProjection = Buffer::Create(BufferDescription::Defaults::UniformBuffer, sizeof(glm::mat4));

	RenderGraph graph;
	auto gbuffer = graph.AddStage(nullptr, {
		"GBuffer", Shader::Create("GBuffer", "GBuffer.vertex", "GBuffer.fragment"), RendererStageType::ForwardGraphics,
		{
			{DataType::Defaults::Float3, "a_Position"},
			{DataType::Defaults::Float2, "a_TexCoords"},
			{DataType::Defaults::Float3, "a_Normal"},
			{DataType::Defaults::Float4, "a_Tangent"},
			{DataType::Defaults::Int, "a_MaterialIndex"},
		},
		{
			{"u_ViewProjection", ResourceType::Uniform, ShaderType::Defaults::Vertex, m_ViewProjection, 0, 0},
			{"u_Materials", ResourceType::Uniform, ShaderType::Defaults::Fragment, m_MaterialBuffer, 1, 0},
			{"u_Textures", ResourceType::SamplerArray, ShaderType::Defaults::Fragment, m_Textures, 2, 0, 512},
			{"p_Model", ResourceType::PushConstant, ShaderType::Defaults::Vertex, sizeof(PushConstant), &m_PushConstant},
		},
		m_OpaqueMeshes,
		{
			{"Position", AttachmentType::Color, positionAttachment->GetImage(), true, {ImageLayout::ColorAttachmentOptimal, ImageLayout::ShaderReadOnlyOptimal}},
			{"Normal", AttachmentType::Color, normalAttachment->GetImage(), true, {ImageLayout::ColorAttachmentOptimal, ImageLayout::ShaderReadOnlyOptimal}},
			{"Albedo", AttachmentType::Color, albedoAttachment->GetImage(), true, {ImageLayout::ColorAttachmentOptimal, ImageLayout::ShaderReadOnlyOptimal}},
			{"Depth", AttachmentType::Depth, depthAttachment->GetImage(), true, {ImageLayout::DepthStencilAttachmentOptimal, ImageLayout::DepthStencilAttachmentOptimal}},
		},
	});

	auto lightingPass = graph.AddStage(gbuffer, {
		"Lighting stage", Shader::Create("Lighting", "fullscreen.vertex", "Lighting.fragment"), RendererStageType::ScreenSpacePass,
		{
			{"u_Position", ResourceType::Sampler, ShaderType::Defaults::Fragment, positionAttachment, 0, 0},
			{"u_Normal", ResourceType::Sampler, ShaderType::Defaults::Fragment, normalAttachment, 1, 0},
			{"u_Albedo", ResourceType::Sampler, ShaderType::Defaults::Fragment, albedoAttachment, 2, 0},
		},
		{
			{"Color", AttachmentType::Color, colorAttachment->GetImage(), true, {ImageLayout::ColorAttachmentOptimal, ImageLayout::ShaderReadOnlyOptimal}},
		},
		});

	//auto imGuiStage = graph.AddStage(graphics, {
	//	"ImGuiStage", RendererStageType::ImGui, {
	//		{"ColorTarget", AttachmentType::Color, colorAttachment, false, {
	//			PipelineStage::ColorAttachmentOutput, AccessFlag::ColorAttachmentWrite,
	//			PipelineStage::ColorAttachmentOutput, AccessFlag::ColorAttachmentRead,
	//			ImageLayout::ColorAttachmentOptimal,
	//			ImageLayout::ShaderReadOnlyOptimal
	//		}},
	//	}
	//});

	graph.AddStage(gbuffer, {
		"BlitStage", Shader::Create("Blit", "fullscreen.vertex", "blit.fragment", false), RendererStageType::Blit,
		{{"FinalRender", ResourceType::Sampler, ShaderType::Defaults::Fragment, colorAttachment, 0, 0, {
				PipelineStage::ColorAttachmentOutput, AccessFlag::ColorAttachmentWrite,
				PipelineStage::FragmentShader, AccessFlag::ShaderSampledRead,
		}},},
		{{"SwapchainImage", AttachmentType::Swapchain, true, {ImageLayout::ColorAttachmentOptimal, ImageLayout::PresentSrcKHR}},},
	});

	Renderer::Initialize(graph);

	m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 10000.0f);
}

void DeferredExample::OnDetach()
{
	HG_PROFILE_FUNCTION()

	GraphicsContext::WaitIdle();

	Renderer::Cleanup();

	m_OpaqueMeshes.clear();
	m_TransparentMeshes.clear();
	m_Textures.clear();
	m_Materials.clear();
	m_MaterialBuffer.reset();
	m_ViewProjection.reset();

	GraphicsContext::Deinitialize();
}

void DeferredExample::OnUpdate(Timestep ts)
{
	HG_PROFILE_FUNCTION();

	m_EditorCamera.OnUpdate(ts);
	glm::mat4 viewProj = m_Cameras.begin()->second;
	m_ViewProjection->WriteData(&viewProj, sizeof(viewProj));
}

void DeferredExample::OnImGuiRender()
{
	ImGui::ShowDemoWindow();
}

void DeferredExample::OnEvent(Event& e)
{
	m_EditorCamera.OnEvent(e);

	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<FrameBufferResizeEvent>(HG_BIND_EVENT_FN(DeferredExample::OnResized));
}

bool DeferredExample::OnResized(FrameBufferResizeEvent& e)
{
	GraphicsContext::RecreateSwapChain();

	m_EditorCamera.SetViewportSize((float)e.GetWidth(), (float)e.GetHeight());

	return false;
}