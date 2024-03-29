#include "runtime/function/render/rhi/vulkan/vulkan_rhi.h"
#include "runtime/function/render/rhi/vulkan/vulkan_buffer_utils.h"
#include "runtime/function/render/rhi/vulkan/vulkan_device.h"
#include "runtime/function/render/rhi/vulkan/vulkan_instance.h"
#include "runtime/function/render/rhi/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/rhi/vulkan/vulkan_render_pass.h"
#include "runtime/function/render/rhi/vulkan/vulkan_shader.h"
#include "runtime/function/render/rhi/vulkan/vulkan_swap_chain.h"
#include "runtime/function/render/rhi/vulkan/vulkan_ui.h"
#include "runtime/function/render/rhi/vulkan/vulkan_utils.h"

#include "runtime/function/render/geometry/vertex.h"

#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/config_manager/config_manager.h"

#include "runtime/function/window/window_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/math/graphics_utils.h"

#if defined(__GNUC__)
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
#if defined(__linux__)
#include <stdlib.h>
#elif defined(__MACH__)
// https://developer.apple.com/library/archive/documentation/Porting/Conceptual/PortingUnix/compiling/compiling.html
#include <stdlib.h>
#else
#error Unknown Platform
#endif
#endif

const std::vector<ArchViz::Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

namespace ArchViz
{
    void VulkanRHI::setConfigManager(std::shared_ptr<ConfigManager> config_manager)
    {
        ASSERT(config_manager);
        m_config_manager = config_manager;
    }

    void VulkanRHI::setAssetManager(std::shared_ptr<AssetManager> asset_manager)
    {
        ASSERT(asset_manager);
        m_asset_manager = asset_manager;
    }

    void VulkanRHI::createInstance()
    {
        m_vulkan_instance = std::make_shared<VulkanInstance>(m_enable_validation_layers);
        m_vulkan_instance->connect(m_initialize_info.window_system->getWindow());
        m_vulkan_instance->initialize();
    }

    void VulkanRHI::createVulkanDevice()
    {
        m_vulkan_device = std::make_shared<VulkanDevice>(m_enable_validation_layers);
        m_vulkan_device->connect(m_vulkan_instance->m_instance, m_vulkan_instance->m_surface);
        m_vulkan_device->initialize();
    }

    void VulkanRHI::createSwapChain()
    {
        m_vulkan_swap_chain = std::make_shared<VulkanSwapChain>();

        uint32_t width  = m_initialize_info.window_system->getWindowWidth();
        uint32_t height = m_initialize_info.window_system->getWindowHeight();

        m_vulkan_swap_chain->connect(m_vulkan_instance->m_instance, m_vulkan_instance->m_surface, m_vulkan_device->m_physical_device, m_vulkan_device->m_device);
        m_vulkan_swap_chain->initialize(width, height, false, false);
    }

    void VulkanRHI::recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_initialize_info.window_system->getWindow(), &width, &height);
        // for minimize
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_initialize_info.window_system->getWindow(), &width, &height);
            glfwWaitEvents();
        }

        m_vulkan_device->wait();

        for (auto framebuffer : m_swap_chain_framebuffers)
        {
            vkDestroyFramebuffer(m_vulkan_device->m_device, framebuffer, nullptr);
        }

        m_vulkan_swap_chain->clear();

        m_vulkan_swap_chain->initialize(width, height);
        createFramebuffers();
    }

    void VulkanRHI::createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding ubo_layout_binding {};
        ubo_layout_binding.binding            = 0;
        ubo_layout_binding.descriptorCount    = 1;
        ubo_layout_binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout_binding.pImmutableSamplers = nullptr;
        ubo_layout_binding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layout_info {};
        layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings    = &ubo_layout_binding;

        if (vkCreateDescriptorSetLayout(m_vulkan_device->m_device, &layout_info, nullptr, &m_descriptor_set_layout) != VK_SUCCESS)
        {
            LOG_FATAL("failed to create descriptor set layout!");
        }
    }

    void VulkanRHI::createRenderPass()
    {
        m_vulkan_render_pass = std::make_shared<VulkanRenderPass>();
        m_vulkan_render_pass->setDevice(m_vulkan_device);
        m_vulkan_render_pass->m_color_format = m_vulkan_swap_chain->m_swap_chain_image_format;
        m_vulkan_render_pass->initialize();
    }

    void VulkanRHI::createGraphicsPipeline()
    {
        ShaderModuleConfig config;
        config.m_vert_shader = "shader/glsl/shader_ubo.vert";
        config.m_frag_shader = "shader/glsl/shader_ubo.frag";

        std::shared_ptr<VulkanShader> shader = std::make_shared<VulkanShader>(config);

        shader->m_device         = m_vulkan_device;
        shader->m_config_manager = m_config_manager;
        shader->m_asset_manager  = m_asset_manager;

        m_vulkan_pipeline = std::make_shared<VulkanPipeline>();
        m_vulkan_pipeline->setDevice(m_vulkan_device);
        m_vulkan_pipeline->setShaderModule(shader);
        m_vulkan_pipeline->setRenderPass(m_vulkan_render_pass->m_render_pass);
        m_vulkan_pipeline->setDescriptorSetLayout(m_descriptor_set_layout);
        m_vulkan_pipeline->initialize();
    }

    void VulkanRHI::createFramebuffers()
    {
        m_swap_chain_framebuffers.resize(m_vulkan_swap_chain->m_buffers.size());

        for (size_t i = 0; i < m_vulkan_swap_chain->m_buffers.size(); i++)
        {
            VkImageView attachments[] = {m_vulkan_swap_chain->m_buffers[i].view};

            VkFramebufferCreateInfo framebuffer_info {};
            framebuffer_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass      = m_vulkan_render_pass->m_render_pass;
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments    = attachments;
            framebuffer_info.width           = m_vulkan_swap_chain->m_swap_chain_extent.width;
            framebuffer_info.height          = m_vulkan_swap_chain->m_swap_chain_extent.height;
            framebuffer_info.layers          = 1;

            if (vkCreateFramebuffer(m_vulkan_device->m_device, &framebuffer_info, nullptr, &m_swap_chain_framebuffers[i]) != VK_SUCCESS)
            {
                LOG_FATAL("failed to create framebuffer!");
            }
        }
    }

    void VulkanRHI::createCommandPool()
    {
        VkCommandPoolCreateInfo pool_info {};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = m_vulkan_device->m_indices.m_graphics_family.value();

        if (vkCreateCommandPool(m_vulkan_device->m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS)
        {
            LOG_FATAL("failed to create command pool!");
        }
    }

    void VulkanRHI::createCommandBuffer()
    {
        m_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = m_command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        if (vkAllocateCommandBuffers(m_vulkan_device->m_device, &alloc_info, m_command_buffers.data()) != VK_SUCCESS)
        {
            LOG_FATAL("failed to allocate command buffers!");
        }

        alloc_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(m_vulkan_device->m_device, &alloc_info, &m_transfer_buffer) != VK_SUCCESS)
        {
            LOG_FATAL("failed to allocate command buffers!");
        }
    }

    void VulkanRHI::createImGui()
    {
        m_vulkan_ui = std::make_shared<VulkanUI>();

        m_vulkan_ui->m_window = m_initialize_info.window_system->getWindow();

        m_vulkan_ui->m_instance              = m_vulkan_instance->m_instance;
        m_vulkan_ui->m_physical_device       = m_vulkan_device->m_physical_device;
        m_vulkan_ui->m_device                = m_vulkan_device->m_device;
        m_vulkan_ui->m_graphics_queue_family = m_vulkan_device->m_indices.m_graphics_family.value();
        m_vulkan_ui->m_graphics_queue        = m_vulkan_device->m_graphics_queue;
        m_vulkan_ui->m_image_count           = static_cast<uint32_t>(m_vulkan_swap_chain->m_images.size());
        m_vulkan_ui->m_image_format          = m_vulkan_swap_chain->m_swap_chain_image_format;

        m_vulkan_ui->m_command_pool   = m_command_pool;
        m_vulkan_ui->m_command_buffer = m_transfer_buffer;

        // m_vulkan_ui->initialize();
    }

    void VulkanRHI::createVertexBuffer()
    {
        VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

        VkBuffer              staging_buffer;
        VkDeviceMemory        staging_buffer_memory;
        VkBufferUsageFlags    usage      = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VulkanBufferUtils::createBuffer(m_vulkan_device, buffer_size, usage, properties, staging_buffer, staging_buffer_memory);

        void* data;
        vkMapMemory(m_vulkan_device->m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
        {
            memcpy(data, vertices.data(), (size_t)buffer_size);
        }
        vkUnmapMemory(m_vulkan_device->m_device, staging_buffer_memory);

        usage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VulkanBufferUtils::createBuffer(m_vulkan_device, buffer_size, usage, properties, m_vertex_buffer, m_vertex_buffer_memory);

        VulkanBufferUtils::copyBuffer(m_vulkan_device, m_command_pool, staging_buffer, m_vertex_buffer, buffer_size);

        vkDestroyBuffer(m_vulkan_device->m_device, staging_buffer, nullptr);
        vkFreeMemory(m_vulkan_device->m_device, staging_buffer_memory, nullptr);
    }

    void VulkanRHI::createIndexBuffer()
    {
        VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

        VkBuffer              staging_buffer;
        VkDeviceMemory        staging_buffer_memory;
        VkBufferUsageFlags    usage      = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VulkanBufferUtils::createBuffer(m_vulkan_device, buffer_size, usage, properties, staging_buffer, staging_buffer_memory);

        void* data;
        vkMapMemory(m_vulkan_device->m_device, staging_buffer_memory, 0, buffer_size, 0, &data);
        {
            memcpy(data, indices.data(), (size_t)buffer_size);
        }
        vkUnmapMemory(m_vulkan_device->m_device, staging_buffer_memory);

        usage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VulkanBufferUtils::createBuffer(m_vulkan_device, buffer_size, usage, properties, m_index_buffer, m_index_buffer_memory);

        VulkanBufferUtils::copyBuffer(m_vulkan_device, m_command_pool, staging_buffer, m_index_buffer, buffer_size);

        vkDestroyBuffer(m_vulkan_device->m_device, staging_buffer, nullptr);
        vkFreeMemory(m_vulkan_device->m_device, staging_buffer_memory, nullptr);
    }

    void VulkanRHI::createUniformBuffers()
    {
        VkDeviceSize buffer_size = sizeof(UniformBufferObject);

        m_uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniform_buffers_memory.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            auto usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            auto flag  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            VulkanBufferUtils::createBuffer(m_vulkan_device, buffer_size, usage, flag, m_uniform_buffers[i], m_uniform_buffers_memory[i]);

            vkMapMemory(m_vulkan_device->m_device, m_uniform_buffers_memory[i], 0, buffer_size, 0, &m_uniform_buffers_mapped[i]);
        }
    }

    void VulkanRHI::createDescriptorPool()
    {
        VkDescriptorPoolSize pool_size {};
        pool_size.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_size.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo pool_info {};
        pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes    = &pool_size;
        pool_info.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(m_vulkan_device->m_device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS)
        {
            LOG_FATAL("failed to create descriptor pool!");
        }
    }

    void VulkanRHI::createDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptor_set_layout);

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = m_descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        alloc_info.pSetLayouts        = layouts.data();

        m_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(m_vulkan_device->m_device, &alloc_info, m_descriptor_sets.data()) != VK_SUCCESS)
        {
            LOG_FATAL("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkDescriptorBufferInfo bufferInfo {};
            bufferInfo.buffer = m_uniform_buffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range  = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWrite {};
            descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet          = m_descriptor_sets[i];
            descriptorWrite.dstBinding      = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo     = &bufferInfo;

            vkUpdateDescriptorSets(m_vulkan_device->m_device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    void VulkanRHI::createSyncObjects()
    {
        m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphore_info {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(m_vulkan_device->m_device, &semaphore_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_vulkan_device->m_device, &semaphore_info, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_vulkan_device->m_device, &fence_info, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS)
            {
                LOG_FATAL("failed to create synchronization objects for a frame!");
            }
        }
    }

    void VulkanRHI::createAssetAllocator()
    {
        VmaVulkanFunctions vulkanFunctions    = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocator_create_info = {};
        allocator_create_info.vulkanApiVersion       = m_vulkan_instance->m_vulkan_api_version;
        allocator_create_info.physicalDevice         = m_vulkan_device->m_physical_device;
        allocator_create_info.device                 = m_vulkan_device->m_device;
        allocator_create_info.instance               = m_vulkan_instance->m_instance;
        allocator_create_info.pVulkanFunctions       = &vulkanFunctions;

        vmaCreateAllocator(&allocator_create_info, &m_assets_allocator);
    }

    void VulkanRHI::initialize(RHIInitInfo initialize_info)
    {
        m_initialize_info = initialize_info;

        createInstance();

        createVulkanDevice();

        createSwapChain();

        createDescriptorSetLayout();

        createRenderPass();
        createGraphicsPipeline();

        createFramebuffers();

        createCommandPool();
        createCommandBuffer();

        createImGui();

        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();

        createSyncObjects();
    }

    void VulkanRHI::recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index)
    {
        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
        {
            LOG_FATAL("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo render_pass_info {};
        render_pass_info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass        = m_vulkan_render_pass->m_render_pass;
        render_pass_info.framebuffer       = m_swap_chain_framebuffers[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = m_vulkan_swap_chain->m_swap_chain_extent;

        VkClearValue clear_color         = {{{0.2f, 0.3f, 0.4f, 1.0f}}};
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues    = &clear_color;

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vulkan_pipeline->m_pipeline);

            VkViewport viewport {};
            viewport.x        = 0.0f;
            viewport.y        = 0.0f;
            viewport.width    = (float)m_vulkan_swap_chain->m_swap_chain_extent.width;
            viewport.height   = (float)m_vulkan_swap_chain->m_swap_chain_extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);

            VkRect2D scissor {};
            scissor.offset = {0, 0};
            scissor.extent = m_vulkan_swap_chain->m_swap_chain_extent;
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);

            VkBuffer     vertex_buffers[] = {m_vertex_buffer};
            VkDeviceSize offsets[]        = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

            vkCmdBindIndexBuffer(command_buffer, m_index_buffer, 0, VK_INDEX_TYPE_UINT16);

            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vulkan_pipeline->m_pipeline_layout, 0, 1, &m_descriptor_sets[m_current_frame], 0, nullptr);

            vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(command_buffer);

        // m_vulkan_ui->recordCommandBuffer(command_buffer, m_swap_chain_framebuffers[image_index], m_vulkan_swap_chain->m_swap_chain_extent.width, m_vulkan_swap_chain->m_swap_chain_extent.height);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        {
            LOG_FATAL("failed to record command buffer!");
        }
    }

    void VulkanRHI::updateUniformBuffer(uint32_t current_image)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto  currentTime = std::chrono::high_resolution_clock::now();
        float time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo {};
        ubo.model = FMatrix4::Identity();
        ubo.model.block<3, 3>(0, 0) = Eigen::AngleAxisf(time * 0.1f, FVector3::UnitZ()).toRotationMatrix();
        // glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = look_at({2.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
        // look_at({2.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
        ubo.proj = perspective(45.0f / 180.0 * 3.14, m_vulkan_swap_chain->m_swap_chain_extent.width / m_vulkan_swap_chain->m_swap_chain_extent.height, 0.1f, 10.0f);
        // glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj(1, 1) *= -1;

        memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
    }

    void VulkanRHI::drawFrame()
    {
        // TODO : Although many drivers and platforms trigger VK_ERROR_OUT_OF_DATE_KHR automatically after a window resize, it is not guaranteed to happen.
        // That's why we'll add some extra code to also handle resizes explicitly.

        // handle swap chain recreation
        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(m_vulkan_device->m_device, m_vulkan_swap_chain->m_swap_chain, UINT64_MAX, m_image_available_semaphores[m_current_frame], VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            LOG_FATAL("failed to acquire swap chain image!");
        }

        vkResetFences(m_vulkan_device->m_device, 1, &m_in_flight_fences[m_current_frame]);

        vkResetCommandBuffer(m_command_buffers[m_current_frame], /*VkCommandBufferResetFlagBits*/ 0);
        recordCommandBuffer(m_command_buffers[m_current_frame], image_index);

        VkSemaphore          wait_semaphores[]   = {m_image_available_semaphores[m_current_frame]};
        VkPipelineStageFlags wait_stages[]       = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore          signal_semaphores[] = {m_render_finished_semaphores[m_current_frame]};

        VkSubmitInfo submit_info {};
        submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount   = 1;
        submit_info.pWaitSemaphores      = wait_semaphores;
        submit_info.pWaitDstStageMask    = wait_stages;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &m_command_buffers[m_current_frame];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = signal_semaphores;

        if (vkQueueSubmit(m_vulkan_device->m_graphics_queue, 1, &submit_info, m_in_flight_fences[m_current_frame]) != VK_SUCCESS)
        {
            LOG_FATAL("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR present_info {};
        VkSwapchainKHR   swap_chains[]  = {m_vulkan_swap_chain->m_swap_chain};
        present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = signal_semaphores;
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = swap_chains;
        present_info.pImageIndices      = &image_index;

        result = vkQueuePresentKHR(m_vulkan_device->m_present_queue, &present_info);
        if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result)
        {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            LOG_FATAL("failed to present swap chain image!");
        }

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanRHI::prepareContext()
    {
        vkWaitForFences(m_vulkan_device->m_device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);
        vkResetFences(m_vulkan_device->m_device, 1, &m_in_flight_fences[m_current_frame]);

        // m_vulkan_ui->prepareContext();
        updateUniformBuffer(m_current_frame);
    }

    void VulkanRHI::render()
    {
        // m_vulkan_ui->render();
        drawFrame();
    }

    void VulkanRHI::clear()
    {
        m_vulkan_device->wait();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(m_vulkan_device->m_device, m_image_available_semaphores[i], nullptr);
            vkDestroySemaphore(m_vulkan_device->m_device, m_render_finished_semaphores[i], nullptr);
            vkDestroyFence(m_vulkan_device->m_device, m_in_flight_fences[i], nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyBuffer(m_vulkan_device->m_device, m_uniform_buffers[i], nullptr);
            vkFreeMemory(m_vulkan_device->m_device, m_uniform_buffers_memory[i], nullptr);
        }

        vkDestroyCommandPool(m_vulkan_device->m_device, m_command_pool, nullptr);

        // m_vulkan_ui->clear();
        m_vulkan_ui.reset();

        vkDestroyBuffer(m_vulkan_device->m_device, m_index_buffer, nullptr);
        vkFreeMemory(m_vulkan_device->m_device, m_index_buffer_memory, nullptr);

        vkDestroyBuffer(m_vulkan_device->m_device, m_vertex_buffer, nullptr);
        vkFreeMemory(m_vulkan_device->m_device, m_vertex_buffer_memory, nullptr);

        for (auto framebuffer : m_swap_chain_framebuffers)
        {
            vkDestroyFramebuffer(m_vulkan_device->m_device, framebuffer, nullptr);
        }

        vkDestroyDescriptorPool(m_vulkan_device->m_device, m_descriptor_pool, nullptr);

        vkDestroyDescriptorSetLayout(m_vulkan_device->m_device, m_descriptor_set_layout, nullptr);

        m_vulkan_pipeline->clear();
        m_vulkan_pipeline.reset();

        m_vulkan_render_pass->clear();
        m_vulkan_render_pass.reset();

        m_vulkan_swap_chain->clear();
        m_vulkan_swap_chain.reset();

        m_vulkan_device->clear();
        m_vulkan_device.reset();

        m_vulkan_instance->clear();
        m_vulkan_instance.reset();
    }
} // namespace ArchViz
