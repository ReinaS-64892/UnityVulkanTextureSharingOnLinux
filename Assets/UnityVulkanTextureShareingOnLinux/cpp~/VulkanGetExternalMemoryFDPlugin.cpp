#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "Unity/IUnityInterface.h"

#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsVulkan.h"
#include <unistd.h>
#include <fstream>

// Original from MIT License Copyright (c) 2016, Unity Technologies https://github.com/Unity-Technologies/NativeRenderingPlugin/blob/f703c47a140d343c2c863a36d1aa5832586f3aaa/PluginSource/source/RenderAPI_Vulkan.cpp#L15-L58
#define UNITY_USED_VULKAN_API_FUNCTIONS(apply)  \
    apply(vkCreateInstance);                    \
    apply(vkCmdBeginRenderPass);                \
    apply(vkCreateBuffer);                      \
    apply(vkGetPhysicalDeviceMemoryProperties); \
    apply(vkGetBufferMemoryRequirements);       \
    apply(vkMapMemory);                         \
    apply(vkBindBufferMemory);                  \
    apply(vkAllocateMemory);                    \
    apply(vkDestroyBuffer);                     \
    apply(vkFreeMemory);                        \
    apply(vkUnmapMemory);                       \
    apply(vkQueueWaitIdle);                     \
    apply(vkDeviceWaitIdle);                    \
    apply(vkCmdCopyBufferToImage);              \
    apply(vkFlushMappedMemoryRanges);           \
    apply(vkCreatePipelineLayout);              \
    apply(vkCreateShaderModule);                \
    apply(vkDestroyShaderModule);               \
    apply(vkCreateGraphicsPipelines);           \
    apply(vkCmdBindPipeline);                   \
    apply(vkCmdDraw);                           \
    apply(vkCmdPushConstants);                  \
    apply(vkCmdBindVertexBuffers);              \
    apply(vkDestroyPipeline);                   \
    apply(vkDestroyPipelineLayout);             \
    apply(vkGetMemoryFdKHR);                    \
    apply(vkCreateImage);                       \
    apply(vkGetImageMemoryRequirements);        \
    apply(vkCmdCopyImage);                      \
    apply(vkAllocateCommandBuffers);            \
    apply(vkDestroyImage);                      \
    apply(vkResetCommandBuffer);                \
    apply(vkBindImageMemory);

#define VULKAN_DEFINE_API_FUNCPTR(func) static PFN_##func func
VULKAN_DEFINE_API_FUNCPTR(vkGetInstanceProcAddr);
UNITY_USED_VULKAN_API_FUNCTIONS(VULKAN_DEFINE_API_FUNCPTR);
#undef VULKAN_DEFINE_API_FUNCPTR

static void LoadVulkanAPI(PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkInstance instance)
{
    if (!vkGetInstanceProcAddr && getInstanceProcAddr)
        vkGetInstanceProcAddr = getInstanceProcAddr;

    if (!vkCreateInstance)
        vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");

#define LOAD_VULKAN_FUNC(fn) \
    if (!fn)                 \
    fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn)
    UNITY_USED_VULKAN_API_FUNCTIONS(LOAD_VULKAN_FUNC);
#undef LOAD_VULKAN_FUNC
}

// Original from MIT License Copyright (c) 2016, Unity Technologies https://github.com/Unity-Technologies/NativeRenderingPlugin/blob/f703c47a140d343c2c863a36d1aa5832586f3aaa/PluginSource/source/RenderAPI_Vulkan.cpp#L96-L113
static int FindMemoryTypeIndex(VkPhysicalDeviceMemoryProperties const &physicalDeviceMemoryProperties, VkMemoryRequirements const &memoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags)
{
    uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;

    // Search memtypes to find first index with those properties
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex)
    {
        if ((memoryTypeBits & 1) == 1)
        {
            // Type is available, does it match user properties?
            if ((physicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
                return memoryTypeIndex;
        }
        memoryTypeBits >>= 1;
    }

    return -1;
}

static IUnityInterfaces *s_UnityInterfaces = NULL;
static IUnityGraphics *s_Graphics = NULL;
static IUnityGraphicsVulkanV2 *s_UnityInterfaceVulkan = NULL;

static bool s_IsVulkan = false;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *unityInterfaces)
{
    s_UnityInterfaces = unityInterfaces;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DisposeExportTexture();

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    // リークしないために
    DisposeExportTexture();
}

typedef struct TextureHolder
{
    bool initialized;

    VkImage vkImage;
    VkDeviceMemory vkMemory;

    int thisPid;
    int exportFileDescriptor;

} TextureHolder;

static TextureHolder s_holder = {};

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ExportTexture(void *renderTextureHandle)
{
    if (s_UnityInterfaces == NULL)
    {
        return -6;
    }
    if (s_Graphics == NULL)
    {
        s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
        s_IsVulkan = s_Graphics->GetRenderer() == kUnityGfxRendererVulkan;

        if (s_IsVulkan)
        {
            s_UnityInterfaceVulkan = s_UnityInterfaces->Get<IUnityGraphicsVulkanV2>();

            UnityVulkanInstance uvInstance = s_UnityInterfaceVulkan->Instance();
            LoadVulkanAPI(uvInstance.getInstanceProcAddr, uvInstance.instance);
        }
    }

    if (s_IsVulkan == false)
    {
        return -1;
    }

    UnityVulkanInstance uvInstance = s_UnityInterfaceVulkan->Instance();
    VkDevice device = uvInstance.device;

    s_UnityInterfaceVulkan->EnsureOutsideRenderPass();

    VkImage image = *reinterpret_cast<VkImage *>(renderTextureHandle);

    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (512 * 512 * 4) + (256 * 256 * 4) + (128 * 128 * 4) + (64 * 64 * 4) + (32 * 32 * 4) + (16 * 16 * 4) + (8 * 8 * 4) + (4 * 4 * 4) + (2 * 2 * 4) + (1 * 1 * 4);

    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - 4;
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (16 * 16 * 4);//(-1024)
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (2048);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (2048 + 1024);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (2048 + 1024 + 512);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 64);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 128);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 256 - 256);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 256 - 128 - 64 - 32);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 256 - 128 - 64);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 256 - 128);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 256 - 64);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 256);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) - (4096 - 512);

    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) + (-4096 + 512);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) + (-4096 + 128);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) + (-4096 + 64);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) + (-4096 + 32);


    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) + (-4096 + 4);
    // --- ↑ up ok --- ↓ down ng size
    // おそらく 4206592 が境界だと思しき
    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (64 * 64 * 4) + (-4096);

    // VkDeviceSize deviceSize = (1024 * 1024 * 4) + (32 * 32 * 4);
    // VkDeviceSize deviceSize = (1024 * 1024 * 4);

    VkPhysicalDeviceMemoryProperties physicalDeviceProperties;
    vkGetPhysicalDeviceMemoryProperties(uvInstance.physicalDevice, &physicalDeviceProperties);

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, image, &requirements);

    int memTypeIndex = FindMemoryTypeIndex(physicalDeviceProperties, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memTypeIndex < 0)
    {
        return -3;
    }

    VkDeviceMemory exportSourceMemory = {};
    VkExportMemoryAllocateInfo exportInfo = {};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &exportInfo;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memTypeIndex;
    vkAllocateMemory(device, &allocInfo, nullptr, &exportSourceMemory);

    VkResult bindResult = vkBindImageMemory(device, image, exportSourceMemory, 0);
    if (bindResult != VK_SUCCESS)
    {
        return -4;
    }

    int fd = -1;
    VkMemoryGetFdInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    info.pNext = nullptr;
    info.memory = exportSourceMemory;
    info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkResult getfdResult = vkGetMemoryFdKHR(device, &info, &fd);
    if (getfdResult != VK_SUCCESS)
    {
        return -5;
    }

    // result write
    s_holder.initialized = true;

    s_holder.vkImage = image;
    s_holder.vkMemory = exportSourceMemory;

    s_holder.thisPid = getpid();
    s_holder.exportFileDescriptor = fd;

    return 0;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetExportFileDescriptor()
{
    if (s_holder.initialized == false)
    {
        return -1;
    }
    return s_holder.exportFileDescriptor;
}
extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ThisPID()
{
    if (s_holder.initialized == false)
    {
        return -1;
    }
    return s_holder.thisPid;
}

// extern "C" void *UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SharedTextureNativePtr()
// {
//     if (s_holder.initialized == false)
//     {
//         return nullptr;
//     }

//     return &s_holder.vkImage;
// }
// extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CopyToExportedTexture()
// {
//     if (s_holder.initialized == false)
//     {
//         return -1;
//     }
//     if (s_holder.sourcePtr == nullptr)
//     {
//         return -2;
//     }

//     UnityVulkanInstance uvInstance = s_UnityInterfaceVulkan->Instance();
//     VkDevice device = uvInstance.device;

//     s_UnityInterfaceVulkan->EnsureOutsideRenderPass();

//     UnityVulkanImage unityVulkanImage = {};
//     VkImageLayout imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//     VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
//     VkAccessFlags accessFlags = VK_ACCESS_TRANSFER_READ_BIT;
//     UnityVulkanResourceAccessMode accessMode = kUnityVulkanResourceAccess_PipelineBarrier;
//     if (!s_UnityInterfaceVulkan->AccessTexture(s_holder.sourcePtr, UnityVulkanWholeImage, imageLayout, pipelineStageFlags, accessFlags, accessMode, &unityVulkanImage))
//     {
//         return -3;
//     }

//     UnityVulkanRecordingState recordingState;
//     if (!s_UnityInterfaceVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
//     {
//         return -4;
//     }

//     if (recordingState.commandBuffer == VK_NULL_HANDLE)
//     {
//         return -5;
//     }

//     VkImageCopy copyRange;
//     copyRange.extent.width = 1024;
//     copyRange.extent.height = 1024;
//     copyRange.extent.depth = 1;
//     vkCmdCopyImage(recordingState.commandBuffer, unityVulkanImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s_holder.vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRange);
//     return 0;
// }
// static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
// {
//     int result = CopyToExportedTexture();

//     std::ofstream outfile;
//     outfile.open("/home/Reina/Rs/UnityProject/UnityVulkanTextureSharingOnLinux/Library/UVTSOL-Log.txt", std::ios::app);

//     outfile << std::to_string(result);
//     outfile << "\n";

//     outfile.close();
// }
// extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
// {
//     return OnRenderEvent;
// }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DisposeExportTexture()
{
    if (s_holder.initialized == false)
    {
        return;
    }

    UnityVulkanInstance uvInstance = s_UnityInterfaceVulkan->Instance();
    VkDevice device = uvInstance.device;

    vkDestroyImage(device, s_holder.vkImage, nullptr);
    vkFreeMemory(device, s_holder.vkMemory, nullptr);

    s_holder = {};
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DebugCall()
{
    return 0;
}
