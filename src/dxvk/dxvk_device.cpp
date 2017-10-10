#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {
  
  DxvkDevice::DxvkDevice(
    const Rc<DxvkAdapter>&  adapter,
    const Rc<vk::DeviceFn>& vkd)
  : m_adapter       (adapter),
    m_vkd           (vkd),
    m_memory        (adapter, vkd),
    m_renderPassPool(vkd) {
    TRACE(this, adapter);
    
    m_vkd->vkGetDeviceQueue(m_vkd->device(),
      m_adapter->graphicsQueueFamily(), 0,
      &m_graphicsQueue);
    m_vkd->vkGetDeviceQueue(m_vkd->device(),
      m_adapter->presentQueueFamily(), 0,
      &m_presentQueue);
  }
  
  
  DxvkDevice::~DxvkDevice() {
    TRACE(this);
    m_vkd->vkDeviceWaitIdle(m_vkd->device());
    m_vkd->vkDestroyDevice(m_vkd->device(), nullptr);
  }
  
  
  Rc<DxvkCommandList> DxvkDevice::createCommandList() {
    return new DxvkCommandList(m_vkd,
      m_adapter->graphicsQueueFamily());
  }
  
  
  Rc<DxvkContext> DxvkDevice::createContext() {
    return new DxvkContext(m_vkd);
  }
  
  
  Rc<DxvkFramebuffer> DxvkDevice::createFramebuffer(
    const DxvkRenderTargets& renderTargets) {
    auto format = renderTargets.renderPassFormat();
    auto renderPass = m_renderPassPool.getRenderPass(format);
    return new DxvkFramebuffer(m_vkd, renderPass, renderTargets);
  }
  
  
  Rc<DxvkImage> DxvkDevice::createImage(
    const DxvkImageCreateInfo&  createInfo,
          VkMemoryPropertyFlags memoryType) {
    
  }
  
  
  Rc<DxvkImageView> DxvkDevice::createImageView(
    const Rc<DxvkImage>&            image,
    const DxvkImageViewCreateInfo&  createInfo) {
    return new DxvkImageView(m_vkd, image, createInfo);
  }
  
  
  Rc<DxvkSemaphore> DxvkDevice::createSemaphore() {
    return new DxvkSemaphore(m_vkd);
  }
  
  
  Rc<DxvkSwapchain> DxvkDevice::createSwapchain(
    const Rc<DxvkSurface>&          surface,
    const DxvkSwapchainProperties&  properties) {
    return new DxvkSwapchain(this, surface, properties, m_presentQueue);
  }
  
  
  Rc<DxvkFence> DxvkDevice::submitCommandList(
    const Rc<DxvkCommandList>&      commandList,
    const Rc<DxvkSemaphore>&        waitSync,
    const Rc<DxvkSemaphore>&        wakeSync) {
    TRACE(this, commandList, waitSync, wakeSync);
    
    Rc<DxvkFence> fence = new DxvkFence(m_vkd);
    
    VkCommandBuffer commandBuffer = commandList->handle();
    VkSemaphore     waitSemaphore = VK_NULL_HANDLE;
    VkSemaphore     wakeSemaphore = VK_NULL_HANDLE;
    
    if (waitSync != nullptr) {
      waitSemaphore = waitSync->handle();
      commandList->trackResource(waitSync);
    }
    
    if (wakeSync != nullptr) {
      wakeSemaphore = wakeSync->handle();
      commandList->trackResource(wakeSync);
    }
    
    const VkPipelineStageFlags waitStageMask
      = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    
    VkSubmitInfo info;
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext                = nullptr;
    info.waitSemaphoreCount   = waitSemaphore == VK_NULL_HANDLE ? 0 : 1;
    info.pWaitSemaphores      = &waitSemaphore;
    info.pWaitDstStageMask    = &waitStageMask;
    info.commandBufferCount   = commandBuffer == VK_NULL_HANDLE ? 0 : 1;
    info.pCommandBuffers      = &commandBuffer;
    info.signalSemaphoreCount = wakeSemaphore == VK_NULL_HANDLE ? 0 : 1;
    info.pSignalSemaphores    = &wakeSemaphore;
    
    if (m_vkd->vkQueueSubmit(m_graphicsQueue, 1, &info, fence->handle()) != VK_SUCCESS)
      throw DxvkError("DxvkDevice::submitCommandList: Command submission failed");
    
    // TODO Store fence + command list pairs in a ring buffer
    fence->wait(std::numeric_limits<uint64_t>::max());
    commandList->reset();
    return fence;
  }
  
  
  void DxvkDevice::waitForIdle() const {
    TRACE(this);
    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      throw DxvkError("DxvkDevice::waitForIdle: Operation failed");
  }
  
}