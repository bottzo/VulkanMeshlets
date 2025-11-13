#ifndef __MODULE_EDITOR_CAMERA_H__
#define __MODULE_EDITOR_CAMERA_H__
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "Module.h"

class ModuleInput;
class ModuleWindow;

class Camera
{
public:
	Camera() : pos(0.f), view(1.0f), proj(1.0f), nearPlane(0.0f), farPlane(0.0f), fovy(0.0f), fovx(0.0f) {};
	~Camera() {};

	void SetPerspective(float fovy, float aspectRatio, float near, float far);
	void LookAt(const glm::vec3& position, const glm::vec3& target);
	void LookAt(const glm::vec3& target);
	void Translate(const glm::vec3& newVector);
	//void Translate(glm::vec3 newVector) { LookAt() }
	glm::vec3 GetUp() const { return glm::vec3(view[0][1], view[1][1], view[2][1]); }
	glm::vec3 GetFoward() const { return glm::vec3(-view[0][2], -view[1][2], -view[2][2]); }
	glm::vec3 GetRight() const { return glm::vec3(view[0][0], view[1][0], view[2][0]); }
	const glm::vec3& GetPosition() const { return pos; }
	const glm::mat4& GetViewMatrix() const { return view; }
	const glm::mat4& GetProjectionMatrix() const { return proj; }
	void GetPlanes(glm::vec4(&planes)[6]) const;
	glm::vec4 Camera::NearPlane() const;
	glm::vec4 Camera::FarPlane() const;
	glm::vec4 Camera::LeftPlane() const;
	glm::vec4 Camera::RightPlane() const;
	glm::vec4 Camera::TopPlane() const;
	glm::vec4 Camera::BottomPlane() const;
private:

	glm::vec3 pos;
	glm::mat4 view;
	glm::mat4 proj;
	float nearPlane;
	float farPlane;
	float fovy;
	float fovx;
};

class ModuleEditorCamera: public Module {
public:
	ModuleEditorCamera(ModuleInput* mInput, ModuleWindow* mWindow);
	~ModuleEditorCamera();

	bool Init() override;
	UpdateStatus PreUpdate(float dt) override;
	
	const glm::vec3& GetPosition() const { return camera.GetPosition(); }
	void GetFrustumPlanes(glm::vec4(&planes)[6]) { camera.GetPlanes(planes); }
	void ChangeAspectRatio(float aspectRatio) { camera.SetPerspective(glm::radians(45.0f), aspectRatio, 0.1f, 10000.0f); }
	const glm::mat4& GetView() { return camera.GetViewMatrix(); }
	const glm::mat4& GetProj() { return camera.GetProjectionMatrix(); }
private:
	Camera camera;
	ModuleInput* mInput;
	ModuleWindow* mWindow;
};

#endif // !__MODULE_EDITOR_CAMERA_H__