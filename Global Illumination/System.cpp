#include "System.h"

System::System()
{
}

System::~System()
{
	//cleanup();
}

void System::initialize()
{
	create();
}

bool System::mainLoop()
{
	while (!glfwWindowShouldClose(m_vk.getWindow()))
	{
		glfwPollEvents();

		// Event processing
		if (glfwGetKey(m_vk.getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS && m_oldEscapeState == GLFW_RELEASE)
		{
			m_swapChainRenderPass.setDrawMenu(&m_vk, m_swapChainRenderPass.getDrawMenu() ? false : true);
			m_camera.setFixed(m_swapChainRenderPass.getDrawMenu());
		}

		if (m_swapChainRenderPass.getDrawMenu())
			m_menu.update(&m_vk, m_vk.getSwapChainExtend().width, m_vk.getSwapChainExtend().height);

		// Key update
		m_oldEscapeState = glfwGetKey(m_vk.getWindow(), GLFW_KEY_ESCAPE);

		m_camera.update(m_vk.getWindow());

		m_uboVPData.view = m_camera.getViewMatrix();
		m_uboVP.update(&m_vk, m_uboVPData);

		if (m_usedEffects & EFFECT_TYPE_CASCADED_SHADOW_MAPPING)
		{
			m_uboDirLightCSMData.camPos = glm::vec4(m_camera.getPosition(), 1.0f);
			m_uboDirLightCSM.update(&m_vk, m_uboDirLightCSMData);
			updateCSM();
		}

		static auto startTime = std::chrono::steady_clock::now();

		auto currentTime = std::chrono::steady_clock::now();
		m_fpsCount++;
		if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_startTimeFPSCounter).count() > 1000.0f)
		{
			std::wstring text = L"FPS : " + std::to_wstring(m_fpsCount);
			m_text.changeText(&m_vk, text, m_fpsCounterTextID);

			m_fpsCount = 0;
			m_startTimeFPSCounter = currentTime;

			/*std::cout << m_camera.getPosition().x << " " << m_camera.getPosition().y << " " << m_camera.getPosition().z << std::endl;
			std::cout << m_camera.getTarget().x << " " << m_camera.getTarget().y << " " << m_camera.getTarget().z << std::endl;*/
		}

		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

		m_uboDirLightData.camPos = glm::vec4(m_camera.getPosition(), 1.0f);
		m_uboDirLight.update(&m_vk, m_uboDirLightData);

		if (m_usedEffects & EFFECT_TYPE_CASCADED_SHADOW_MAPPING) 
		{
			updateCSM();
			m_offscreenCascadedShadowMap.drawCall(&m_vk);
			m_offscreenShadowCalculation.drawCall(&m_vk);
			for (int i(0); i < m_blurAmount; ++i)
			{
				m_offscreenShadowBlurHorizontal[i].drawCall(&m_vk);
				m_offscreenShadowBlurVertical[i].drawCall(&m_vk);
			}
		}
		if (m_usedEffects & EFFECT_TYPE_RSM)
		{
			m_offscreenRSM.drawCall(&m_vk);
		}
		/*else if (m_usedEffects == (EFFECT_TYPE_CASCADED_SHADOW_MAPPING | EFFECT_TYPE_RSM))
		{
			updateCSM();
			m_offscreenCascadedShadowMap.drawCall(&m_vk);
			m_offscreenShadowCalculation.drawCall(&m_vk);
			for (int i(0); i < m_blurAmount; ++i)
			{
				m_offscreenShadowBlurHorizontal[i].drawCall(&m_vk);
				m_offscreenShadowBlurVertical[i].drawCall(&m_vk);
			}
			m_offscreenRSM.drawCall(&m_vk);
		}*/
		m_swapChainRenderPass.drawCall(&m_vk);
	}

	vkDeviceWaitIdle(m_vk.getDevice());

	return true;
}

void System::cleanup()
{
	std::cout << "Cleanup..." << std::endl;

	vkQueueWaitIdle(m_vk.getGraphicalQueue());

	//m_skybox.cleanup(m_vk.getDevice());
	//m_wall.cleanup(m_vk.getDevice());
	m_sponza.cleanup(m_vk.getDevice());
	m_quad.cleanup(m_vk.getDevice());

	m_swapChainRenderPass.cleanup(&m_vk);
	m_offscreenCascadedShadowMap.cleanup(&m_vk);
	m_offscreenShadowCalculation.cleanup(&m_vk);
	for (int i(0); i < m_offscreenShadowBlurHorizontal.size() /* don't clean if vector empty */; ++i)
	{
		m_offscreenShadowBlurHorizontal[i].cleanup(m_vk.getDevice());
		m_offscreenShadowBlurVertical[i].cleanup(m_vk.getDevice());
	}

	/*for (int cascade(0); cascade < m_cascadeCount; ++cascade)
	{
		vkFreeMemory(m_vk.getDevice(), m_shadowMapBuffers[cascade].second, nullptr);
		vkDestroyBuffer(m_vk.getDevice(), m_shadowMapBuffers[cascade].first, nullptr);
	}*/

	m_vk.cleanup();
}

void System::setMenuOptionImageView(VkImageView imageView)
{
	m_swapChainRenderPass.updateImageViewMenuItemOption(&m_vk, imageView);
}

void System::changePCF(bool status)
{
	m_uboDirLightData.usePCF = status ? 1.0f : 0.0f;
	m_uboDirLight.update(&m_vk, m_uboDirLightData);

	m_swapChainRenderPass.updateImageViewMenuItemOption(&m_vk, m_menu.getOptionImageView());
}

void System::drawFPSCounter(bool status)
{
	m_swapChainRenderPass.setDrawText(&m_vk, status);

	m_swapChainRenderPass.updateImageViewMenuItemOption(&m_vk, m_menu.getOptionImageView());
}

void System::changeShadows(std::wstring value)
{
	if (value == L"No")
		m_usedEffects = m_usedEffects & ~EFFECT_TYPE_CASCADED_SHADOW_MAPPING;
	else if (value == L"CSM")
		m_usedEffects |= EFFECT_TYPE_CASCADED_SHADOW_MAPPING;

	createUniformBufferObjects();
	createPasses(true);
	setSemaphores();

	m_swapChainRenderPass.updateImageViewMenuItemOption(&m_vk, m_menu.getOptionImageView());
}

void System::changeGlobalIllumination(std::wstring value)
{
	if (value == L"No")
	{
		m_uboDirLightData.ambient = 0.0f;
		m_uboDirLightCSMData.ambient = 0.0f;

		m_usedEffects = m_usedEffects & ~EFFECT_TYPE_RADIOSITY_PROBES;
	}
	else if (value == L"Ambient Lightning")
	{
		m_uboDirLightData.ambient = 0.2f;
		m_uboDirLightCSMData.ambient = 0.2f;

		m_usedEffects = m_usedEffects & ~EFFECT_TYPE_RADIOSITY_PROBES;
	}
	else if (value == L"Radiosity Probes")
	{
		m_usedEffects |= EFFECT_TYPE_RADIOSITY_PROBES;
	}

	m_uboDirLight.update(&m_vk, m_uboDirLightData);
	m_uboDirLightCSM.update(&m_vk, m_uboDirLightCSMData);

	createUniformBufferObjects();
	createPasses(true);
	setSemaphores();
}

void System::changeMSAA(std::wstring value)
{
	if (value == L"No")
		m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	else if (value == L"2x")
		m_msaaSamples = VK_SAMPLE_COUNT_2_BIT;
	else if (value == L"4x")
		m_msaaSamples = VK_SAMPLE_COUNT_4_BIT;
	else if (value == L"8x")
		m_msaaSamples = VK_SAMPLE_COUNT_8_BIT;

	create(true);
}

void System::changeReflectiveShadowMap(bool status)
{
	if (status)
		m_usedEffects |= EFFECT_TYPE_RSM;
	else
		m_usedEffects &= ~EFFECT_TYPE_RSM;

	createUniformBufferObjects();
	createPasses(true);
	setSemaphores();
}

void System::create(bool recreate)
{
	m_vk.initialize(1280, 720, "Vulkan Demo", recreateCallback, (void*)this, recreate);

	if (!recreate)
	{
		createRessources();
		m_camera.initialize(glm::vec3(1.4f, 1.2f, 0.3f), glm::vec3(2.0f, 0.9f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.01f, 5.0f, m_vk.getSwapChainExtend().width / (float)m_vk.getSwapChainExtend().height);

		m_usedEffects = 0;
	}
	else
		m_camera.setAspect(m_vk.getSwapChainExtend().width / (float)m_vk.getSwapChainExtend().height);

	createUniformBufferObjects();
	createPasses(recreate);
	setSemaphores(); // be sure all passes semaphore are initialized
}

void System::createRessources()
{
	m_text.initialize(&m_vk, 48, "Fonts/arial.ttf");
	m_fpsCounterTextID = m_text.addText(&m_vk, L"FPS : 0", glm::vec2(-0.99f, 0.85f), 0.065f, glm::vec3(1.0f));

	m_menu.initialize(&m_vk, "Fonts/arial.ttf", setMenuOptionImageViewCallback, this);
	m_menu.addBooleanItem(&m_vk, L"FPS Counter", drawFPSCounterCallback, true, this, { "", "" });
	int shadowsItemID = m_menu.addPicklistItem(&m_vk, L"Shadows", changeShadowsCallback, this, 0, { L"No", L"CSM" });
	int pcfItemID = m_menu.addBooleanItem(&m_vk, L"Percentage Closer Filtering", changePCFCallback, true, this, { "Image_options/shadow_no_pcf.JPG", "Image_options/shadow_with_pcf.JPG" });
	m_menu.addPicklistItem(&m_vk, L"MSAA", changeMSAACallback, this, 0, { L"No", L"2x", L"4x", L"8x" });
	m_menu.addPicklistItem(&m_vk, L"Global Illumination", changeGlobalIlluminationCallback, this, 0, { L"No", L"Ambient Lightning", L"Radiosity Probes" });
	m_menu.addBooleanItem(&m_vk, L"Reflective shadow Map", changeReflectiveShadowMapCallback, false, this, { "", "" });

	m_menu.addDependency(MENU_ITEM_TYPE_PICKLIST, shadowsItemID, MENU_ITEM_TYPE_BOOLEAN, pcfItemID, { 1 });

	m_sponza.loadObj(&m_vk, "Models/sponza/sponza.obj", "Models/sponza");

	//m_wall.loadObj(&m_vk, "Models/sponza/sponza.obj");
	//m_wall.createTextureSampler(&m_vk, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	m_linearSamplerNoMipMap.create(&m_vk, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0f, VK_FILTER_LINEAR);
	m_nearestSamplerNoMipMap.create(&m_vk, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0f, VK_FILTER_NEAREST);

	m_sphere.loadObj(&m_vk, "Models/sphere.obj");
}

void System::createPasses(bool recreate)
{
	if (recreate)
	{
		m_swapChainRenderPass.cleanup(&m_vk);

		m_uboVPData.proj = m_camera.getProjection(); // change aspect

		//m_swapChainRenderPass.initialize(&m_vk, { { 0, 0 } }, true, msaaSamples);
	}

	{
		/* Light + VP final pass */
		{
			m_swapChainRenderPass.initialize(&m_vk, { m_vk.getSwapChainExtend() }, true, m_msaaSamples, { m_vk.getSwapChainImageFormat() }, m_vk.findDepthFormat(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}

		/* Cascade Shadow Map */
		if (m_usedEffects & EFFECT_TYPE_CASCADED_SHADOW_MAPPING)
		{
			/* Shadow maps pass */
			if (!m_offscreenCascadedShadowMap.getIsInitialized())
			{
				m_offscreenCascadedShadowMap.initialize(&m_vk, m_shadowMapExtents,
					false, VK_SAMPLE_COUNT_1_BIT, {}, { VK_FORMAT_D16_UNORM }, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

				PipelineShaders offscreenShadowMap;
				offscreenShadowMap.vertexShader = "Shaders/pbr_csm_textured/shadowmap/vert.spv";
				//offscreenShadowMap.fragmentShader = "Shaders/pbr_csm_textured/shadowmap/frag.spv";
				for (int cascade(0); cascade < m_cascadeCount; ++cascade)
				{
					m_offscreenCascadedShadowMap.addMesh(&m_vk, { { m_sponza.getMeshes(), { &m_uboVPCSM[cascade], &m_uboModel } } }, offscreenShadowMap, 0, false, cascade);
				}

				m_offscreenCascadedShadowMap.recordDraw(&m_vk);
			}
			else
			{
				m_offscreenShadowCalculation.cleanup(&m_vk);
				for (int i(0); i < m_blurAmount; ++i)
				{
					m_offscreenShadowBlurHorizontal[i].cleanup(m_vk.getDevice());
					m_offscreenShadowBlurVertical[i].cleanup(m_vk.getDevice());
				}
			}

			/* Shadow Calculation pass */
			m_offscreenShadowCalculation.initialize(&m_vk, { m_vk.getSwapChainExtend() }, false, VK_SAMPLE_COUNT_1_BIT, { m_vk.getSwapChainImageFormat() }, m_vk.findDepthFormat(),
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			PipelineShaders shadowCalculation;
			shadowCalculation.vertexShader = "Shaders/pbr_csm_textured/shadow_calculation/vert.spv";
			shadowCalculation.fragmentShader = "Shaders/pbr_csm_textured/shadow_calculation/frag.spv";

			std::vector<VkSampler> depthMapSamplers;
			for (int i(0); i < m_cascadeCount; ++i)
				depthMapSamplers.push_back(m_linearSamplerNoMipMap.getSampler());
			m_offscreenShadowCalculation.addMesh(&m_vk,
				{
					{
						m_sponza.getMeshes(), { &m_uboVP, &m_uboModel, &m_uboLightSpaceCSM, &m_uboCascadeSplits },
						nullptr,
						{
							{ m_offscreenCascadedShadowMap.getFrameBuffer(0).depthImage.getImageView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL },
							{ m_offscreenCascadedShadowMap.getFrameBuffer(1).depthImage.getImageView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL },
							{ m_offscreenCascadedShadowMap.getFrameBuffer(2).depthImage.getImageView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL },
							{ m_offscreenCascadedShadowMap.getFrameBuffer(3).depthImage.getImageView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL }
						},
						depthMapSamplers
					}
				},
				shadowCalculation, m_cascadeCount, false
			);

			Operation blitOperation;
			blitOperation.type = OPERATION_TYPE_BLIT;
			VkExtent2D shadowScreenDownscale = { m_vk.getSwapChainExtend().width , m_vk.getSwapChainExtend().height };
			m_shadowScreenImage.create(&m_vk, shadowScreenDownscale, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			m_shadowScreenImage.transitionImageLayout(&m_vk, VK_IMAGE_LAYOUT_GENERAL);
			blitOperation.dstImages = { m_shadowScreenImage.getImage() };
			blitOperation.dstBlitExtent = { shadowScreenDownscale };

			m_offscreenShadowCalculation.recordDraw(&m_vk, { blitOperation });

			/* Blur */
			m_offscreenShadowBlurHorizontal.resize(m_blurAmount);
			m_offscreenShadowBlurVertical.resize(m_blurAmount);
			m_offscreenShadowBlurHorizontal[0].initialize(&m_vk, shadowScreenDownscale, { 16, 16, 1 }, "Shaders/pbr_csm_textured/blur/horizontal/comp.spv", m_shadowScreenImage.getImageView());
			m_offscreenShadowBlurVertical[0].initialize(&m_vk, shadowScreenDownscale, { 16, 16, 1 }, "Shaders/pbr_csm_textured/blur/vertical/comp.spv", m_offscreenShadowBlurHorizontal[0].getImageView());
			for (int i(1); i < m_blurAmount; ++i)
			{
				m_offscreenShadowBlurHorizontal[i].initialize(&m_vk, shadowScreenDownscale, { 16, 16, 1 }, "Shaders/pbr_csm_textured/blur/horizontal/comp.spv",
					m_offscreenShadowBlurVertical[i - 1].getImageView());
				m_offscreenShadowBlurVertical[i].initialize(&m_vk, shadowScreenDownscale, { 16, 16, 1 }, "Shaders/pbr_csm_textured/blur/vertical/comp.spv",
					m_offscreenShadowBlurHorizontal[i].getImageView());
			}
		}

		/* Reflective Shadow Map */
		if (m_usedEffects & EFFECT_TYPE_RSM)
		{
			m_offscreenRSM.initialize(&m_vk, { { 512, 512 } }, false, VK_SAMPLE_COUNT_1_BIT, {
				VK_FORMAT_R32G32B32A32_SFLOAT /* world space*/,
				VK_FORMAT_R32G32B32A32_SFLOAT /* normal */,
				VK_FORMAT_R32G32B32A32_SFLOAT /* flux */ },
				m_vk.findDepthFormat(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			MeshRender sponzaMeshes;
			sponzaMeshes.meshes = m_sponza.getMeshes();
			sponzaMeshes.ubos = { &m_uboVPRSM, &m_uboModel, &m_uboDirLight };
			for (int i(0); i < 5; ++i)
				sponzaMeshes.samplers.push_back(m_sponza.getMeshes()[0]->getSampler());

			PipelineShaders rsmRenderingPipeline;
			rsmRenderingPipeline.vertexShader = "Shaders/reflective_shadow_map/vert.spv";
			rsmRenderingPipeline.fragmentShader = "Shaders/reflective_shadow_map/frag.spv";

			m_offscreenRSM.addMesh(&m_vk, { sponzaMeshes }, rsmRenderingPipeline, 5, false);
			m_offscreenRSM.recordDraw(&m_vk);
		}

		/* Screen Space Ambient Occlusion */
		if (m_usedEffects & EFFECT_TYPE_SSAO)
		{
			
		}

		/* Radiosity probes */
		if (m_usedEffects & EFFECT_TYPE_RADIOSITY_PROBES && !(m_loadedEffects & EFFECT_TYPE_RADIOSITY_PROBES))
		{
			std::vector <ModelInstance> perInstance;
			for (int i(0); i < 10; ++i)
			{
				for (int j(0); j < 10; ++j)
				{
					for (int k(0); k < 10; ++k)
					{
						ModelInstance mi;
						mi.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(20.0f - i * 4.0f, 0.1f + j * 1.7f, -10.0f + k * 2.5f)), glm::vec3(0.01f));
						mi.id = 100 * i + 10 * j + k;

						perInstance.push_back(mi);
					}
				}
			}

			m_sphereInstance.load(&m_vk, sizeof(perInstance[0]) * perInstance.size(), perInstance.data());

			std::array<std::vector<float>, 2> probesIntensityPingPong;
			probesIntensityPingPong[0].resize(1000);
			probesIntensityPingPong[1].resize(1000);

			bool useDataInFileShadow = false;
			if(!useDataInFileShadow)
			{
                for (int i(0); i < 1000; ++i)
                {
                    glm::vec3 probePos = glm::vec3(20.0f - ((int)i / 100) * 4.0f, 0.1f + (((int)i / 10) % 10) * 1.7f, -10.0f + (i % 10) * 2.5f);
                    probePos /= 0.01f;

                    if(!m_sponza.checkIntersection(probePos, probePos - (30.0f * m_lightDir) / 0.01f))
                        probesIntensityPingPong[0][i] = 1.0f;
                    else
                        probesIntensityPingPong[0][i] = 0.0f;
                }

                std::ofstream probeDirectLightFile;
                probeDirectLightFile.open("Data/probesDirectLight.dat");
                for (int i(0); i < 1000; ++i)
                {
                    probeDirectLightFile << (probesIntensityPingPong[0][i] == 1.0f) << "\n";
                }
                probeDirectLightFile.close();
			}
			else
            {
                std::ifstream probeDirectLightFile("Data/probesDirectLight.dat");
                if (probeDirectLightFile.is_open())
                {
                    std::string line; int i(0);
                    while (std::getline(probeDirectLightFile, line))
                    {
                        probesIntensityPingPong[0][i] = line[0] == '1' ? 1.0f : 0.0f;
                        i++;
                    }
                }
            }

			bool useDataInFile = true;
			struct ProbeCollision
			{
				bool pX;
				bool lX;

				bool pY;
				bool lY;

				bool pZ;
				bool lZ;
			};
			std::vector<ProbeCollision> probeCollisions(1000);
			if (!useDataInFile)
			{
				for (int i(0); i < 1000; ++i)
				{
					int probeI = (int)i / 100, probeJ = ((int)i / 10) % 10, probeK = i % 10;
					glm::vec3 probePos = glm::vec3(20.0f - probeI * 4.0f, 0.1f + probeJ * 1.7f, -10.0f + probeK * 2.5f);
					probePos /= 0.01f;

					if (probeI == 0)
						probeCollisions[i].lX = true;
					else
						probeCollisions[i].lX = m_sponza.checkIntersection(probePos, probePos + glm::vec3(4.0f / 0.01f, 0.0f, 0.0f));
					if (probeI == 9)
						probeCollisions[i].pX = true;
					else
						probeCollisions[i].pX = m_sponza.checkIntersection(probePos, probePos - glm::vec3(4.0f / 0.01f, 0.0f, 0.0f));

					if (probeJ == 0)
						probeCollisions[i].lY = true;
					else
						probeCollisions[i].lY = m_sponza.checkIntersection(probePos, probePos - glm::vec3(0.0f, 1.7f / 0.01f, 0.0f));
					if (probeJ == 9)
						probeCollisions[i].pY = true;
					else
						probeCollisions[i].pY = m_sponza.checkIntersection(probePos, probePos + glm::vec3(0.0f, 1.7f / 0.01f, 0.0f));

					if (probeK == 0)
						probeCollisions[i].lZ = true;
					else
						probeCollisions[i].lZ = m_sponza.checkIntersection(probePos, probePos - glm::vec3(0.0f, 0.0f, 2.5f / 0.01f));
					if (probeK == 9)
						probeCollisions[i].pZ = true;
					else
						probeCollisions[i].pZ = m_sponza.checkIntersection(probePos, probePos + glm::vec3(0.0f, 0.0f, 2.5f / 0.01f));
				}

				std::ofstream probeCollisionsFile;
				probeCollisionsFile.open("Data/probesCollision.dat");
				for (int i(0); i < 1000; ++i)
				{
					probeCollisionsFile << probeCollisions[i].lX << "," << probeCollisions[i].pX << "," << probeCollisions[i].lY << "," << 
						probeCollisions[i].pY << "," << probeCollisions[i].lZ << "," << probeCollisions[i].pZ << "\n";
				}
				probeCollisionsFile.close();
			}
			else
			{
				std::ifstream probeCollisionsFile("Data/probesCollision.dat");
				if (probeCollisionsFile.is_open())
				{
					std::string line; int i(0);
					while (std::getline(probeCollisionsFile, line))
					{
						int valueNum = 0;
						for (int c(0); c < line.size(); ++c)
						{
							if (line[c] == ',')
								valueNum++;
							else
							{
								if (valueNum == 0)
									probeCollisions[i].lX = line[c] == '1';
								else if(valueNum == 1)
									probeCollisions[i].pX = line[c] == '1';
								else if (valueNum == 2)
									probeCollisions[i].lY = line[c] == '1';
								else if (valueNum == 3)
									probeCollisions[i].pY = line[c] == '1';
								else if (valueNum == 4)
									probeCollisions[i].lZ = line[c] == '1';
								else if (valueNum == 5)
									probeCollisions[i].pZ = line[c] == '1';
							}
						}

						i++;
					}
					probeCollisionsFile.close();
				}
			}

			float probeIntensityCoeffs[] = { 0.4f, 0.3f, 0.25f, 0.20f, 0.15f };
			for (int probePassCalculation(0); probePassCalculation < 5; ++probePassCalculation)
			{
				for (int i(0); i < 1000; ++i)
                {
					probesIntensityPingPong[(probePassCalculation + 1)% 2][i] = probesIntensityPingPong[probePassCalculation % 2][i];

					if (probesIntensityPingPong[probePassCalculation % 2][i] > 0.0f)
						continue;

					int probeI = (int)i / 100, probeJ = ((int)i / 10) % 10, probeK = i % 10;
					glm::vec3 probePos = glm::vec3(20.0f - probeI * 4.0f, 0.1f + probeJ * 1.7f, -10.0f + probeK * 2.5f);

					// X
					if (!probeCollisions[i].lX)
						probesIntensityPingPong[(probePassCalculation + 1) % 2][i] += 
							probesIntensityPingPong[probePassCalculation % 2][100 * (probeI - 1) + 10 * probeJ + probeK] * probeIntensityCoeffs[probePassCalculation];
					if (!probeCollisions[i].pX)
						probesIntensityPingPong[(probePassCalculation + 1) % 2][i] += 
							probesIntensityPingPong[probePassCalculation % 2][100 * (probeI + 1) + 10 * probeJ + probeK] * probeIntensityCoeffs[probePassCalculation];

					// Y
					if (!probeCollisions[i].lY)
						probesIntensityPingPong[(probePassCalculation + 1) % 2][i] += 
							probesIntensityPingPong[probePassCalculation % 2][100 * probeI + 10 * (probeJ - 1) + probeK] * probeIntensityCoeffs[probePassCalculation];
					if (!probeCollisions[i].pY)
						probesIntensityPingPong[(probePassCalculation + 1) % 2][i] += 
							probesIntensityPingPong[probePassCalculation % 2][100 * probeI + 10 * (probeJ + 1) + probeK] * probeIntensityCoeffs[probePassCalculation];

					// Z
					if (!probeCollisions[i].lZ)
						probesIntensityPingPong[(probePassCalculation + 1) % 2][i] += 
							probesIntensityPingPong[probePassCalculation % 2][100 * probeI + 10 * probeJ + probeK - 1] * probeIntensityCoeffs[probePassCalculation];
					if (!probeCollisions[i].pZ)
						probesIntensityPingPong[(probePassCalculation + 1) % 2][i] += 
							probesIntensityPingPong[probePassCalculation % 2][100 * probeI + 10 * probeJ + probeK + 1] * probeIntensityCoeffs[probePassCalculation];
				}
			}

			for (int i(0); i < 1000; ++i)
				m_uboRadiosityProbesData.values[i].x = probesIntensityPingPong[1][i];
			m_uboRadiosityProbes.update(&m_vk, m_uboRadiosityProbesData.getData(), m_uboRadiosityProbesData.getSize());

            m_loadedEffects |= EFFECT_TYPE_RADIOSITY_PROBES;
		} // end load radiosity probes
	}

	// Set on screen render pass shaders / meshes
	if (m_usedEffects == EFFECT_TYPE_CASCADED_SHADOW_MAPPING)
	{
		PipelineShaders pbrCsmTextured;
		pbrCsmTextured.vertexShader = "Shaders/pbr_csm_textured/vert.spv";
		pbrCsmTextured.fragmentShader = "Shaders/pbr_csm_textured/frag.spv";

		std::vector<VkSampler> samplers;
		for (int i(0); i < 5; ++i)
			samplers.push_back(m_sponza.getMeshes()[0]->getSampler());
		samplers.push_back(m_linearSamplerNoMipMap.getSampler()); // shadow mask

		m_swapChainRenderPass.addMesh(&m_vk, { { m_sponza.getMeshes(), { &m_uboVP, &m_uboModel, &m_uboDirLightCSM }, nullptr,
			{ { m_offscreenShadowBlurVertical[m_blurAmount - 1].getImageView(), VK_IMAGE_LAYOUT_GENERAL } }, samplers } },
			pbrCsmTextured, 6, true);
	}
	else if (m_usedEffects == 0)
	{
		PipelineShaders pbrNoShadowTextured;
		pbrNoShadowTextured.vertexShader = "Shaders/pbr_no_shadow_textured/vert.spv";
		pbrNoShadowTextured.fragmentShader = "Shaders/pbr_no_shadow_textured/frag.spv";
		MeshRender sponzaMeshes;
		sponzaMeshes.meshes = m_sponza.getMeshes();
		sponzaMeshes.ubos = { &m_uboVP, &m_uboModel, &m_uboDirLight };
		for (int i(0); i < 5; ++i)
			sponzaMeshes.samplers.push_back(m_sponza.getMeshes()[0]->getSampler());

		m_swapChainRenderPass.addMesh(&m_vk, { sponzaMeshes },
			pbrNoShadowTextured, 5);
	}
	else if (m_usedEffects == EFFECT_TYPE_RSM)
	{
		PipelineShaders pbrNoShadowTextured;
		pbrNoShadowTextured.vertexShader = "Shaders/pbr_no_shadow_rsm_textured/vert.spv";
		pbrNoShadowTextured.fragmentShader = "Shaders/pbr_no_shadow_rsm_textured/frag.spv";

		MeshRender meshesRender;
		meshesRender.meshes = m_sponza.getMeshes();
		meshesRender.ubos = { &m_uboVP, &m_uboModel, &m_uboProjRSM , &m_uboDirLight };
		meshesRender.instance = nullptr;
		meshesRender.images = {
			{ m_offscreenRSM.getFrameBuffer(0).colorImages[0].getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ m_offscreenRSM.getFrameBuffer(0).colorImages[1].getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ m_offscreenRSM.getFrameBuffer(0).colorImages[2].getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		};
		for (int i(0); i < 5; ++i)
			meshesRender.samplers.push_back(m_sponza.getMeshes()[0]->getSampler());
		for (int i(0); i < 3; ++i)
			meshesRender.samplers.push_back(m_nearestSamplerNoMipMap.getSampler());
		m_swapChainRenderPass.addMesh(&m_vk, { meshesRender }, pbrNoShadowTextured, 8);
	}
	else if (m_usedEffects == (EFFECT_TYPE_CASCADED_SHADOW_MAPPING | EFFECT_TYPE_RSM))
	{
		PipelineShaders pbrCsmRsmTextured;
		pbrCsmRsmTextured.vertexShader = "Shaders/pbr_csm_rsm_textured/vert.spv";
		pbrCsmRsmTextured.fragmentShader = "Shaders/pbr_csm_rsm_textured/frag.spv";

		MeshRender meshesRender;
		meshesRender.meshes = m_sponza.getMeshes();
		meshesRender.ubos = { &m_uboVP, &m_uboModel, &m_uboProjRSM , &m_uboDirLightCSM };
		meshesRender.instance = nullptr;
		meshesRender.images = {
			{ m_offscreenRSM.getFrameBuffer(0).colorImages[0].getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ m_offscreenRSM.getFrameBuffer(0).colorImages[1].getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ m_offscreenRSM.getFrameBuffer(0).colorImages[2].getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ m_offscreenShadowBlurVertical[m_blurAmount - 1].getImageView(), VK_IMAGE_LAYOUT_GENERAL }
		};
		for (int i(0); i < 5; ++i)
			meshesRender.samplers.push_back(m_sponza.getMeshes()[0]->getSampler());
		for (int i(0); i < 3; ++i)
			meshesRender.samplers.push_back(m_nearestSamplerNoMipMap.getSampler());
		meshesRender.samplers.push_back(m_linearSamplerNoMipMap.getSampler());
		m_swapChainRenderPass.addMesh(&m_vk, { meshesRender }, pbrCsmRsmTextured, 9);
	}

	else if (m_usedEffects == EFFECT_TYPE_RADIOSITY_PROBES)
	{
		PipelineShaders pbrNoShadowTextured;
		pbrNoShadowTextured.vertexShader = "Shaders/pbr_no_shadow_textured/vert.spv";
		pbrNoShadowTextured.fragmentShader = "Shaders/pbr_no_shadow_textured/frag.spv";

		MeshRender sponzaMeshes;
		sponzaMeshes.meshes = m_sponza.getMeshes();
		sponzaMeshes.ubos = { &m_uboVP, &m_uboModel, &m_uboDirLight };

		for (int i(0); i < 5; ++i)
			sponzaMeshes.samplers.push_back(m_sponza.getMeshes()[0]->getSampler());

		m_swapChainRenderPass.addMesh(&m_vk, { sponzaMeshes },
			pbrNoShadowTextured, 5);

		MeshRender sphereInstancedMeshes;
		sphereInstancedMeshes.meshes = { &m_sphere };
		sphereInstancedMeshes.ubos = { &m_uboVP, &m_uboDirLight, &m_uboRadiosityProbes };
		sphereInstancedMeshes.instance = &m_sphereInstance;
		m_swapChainRenderPass.addMeshInstanced(&m_vk, { sphereInstancedMeshes }, "Shaders/radiosity_probes/sphere/vert.spv", "Shaders/radiosity_probes/sphere/frag.spv", 0);
	}

	else if (m_usedEffects == (EFFECT_TYPE_CASCADED_SHADOW_MAPPING | EFFECT_TYPE_RADIOSITY_PROBES))
	{
		PipelineShaders pbrCsmTextured;
		pbrCsmTextured.vertexShader = "Shaders/pbr_csm_radiosity_probes/vert.spv";
		pbrCsmTextured.fragmentShader = "Shaders/pbr_csm_radiosity_probes/frag.spv";

		std::vector<VkSampler> samplers;
		for (int i(0); i < 5; ++i)
			samplers.push_back(m_sponza.getMeshes()[0]->getSampler());
		samplers.push_back(m_linearSamplerNoMipMap.getSampler()); // shadow mask

		m_swapChainRenderPass.addMesh(&m_vk, { { m_sponza.getMeshes(), { &m_uboVP, &m_uboModel, &m_uboDirLightCSM, &m_uboRadiosityProbes }, nullptr,
			{ { m_offscreenShadowBlurVertical[m_blurAmount - 1].getImageView(), VK_IMAGE_LAYOUT_GENERAL } }, samplers } },
			pbrCsmTextured, 6, true);
	}

	m_swapChainRenderPass.addMenu(&m_vk, &m_menu);
	m_swapChainRenderPass.addText(&m_vk, &m_text);

	// Record on screen render pass
	m_swapChainRenderPass.recordDraw(&m_vk);
}

void System::updateCSM()
{
	float lastSplitDist = m_camera.getNear();
	for (int cascade(0); cascade < m_cascadeCount; ++cascade)
	{
		float startCascade = lastSplitDist;
		float endCascade = m_cascadeSplits[cascade];

		//float ar = (float)m_vk.getSwapChainExtend().height / m_vk.getSwapChainExtend().width;
		//float tanHalfHFOV = glm::tan((m_camera.getFOV() * (1.0 / ar)) / 2.0f);
		//float tanHalfVFOV = glm::tan((m_camera.getFOV()) / 2.0f);
		//float cosHalfHFOV = glm::cos((m_camera.getFOV() * (1.0 / ar)) / 2.0f);

		//float xn = startCascade * tanHalfHFOV;
		//float xf = endCascade * tanHalfHFOV;
		//float yn = startCascade * tanHalfVFOV;
		//float yf = endCascade * tanHalfVFOV;

		//glm::vec4 frustumCorners[8] = 
		//{
		//	// near face
		//	glm::vec4(xn, yn, -startCascade * cosHalfHFOV, 1.0),
		//	glm::vec4(-xn, yn, -startCascade * cosHalfHFOV, 1.0),
		//	glm::vec4(xn, -yn, -startCascade * cosHalfHFOV, 1.0),
		//	glm::vec4(-xn, -yn, -startCascade * cosHalfHFOV, 1.0),

		//	// far face
		//	glm::vec4(xf, yf, -endCascade, 1.0),
		//	glm::vec4(-xf, yf, -endCascade, 1.0),
		//	glm::vec4(xf, -yf, -endCascade, 1.0),
		//	glm::vec4(-xf, -yf, -endCascade, 1.0)
		//};

		//glm::vec4 frustumCornersL[8];

		//float minX = std::numeric_limits<float>::max();
		//float maxX = std::numeric_limits<float>::min();
		//float minY = std::numeric_limits<float>::max();
		//float maxY = std::numeric_limits<float>::min();
		//float minZ = std::numeric_limits<float>::max();
		//float maxZ = std::numeric_limits<float>::min();

		////glm::vec3 frustumCenter = cascade == 0 ?
		////	/* Cascade == 0 */ m_camera.getPosition() + m_camera.getOrientation() * (m_cascadeSplits[cascade] / 2.0f) :
		////	/* Others */ m_camera.getPosition() + m_cascadeSplits[cascade - 1] * m_camera.getOrientation() + m_camera.getOrientation() * (m_cascadeSplits[cascade] / 2.0f);
		//glm::mat4 lightViewMatrix = glm::lookAt(-glm::normalize(m_lightDir) * 30.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		//for (unsigned int j = 0; j < 8; j++)
		//{
		//	// From view to world space
		//	glm::vec4 vW = glm::inverse(m_camera.getViewMatrix(/*glm::vec3(m_camera.getOrientation().x, 0.0f, m_camera.getOrientation().z)*/)) * frustumCorners[j];

		//	// From world to light space
		//	frustumCornersL[j] = lightViewMatrix * vW;

		//	minX = std::min(minX, frustumCornersL[j].x);
		//	maxX = std::max(maxX, frustumCornersL[j].x);
		//	minY = std::min(minY, frustumCornersL[j].y);
		//	maxY = std::max(maxY, frustumCornersL[j].y);
		//	minZ = std::min(minZ, frustumCornersL[j].z);
		//	maxZ = std::max(maxZ, frustumCornersL[j].z);
		//}

		//UniformBufferObjectVP vpTemp;
		//vpTemp.proj = glm::ortho(minX, maxX, minY, maxY, 0.1f, 100.0f);
		//vpTemp.view = lightViewMatrix;

		float radius = (endCascade - startCascade) / 2;

		float ar = (float)m_vk.getSwapChainExtend().height / m_vk.getSwapChainExtend().width;
		float cosHalfHFOV = glm::cos((m_camera.getFOV() * (1.0 / ar)) / 2.0f);
		double b = endCascade / cosHalfHFOV;
		radius = glm::sqrt(b * b + (startCascade + radius) * (startCascade + radius) - 2 * b * startCascade * cosHalfHFOV);

		float texelPerUnit = (float)m_shadowMapExtents[cascade].width / (radius * 2.0f);
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(texelPerUnit));
		glm::mat4 lookAt = scaleMat * glm::lookAt(glm::vec3(0.0f), -m_lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lookAtInv = glm::inverse(lookAt);

		glm::vec3 frustrumCenter = m_camera.getPosition() + startCascade * m_camera.getOrientation() + m_camera.getOrientation() * (endCascade / 2.0f);
		frustrumCenter = lookAt * glm::vec4(frustrumCenter, 1.0f);
		frustrumCenter.x = (float)floor(frustrumCenter.x);
		frustrumCenter.y = (float)floor(frustrumCenter.y);
		frustrumCenter = lookAtInv * glm::vec4(frustrumCenter, 1.0f);

		glm::mat4 lightViewMatrix = glm::lookAt(frustrumCenter - glm::normalize(m_lightDir), frustrumCenter, glm::vec3(0.0f, 1.0f, 0.0f));

		UniformBufferObjectVP vpTemp;
		vpTemp.proj = glm::ortho(-radius, radius, -radius, radius, -30.0f * 6.0f, 30.0f * 6.0f);
		vpTemp.view = lightViewMatrix;

		m_uboVPCSMData[cascade] = vpTemp;
		m_uboVPCSM[cascade].update(&m_vk, m_uboVPCSMData[cascade]);

		lastSplitDist += m_cascadeSplits[cascade];
	}

	for (int i = 0; i < m_cascadeCount; ++i)
	{
		m_uboLightSpaceCSMData.matrices[i] = m_uboVPCSMData[i].proj * m_uboVPCSMData[i].view * m_uboModelData.matrix;
	}

	m_uboLightSpaceCSM.update(&m_vk, m_uboLightSpaceCSMData.getData(), m_uboLightSpaceCSMData.getSize());
}

void System::createUniformBufferObjects()
{
	/* In all types */
	if(!(m_loadedEffects & EFFECT_TYPE_PBR))
	{
		m_uboModelData.matrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)); // reduce sponza size
		m_uboModel.load(&m_vk, m_uboModelData, VK_SHADER_STAGE_VERTEX_BIT);

		m_uboDirLightData.camPos = glm::vec4(m_camera.getPosition(), 1.0f);
		m_uboDirLightData.colorLight = glm::vec4(10.0f);
		m_uboDirLightData.dirLight = glm::vec4(m_lightDir, 0.0f);
		m_uboDirLightData.usePCF = 1.0f;
		m_uboDirLightData.ambient = 0.0f;
		m_uboDirLight.load(&m_vk, m_uboDirLightData, VK_SHADER_STAGE_FRAGMENT_BIT);

		m_uboDirLightCSMData.camPos = m_uboDirLightData.camPos;
		m_uboDirLightCSMData.colorLight = m_uboDirLightData.colorLight;
		m_uboDirLightCSMData.dirLight = m_uboDirLightData.dirLight;
		m_uboDirLightCSMData.ambient = m_uboDirLightData.ambient;
		//m_uboDirLightCSMData.cascadeSplits = std::vector<float>(m_cascadeCount);
		m_uboDirLightCSM.load(&m_vk, m_uboDirLightCSMData, VK_SHADER_STAGE_FRAGMENT_BIT);

		m_uboVPData.proj = m_camera.getProjection();
		m_uboVPData.view = m_camera.getViewMatrix();
		m_uboVP.load(&m_vk, m_uboVPData, VK_SHADER_STAGE_VERTEX_BIT);

        m_loadedEffects |= EFFECT_TYPE_PBR;
	}

	if (m_usedEffects & EFFECT_TYPE_CASCADED_SHADOW_MAPPING)
	{
		m_uboVPCSMData.resize(m_cascadeCount);
		m_uboVPCSM.resize(m_cascadeCount);

		//m_cascadeSplits.resize(m_cascadeCount);
		float near = m_camera.getNear();
		float far = 32.0f; // we don't render shadows on all the range
		for (float i(1.0f / m_cascadeCount); i <= 1.0f; i += 1.0f / m_cascadeCount)
		{
			float d_uni = glm::mix(near, far, i);
			float d_log = near * glm::pow((far / near), i);

			m_cascadeSplits.push_back(glm::mix(d_uni, d_log, 0.5f));
		}
		//m_cascadeSplits = { 4.0f, 10.0f, 20.0f, 32.0f };

		for (int i(0); i < m_uboVPCSM.size(); ++i)
		{
			m_uboVPCSM[i].load(&m_vk, {}, VK_SHADER_STAGE_VERTEX_BIT);
		}

		m_uboLightSpaceCSMData.matrices = std::vector<glm::mat4>(m_cascadeCount);
		m_uboLightSpaceCSM.load(&m_vk, m_uboLightSpaceCSMData.getData(), m_uboLightSpaceCSMData.getSize(), VK_SHADER_STAGE_VERTEX_BIT);

		m_uboCascadeSplitsData.cascadeSplits.resize(m_cascadeCount);
		for (int i(0); i < m_cascadeCount; ++i)
			m_uboCascadeSplitsData.cascadeSplits[i].x = m_cascadeSplits[i];
		m_uboCascadeSplits.load(&m_vk, m_uboCascadeSplitsData.getData(), m_uboCascadeSplitsData.getSize(), VK_SHADER_STAGE_FRAGMENT_BIT);

		updateCSM();
	}
	if (m_usedEffects & EFFECT_TYPE_RSM)
	{
		m_uboVPRSMData.view = glm::lookAt(glm::normalize(m_lightDir) * -30.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		m_uboVPRSMData.proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 50.0f);
		m_uboVPRSM.load(&m_vk, m_uboVPRSMData, VK_SHADER_STAGE_VERTEX_BIT);

		m_uboProjRSMData.matrix = m_uboVPRSMData.proj * m_uboVPRSMData.view * m_uboModelData.matrix;
		m_uboProjRSM.load(&m_vk, m_uboProjRSMData, VK_SHADER_STAGE_VERTEX_BIT);
	}
	if (m_usedEffects & EFFECT_TYPE_RADIOSITY_PROBES)
	{
		m_uboRadiosityProbesData.values.resize(1000);
		for (int i(0); i < 1000; ++i)
			m_uboRadiosityProbesData.values[i].x = 0.0f;
		m_uboRadiosityProbes.load(&m_vk, m_uboRadiosityProbesData.getData(), m_uboRadiosityProbesData.getSize(), VK_SHADER_STAGE_FRAGMENT_BIT);
	}
}

void System::setSemaphores()
{
	if (m_usedEffects == 0)
		m_vk.setRenderFinishedLastRenderPassSemaphore(VK_NULL_HANDLE);
	else if (m_usedEffects == EFFECT_TYPE_CASCADED_SHADOW_MAPPING || m_usedEffects == (EFFECT_TYPE_CASCADED_SHADOW_MAPPING | EFFECT_TYPE_RADIOSITY_PROBES))
	{
		m_offscreenShadowCalculation.setSemaphoreToWait(m_vk.getDevice(), {
			{ m_offscreenCascadedShadowMap.getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT }
			});

		m_offscreenShadowBlurHorizontal[0].setSemaphoreToWait(m_vk.getDevice(), {
				{ m_offscreenShadowCalculation.getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT }
			});
		m_offscreenShadowBlurVertical[0].setSemaphoreToWait(m_vk.getDevice(), {
			{ m_offscreenShadowBlurHorizontal[0].getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT }
			});
		for (int i(1); i < m_blurAmount; ++i)
		{
			m_offscreenShadowBlurHorizontal[i].setSemaphoreToWait(m_vk.getDevice(), {
				{ m_offscreenShadowBlurVertical[i - 1].getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT }
				});
			m_offscreenShadowBlurVertical[i].setSemaphoreToWait(m_vk.getDevice(), {
				{ m_offscreenShadowBlurHorizontal[i].getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT }
				});
		}
		m_vk.setRenderFinishedLastRenderPassSemaphore(m_offscreenShadowBlurVertical[m_blurAmount - 1].getRenderFinishedSemaphore());
	}
	else if (m_usedEffects == EFFECT_TYPE_RSM)
	{
		m_vk.setRenderFinishedLastRenderPassSemaphore(m_offscreenRSM.getRenderFinishedSemaphore());
	}
	else if (m_usedEffects == (EFFECT_TYPE_CASCADED_SHADOW_MAPPING | EFFECT_TYPE_RSM))
	{
		m_offscreenShadowCalculation.setSemaphoreToWait(m_vk.getDevice(), {
			{ m_offscreenCascadedShadowMap.getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT }
			});

		m_offscreenShadowBlurHorizontal[0].setSemaphoreToWait(m_vk.getDevice(), {
				{ m_offscreenShadowCalculation.getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT }
			});
		m_offscreenShadowBlurVertical[0].setSemaphoreToWait(m_vk.getDevice(), {
			{ m_offscreenShadowBlurHorizontal[0].getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT }
			});
		for (int i(1); i < m_blurAmount; ++i)
		{
			m_offscreenShadowBlurHorizontal[i].setSemaphoreToWait(m_vk.getDevice(), {
				{ m_offscreenShadowBlurVertical[i - 1].getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT }
				});
			m_offscreenShadowBlurVertical[i].setSemaphoreToWait(m_vk.getDevice(), {
				{ m_offscreenShadowBlurHorizontal[i].getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT }
				});
		}
		m_offscreenRSM.setSemaphoreToWait(m_vk.getDevice(), { { m_offscreenShadowBlurVertical[m_blurAmount - 1].getRenderFinishedSemaphore(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT } });
		m_vk.setRenderFinishedLastRenderPassSemaphore(m_offscreenRSM.getRenderFinishedSemaphore());
	}
}
