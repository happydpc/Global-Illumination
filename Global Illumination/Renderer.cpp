#include "Renderer.h"

#include <utility>

Renderer::~Renderer()
= default;

void Renderer::initialize(VkDevice device, std::string vertexShader, std::string fragmentShader, std::vector<VkVertexInputBindingDescription> vertexInputDescription,
                          std::vector<VkVertexInputAttributeDescription> attributeInputDescription, std::vector<UniformBufferObjectLayout> uniformBufferObjectLayouts,
                          std::vector<TextureLayout> textureLayouts, std::vector<ImageLayout> imageLayouts, std::vector<SamplerLayout> samplerLayouts, std::vector<BufferLayout> bufferLayouts, 
                          std::vector<bool> alphaBlending)
{
	m_vertexShader = std::move(vertexShader);
	m_geometryShader = "";
	m_fragmentShader = std::move(fragmentShader);
	m_vertexInputDescription = std::move(vertexInputDescription);
	m_attributeInputDescription = std::move(attributeInputDescription);
	m_alphaBlending = std::move(alphaBlending);

	createDescriptorSetLayout(device, std::move(uniformBufferObjectLayouts), std::move(textureLayouts), std::move(imageLayouts), std::move(samplerLayouts),
		std::move(bufferLayouts));
}

void Renderer::initialize(VkDevice device, std::string vertexShader, std::string geometryShader,
	std::string fragmentShader, std::vector<VkVertexInputBindingDescription> vertexInputDescription,
	std::vector<VkVertexInputAttributeDescription> attributeInputDescription,
	std::vector<UniformBufferObjectLayout> uniformBufferObjectLayouts, std::vector<TextureLayout> textureLayouts,
	std::vector<ImageLayout> imageLayouts, std::vector<SamplerLayout> samplerLayouts,
	std::vector<BufferLayout> bufferLayouts, std::vector<bool> alphaBlending)
{
	m_vertexShader = std::move(vertexShader);
	m_geometryShader = std::move(geometryShader);
	m_fragmentShader = std::move(fragmentShader);
	m_vertexInputDescription = std::move(vertexInputDescription);
	m_attributeInputDescription = std::move(attributeInputDescription);
	m_alphaBlending = std::move(alphaBlending);

	createDescriptorSetLayout(device, std::move(uniformBufferObjectLayouts), std::move(textureLayouts), std::move(imageLayouts), std::move(samplerLayouts),
		std::move(bufferLayouts));
}

void Renderer::initialize(VkDevice device, std::string vertexShader,
                          std::vector<VkVertexInputBindingDescription> vertexInputDescription,
                          std::vector<VkVertexInputAttributeDescription> attributeInputDescription,
                          std::vector<UniformBufferObjectLayout> uniformBufferObjectLayouts, std::vector<TextureLayout> textureLayouts,
                          std::vector<bool> alphaBlending)
{
	m_vertexShader = std::move(vertexShader);
	m_geometryShader = "";
	m_fragmentShader = "";
	m_vertexInputDescription = std::move(vertexInputDescription);
	m_attributeInputDescription = std::move(attributeInputDescription);
	m_alphaBlending = std::move(alphaBlending);

	createDescriptorSetLayout(device, std::move(uniformBufferObjectLayouts), std::move(textureLayouts), {}, {}, {}); // !! change definition to allow image and sampler separated
}

void Renderer::createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent, VkSampleCountFlagBits msaa)
{
	if (m_pipelineCreated)
		return;

	m_pipeline.initialize(device, renderPass, m_vertexShader, m_geometryShader, m_fragmentShader, m_vertexInputDescription, m_attributeInputDescription, extent, msaa, m_alphaBlending, &m_descriptorSetLayout);
	m_pipelineCreated = true;
}

void Renderer::destroyPipeline(VkDevice device)
{
	if (!m_pipelineCreated)
		return;

	m_pipeline.cleanup(device);
	m_pipelineCreated = false;
}

int Renderer::addMesh(VkDevice device, VkDescriptorPool descriptorPool, VertexBuffer vertexBuffer,
	std::vector<std::pair<UniformBufferObject*, UniformBufferObjectLayout>> ubos, std::vector<std::pair<Texture*, TextureLayout>> textures,
	std::vector<std::pair<Image*, ImageLayout>> images, std::vector<std::pair<Sampler*, SamplerLayout>> samplers, std::vector<std::pair<VkBuffer, BufferLayout>> buffers)
{
	m_meshes.push_back({ vertexBuffer, createDescriptorSet(device, descriptorPool, ubos, textures, images, samplers, buffers) });
	
	return m_meshes.size() - 1;
}

int Renderer::addMeshInstancied(VkDevice device, VkDescriptorPool descriptorPool, VertexBuffer vertexBuffer,
	InstanceBuffer instanceBuffer, std::vector<std::pair<UniformBufferObject*, UniformBufferObjectLayout>> ubos,
	std::vector<std::pair<Texture*, TextureLayout>> textures)
{
	m_meshesInstancied.push_back({ vertexBuffer, instanceBuffer, createDescriptorSet(device, descriptorPool, ubos, textures, {}, {}, {}) }); // !!

	return m_meshesInstancied.size() - 1;
}

void Renderer::reloadMeshVertexBuffer(VkDevice device, VertexBuffer vertexBuffer, int meshID)
{
	m_meshes[meshID].first = vertexBuffer;
}

void Renderer::cleanup(VkDevice device, VkDescriptorPool descriptorPool)
{
	for (int i(0); i < m_meshes.size(); ++i)
		vkFreeDescriptorSets(device, descriptorPool, 1, &m_meshes[i].second);
	vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
	destroyPipeline(device);

	m_meshes.clear();
	m_meshesInstancied.clear();
}

std::vector<std::pair<VertexBuffer, VkDescriptorSet>> Renderer::getMeshes()
{
	return m_meshes;
}

std::vector<std::tuple<VertexBuffer, InstanceBuffer, VkDescriptorSet>> Renderer::getMeshesInstancied()
{
	return m_meshesInstancied;
}

void Renderer::createDescriptorSetLayout(VkDevice device, std::vector<UniformBufferObjectLayout> uniformBufferObjectLayouts, std::vector<TextureLayout> textureLayouts,
	std::vector<ImageLayout> imageLayouts, std::vector<SamplerLayout> samplerLayouts, std::vector<BufferLayout> bufferLayouts)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	for (auto& uniformBufferObjectLayout : uniformBufferObjectLayouts)
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = uniformBufferObjectLayout.binding;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = uniformBufferObjectLayout.accessibility;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		bindings.push_back(uboLayoutBinding);
	}

	for (auto& textureLayout : textureLayouts)
	{
		VkDescriptorSetLayoutBinding imageSamplerLayoutBinding = {};
		imageSamplerLayoutBinding.binding = textureLayout.binding;
		imageSamplerLayoutBinding.descriptorCount = static_cast<uint32_t>(1);
		imageSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imageSamplerLayoutBinding.stageFlags = textureLayout.accessibility;
		imageSamplerLayoutBinding.pImmutableSamplers = nullptr;

		bindings.push_back(imageSamplerLayoutBinding);
	}

	if (!imageLayouts.empty())
	{
		VkDescriptorSetLayoutBinding imageLayoutBinding = {};
		imageLayoutBinding.binding = imageLayouts[0].binding;
		imageLayoutBinding.descriptorCount = static_cast<uint32_t>(imageLayouts.size());
		imageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		imageLayoutBinding.stageFlags = imageLayouts[0].accessibility;
		imageLayoutBinding.pImmutableSamplers = nullptr;

		bindings.push_back(imageLayoutBinding);
	}

	if (!samplerLayouts.empty())
	{
		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = samplerLayouts[0].binding;
		samplerLayoutBinding.descriptorCount = static_cast<uint32_t>(samplerLayouts.size());
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		samplerLayoutBinding.stageFlags = samplerLayouts[0].accessibility;
		samplerLayoutBinding.pImmutableSamplers = nullptr;

		bindings.push_back(samplerLayoutBinding);
	}

	for (auto& bufferLayout : bufferLayouts)
	{
		VkDescriptorSetLayoutBinding bufferLayoutBinding = {};
		bufferLayoutBinding.binding = bufferLayout.binding;
		bufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bufferLayoutBinding.descriptorCount = 1;
		bufferLayoutBinding.stageFlags = bufferLayout.accessibility;
		bufferLayoutBinding.pImmutableSamplers = nullptr;

		bindings.push_back(bufferLayoutBinding);
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("Error : create descriptor set layout");
}

VkDescriptorSet Renderer::createDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool,
	std::vector<std::pair<UniformBufferObject*, UniformBufferObjectLayout>> ubos, std::vector<std::pair<Texture*, TextureLayout>> textures,
	std::vector<std::pair<Image*, ImageLayout>> images, std::vector<std::pair<Sampler*, SamplerLayout>> samplers, std::vector<std::pair<VkBuffer, BufferLayout>> buffers)
{
	VkDescriptorSetLayout layouts[] = { m_descriptorSetLayout };
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = layouts;

	VkDescriptorSet descriptorSet;
	VkResult res = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Error : allocate descriptor set");

	std::vector<VkWriteDescriptorSet> descriptorWrites;

	std::vector<VkDescriptorBufferInfo> uniformBufferInfo(ubos.size());
	for (int i(0); i < ubos.size(); ++i)
	{
		uniformBufferInfo[i].buffer = ubos[i].first->getUniformBuffer();
		uniformBufferInfo[i].offset = 0;
		uniformBufferInfo[i].range = ubos[i].first->getSize();

		VkWriteDescriptorSet descriptorWrite;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = ubos[i].second.binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &uniformBufferInfo[i];
		descriptorWrite.pNext = NULL;

		descriptorWrites.push_back(descriptorWrite);
	}

	std::vector<VkDescriptorImageInfo> imageSamplerInfo(textures.size());
	for (int i(0); i < textures.size(); ++i)
	{
		imageSamplerInfo[i].imageLayout = textures[i].first->getImageLayout();
		imageSamplerInfo[i].imageView = textures[i].first->getImageView();
		imageSamplerInfo[i].sampler = textures[i].first->getSampler();
	}

	if (!textures.empty())
	{
		VkWriteDescriptorSet descriptorWrite;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = textures[0].second.binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = static_cast<uint32_t>(imageSamplerInfo.size());
		descriptorWrite.pImageInfo = imageSamplerInfo.data();
		descriptorWrite.pNext = NULL;

		descriptorWrites.push_back(descriptorWrite);
	}

	std::vector<VkDescriptorImageInfo> imageInfo(images.size());
	for (int i(0); i < images.size(); ++i)
	{
		imageInfo[i].imageLayout = images[i].first->getImageLayout();
		imageInfo[i].imageView = images[i].first->getImageView();
	}

	if (!images.empty())
	{
		VkWriteDescriptorSet descriptorWrite;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = images[0].second.binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		descriptorWrite.descriptorCount = static_cast<uint32_t>(imageInfo.size());
		descriptorWrite.pImageInfo = imageInfo.data();
		descriptorWrite.pNext = NULL;

		descriptorWrites.push_back(descriptorWrite);
	}

	std::vector<VkDescriptorImageInfo> samplerInfo(samplers.size());
	for (int i(0); i < samplers.size(); ++i)
	{
		samplerInfo[i].sampler = samplers[i].first->getSampler();
	}

	if (!samplers.empty())
	{
		VkWriteDescriptorSet descriptorWrite;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = samplers[0].second.binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptorWrite.descriptorCount = static_cast<uint32_t>(samplerInfo.size());
		descriptorWrite.pImageInfo = samplerInfo.data();
		descriptorWrite.pNext = NULL;

		descriptorWrites.push_back(descriptorWrite);
	}

	std::vector<VkDescriptorBufferInfo> bufferInfo(buffers.size());
	for (int i(0); i < buffers.size(); ++i)
	{
		bufferInfo[i].buffer = buffers[i].first;
		bufferInfo[i].offset = 0;
		bufferInfo[i].range = buffers[i].second.size;

		VkWriteDescriptorSet descriptorWrite;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = buffers[i].second.binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo[i];
		descriptorWrite.pNext = NULL;

		descriptorWrites.push_back(descriptorWrite);
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

	return descriptorSet;
}
