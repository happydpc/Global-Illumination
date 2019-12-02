#include "SceneManager.h"

SceneManager::~SceneManager()
{

}

void SceneManager::load(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, VkQueue computeQueue, VkSurfaceKHR surface,
        std::mutex * graphicsQueueMutex,  std::vector<Image*> swapChainImages)
{
	// Command Pool + Descriptor Pool
    m_commandPool.initialize(device, physicalDevice, surface);
	m_descriptorPool.initialize(device);

	// Camera
	m_camera.initialize(glm::vec3(1.4f, 1.2f, 0.3f), glm::vec3(2.0f, 0.9f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.01f, 5.0f, 
		swapChainImages[0]->getExtent().width / (float)swapChainImages[0]->getExtent().height);

	// Model
    m_model.loadFromFile(device, physicalDevice, m_commandPool.getCommandPool(), graphicsQueue, graphicsQueueMutex,
            "Models/sponza/sponza.obj", "Models/sponza");

	// GBuffer
	glm::mat4 mvp = m_camera.getProjection() * m_camera.getViewMatrix() * m_model.getTransformation();
    m_gbuffer.initialize(device, physicalDevice, surface, m_commandPool.getCommandPool(), m_descriptorPool.getDescriptorPool(), swapChainImages[0]->getExtent(), &m_model, mvp);

	// Compute pass
	m_finalResultTexture.create(device, physicalDevice, swapChainImages[0]->getExtent(), VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	graphicsQueueMutex->lock();
	m_finalResultTexture.setImageLayout(device, m_commandPool.getCommandPool(), graphicsQueue, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	graphicsQueueMutex->unlock();

	m_finalResultTexture.createSampler(device, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0f, VK_FILTER_LINEAR);

	std::vector<Image*> gbufferImages = m_gbuffer.getImages();
	m_gbufferTextures.resize(gbufferImages.size());
	for (int i(0); i < gbufferImages.size(); ++i)
	{
		m_gbufferTextures[i].createFromImage(device, gbufferImages[i]);
		m_gbufferTextures[i].createSampler(device, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0f, VK_FILTER_LINEAR);
	}

	std::vector<std::pair<Texture*, TextureLayout>> texturesForComputePass;
	for (int i(0); i < gbufferImages.size(); ++i)
	{
		TextureLayout textureLayout;
		textureLayout.accessibility = VK_SHADER_STAGE_COMPUTE_BIT;
		textureLayout.binding = i;

		texturesForComputePass.push_back({ &m_gbufferTextures[i], textureLayout});
	}

	TextureLayout textureLayout;
	textureLayout.accessibility = VK_SHADER_STAGE_COMPUTE_BIT;
	textureLayout.binding = gbufferImages.size();
	texturesForComputePass.push_back({ &m_finalResultTexture, textureLayout });

	graphicsQueueMutex->lock();
	m_computePassFinalRender.initialize(device, physicalDevice, m_commandPool.getCommandPool(), m_descriptorPool.getDescriptorPool(), graphicsQueue, computeQueue, swapChainImages[0]->getExtent(),
		{ 16, 16, 1 }, "Shaders/compute/comp.spv", {}, texturesForComputePass);
	graphicsQueueMutex->unlock();

    m_loadingState = 1.0f;
}

void SceneManager::submit(VkDevice device, GLFWwindow* window, VkQueue graphicsQueue, VkQueue computeQueue, uint32_t swapChainImageIndex, Semaphore * imageAvailableSemaphore)
{
	m_camera.update(window);
	m_gbuffer.submit(device, graphicsQueue);
	m_computePassFinalRender.submit(device, computeQueue, { m_gbuffer.getRenderCompleteSemaphore(), imageAvailableSemaphore });
}

void SceneManager::cleanup(VkDevice device)
{
	m_model.cleanup(device);
	m_gbuffer.cleanup(device, m_commandPool.getCommandPool(), m_descriptorPool.getDescriptorPool());
	m_commandPool.cleanup(device);
	m_descriptorPool.cleanup(device);
}
