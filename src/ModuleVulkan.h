#ifndef __MODULE_VULKAN_H__
#define __MODULE_VULKAN_H__

#include "Module.h"

class ModuleWindow;
class ModuleEditorCamera;
#include "vulkan/vulkan.h"
#include "glm/fwd.hpp"
//struct VkInstance;


struct Vertex
{
	//the 4th component of each is just for alignment purposes
	float position[4];
	float normal[4];
};

struct Mesh
{
	unsigned int numIndices;
	unsigned int* indices;
	unsigned int numVertices;
	Vertex* vertices;
};

struct meshopt_Meshlet;
struct meshopt_Bounds;
struct MeshletMesh 
{
	meshopt_Meshlet* meshlets;
	meshopt_Bounds* meshletBounds;
	unsigned int* meshletVertices;
	unsigned char* meshletTriangles;
	size_t meshletCount;
	size_t maxMeshlets;
	Mesh mesh;
	unsigned int GetMeshletsVerticeCount();
	unsigned int GetMeshletsTriangleCount();
};

#include "glm/vec3.hpp"
class AABB
{
public:
	AABB() = default;
	AABB(const Mesh& mesh) { Generate(mesh); }
	void Generate(const Mesh& mesh);
	void GetPoints(glm::vec3(&points)[8]) const;
private:
	glm::vec3 minPoint;
	glm::vec3 maxPoint;
};

class ModuleVulkan final : public Module
{
public:

	ModuleVulkan(ModuleWindow* mWin, ModuleEditorCamera* mCamera);
	~ModuleVulkan();

	bool Init() override;
	UpdateStatus PostUpdate(float dt) override;
	bool CleanUp() override;
	void SetModelMatrix(const glm::mat4& model);
	void SetCameraInfo(const glm::mat4& viewProj, const glm::vec3& cameraPos);

	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	static constexpr int NUM_MODELS = 100000;
private:
	static bool CheckVulkanExtensionsSupport(const char* const* ppEnabledExtensionNames, const unsigned int enabledExtensionCount);
	static bool CheckVulkanLayersSupport(const char* const* ppEnabledExtensionNames, const unsigned int enabledExtensionCount);
	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const char* const* ppEnabledExtensionNames, const unsigned int enabledExtensionCount);
	bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	bool CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numMeshlets);
	bool CreateSwapChain();
	bool CreateFrameBuffers();
	void DestroySwapChain();
	void DestroyFrameBuffers();
	void GenerateMeshlet(Mesh& mesh, MeshletMesh& meshletMesh) const;
	bool FindSupportedFormat(const VkFormat* candidates, size_t numCandidates, VkImageTiling tiling, VkFormatFeatureFlags features, VkFormat& out, VkPhysicalDevice* pDevice = nullptr);
	ModuleWindow* mWindow;
	ModuleEditorCamera* mCamera;
	VkInstance instance;
	const char** extensions;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue;
	int graphicsQueueFamilyIndex = 0;
	//TODO: Get the present Queue -> It could be different than the graphics queue
	//VkQueue presentQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	VkSurfaceFormatKHR swapChainSurfaceFormat;
	VkExtent2D swapChainExtent{0,0};
	VkImage* swapChainImages = nullptr;
	VkImageView* swapChainImageViews = nullptr;
	unsigned int swapChainImageCount = 0;
	VkPresentModeKHR swapChainPresentMode;
	VkFramebuffer* swapChainFramebuffers = nullptr;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipelineLayout cullPipelineLayout;
	VkPipeline graphicsPipeline;
	VkPipeline computePipeline;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
	VkFence frameFences[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore* renderFinishedSemaphores;
	uint32_t currentFrame = 0;
	uint32_t swapChainImageIndex = 0;
	uint32_t meshletMaxOutputVertices = 0;
	uint32_t meshletMaxOutputPrimitives = 0;
	uint32_t maxPreferredMeshWorkGroupInvocations = 0;
	uint32_t maxPreferredTaskWorkGroupInvocations = 0;
	VkDeviceSize minStorageBufferOffsetAlignment = 0;
	VkDeviceSize minUniformBufferOffsetAlignment = 0;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet* descriptorSets;
#ifndef NDEBUG
	VkDebugUtilsMessengerEXT debugMessenger;
	bool layersEnabled = false;
#endif // !NDEBUG
	VkBuffer meshletBuffer;
	VkBuffer meshletCullInfoBuffer;
	VkBuffer meshletVerticesBuffer;
	VkBuffer meshletTrianglesBuffer;
	VkBuffer vertexBuffer;
	VkBuffer transformsBuffer;
	VkDeviceMemory meshletBufferMemory;
	VkDeviceMemory meshletCullInfoBufferMemory;
	VkDeviceMemory meshletVerticesBufferMemory;
	VkDeviceMemory meshletTrianglesBufferMemory;
	VkDeviceMemory vertexBufferMemory;
	VkDeviceMemory transformsBufferMemory;
	void* transformsBufferPtr[MAX_FRAMES_IN_FLIGHT];

	VkBuffer numMeshletsBuffer;
	VkDeviceMemory numMeshletsBufferMemory;
	VkBuffer dispatchIndirectBuffer;
	VkDeviceMemory dispatchIndirectBufferMemory;
	VkBuffer modelIDsBuffer;
	VkDeviceMemory modelIDsBufferMemory;
	VkBuffer parameterBuffer;
	VkDeviceMemory parameterBufferMemory;
	void* parameterBufferPtr[MAX_FRAMES_IN_FLIGHT];
	VkBuffer frustumPlanesBuffer;
	VkDeviceMemory frustumPlanesBufferMemory;
	void* frustumPlanesBufferPtr[MAX_FRAMES_IN_FLIGHT];
	VkBuffer modelMatricesBuffer;
	VkDeviceMemory modelMatricesBufferMemory;
	void* modelMatricesBufferPtr[MAX_FRAMES_IN_FLIGHT];
	VkBuffer OBBsBuffer;
	VkDeviceMemory OBBsBufferMemory;
	void* OBBsBufferPtr[MAX_FRAMES_IN_FLIGHT];

	PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT = nullptr;
	PFN_vkCmdDrawMeshTasksIndirectEXT vkCmdDrawMeshTasksIndirectEXT = nullptr;
	PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT = nullptr;
	MeshletMesh meshletMesh;

	VkImage depthImage;
	VkFormat depthFormat;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	static unsigned int GetInbetweenAlignmentSpace(unsigned int structSize, unsigned int alignment) {
		return AlignedStructSize(structSize, alignment) - structSize;
	}
	static unsigned int AlignedStructSize(unsigned int structSize, unsigned int alignment)
	{
		return (structSize + (alignment - 1)) & ~(alignment - 1);
	}
	AABB modelAABB;
	glm::mat4* modelMatrices;
	
};

#endif // !__MODULE_WINDOW_H__