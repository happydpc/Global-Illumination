#include "SceneManager.h"

#include <utility>

SceneManager::~SceneManager()
{
}

bool SceneManager::initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<Image*> swapChainImages)
{
	m_loadingManager.initialize(device, physicalDevice, surface, std::move(swapChainImages));

	return true;
}

void SceneManager::submit(VkDevice device, VkQueue graphicsQueue, uint32_t swapChainImageIndex, Semaphore imageAvailableSemaphore)
{
    m_loadingManager.submit(device, graphicsQueue, swapChainImageIndex, imageAvailableSemaphore);
}

VkSemaphore SceneManager::getLastRenderFinishedSemaphore()
{
    return m_loadingManager.getLastRenderFinishedSemaphore();
}
