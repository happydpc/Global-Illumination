#include "GameManager.h"

GameManager::~GameManager()
{
}

bool GameManager::initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkQueue graphicsQueue, std::mutex* graphicsQueueMutex,
	VkQueue computeQueue, std::mutex* computeQueueMutex, std::vector<Image*> swapChainImages, HardwareCapabilities hardwareCapabilities)
{
	m_loadingManager.initialize(device, physicalDevice, surface, graphicsQueue, swapChainImages);
    m_sceneLoadingThread = std::thread(&SceneManager::load, &m_sceneManager, device, physicalDevice, graphicsQueue, computeQueue, surface, graphicsQueueMutex, swapChainImages, hardwareCapabilities);

	return true;
}

void GameManager::submit(VkDevice device, VkPhysicalDevice physicalDevice, GLFWwindow* window, VkQueue graphicsQueue, std::mutex * graphicsQueueMutex,
	VkQueue computeQueue, std::mutex* computeQueueMutex, uint32_t swapChainImageIndex, Semaphore * imageAvailableSemaphore)
{
    if(m_gameState == GAME_STATE::LOADING)
    {
        if(m_sceneManager.getLoadingState() == 1.0f)
        {
            m_gameState = GAME_STATE::RUNNING;
            m_sceneLoadingThread.join();
            m_sceneManager.submit(device, physicalDevice, window, graphicsQueue, computeQueue, swapChainImageIndex, imageAvailableSemaphore);
        }
        else
        {
            graphicsQueueMutex->lock();
            m_loadingManager.submit(device, graphicsQueue, swapChainImageIndex, imageAvailableSemaphore);
            graphicsQueueMutex->unlock();
        }
    }
    else if(m_gameState == GAME_STATE::RUNNING)
    {
		m_sceneManager.submit(device, physicalDevice, window, graphicsQueue, computeQueue, swapChainImageIndex, imageAvailableSemaphore);
    }
}

void GameManager::resize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, VkQueue computeQueue, std::vector<Image*> swapChainImages)
{
	if (m_gameState == GAME_STATE::LOADING)
		m_loadingManager.resize(device, physicalDevice, swapChainImages);
	else if (m_gameState == GAME_STATE::RUNNING)
		m_sceneManager.resize(device, physicalDevice, graphicsQueue, computeQueue, swapChainImages);
}

void GameManager::cleanup(VkDevice device)
{
	m_loadingManager.cleanup(device);
	m_sceneManager.cleanup(device);
}

VkSemaphore GameManager::getLastRenderFinishedSemaphore()
{
	if (m_gameState == GAME_STATE::LOADING)
		return m_loadingManager.getLastRenderFinishedSemaphore();

	return m_sceneManager.getLastRenderFinishedSemaphore();
}
