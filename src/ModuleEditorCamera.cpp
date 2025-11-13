#include "ModuleEditorCamera.h"
#include "ModuleInput.h"
#include "ModuleWindow.h"
#include "glm/glm.hpp"
#include "glm/ext/matrix_transform.hpp"

ModuleEditorCamera::ModuleEditorCamera(ModuleInput* input, ModuleWindow* window) : mInput(input), mWindow(window)
{
}

ModuleEditorCamera::~ModuleEditorCamera()
{
}

bool ModuleEditorCamera::Init()
{
	//camera.LookAt(glm::vec3(200.0f, 200.0f, 130.0f), glm::vec3(30.0f, 60.0f, 0.0f));
	camera.LookAt(glm::vec3(200.0f, 200.0f, 200.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	camera.SetPerspective(glm::radians(45.0f), static_cast<float>(mWindow->GetWidth()) / static_cast<float>(mWindow->GetHeight()), 0.1f, 10000.0f);
	return true;
}

UpdateStatus ModuleEditorCamera::PreUpdate(float dt)
{
	//TODO: add the delta time to the inputs to work equal on different framerates
	float speed = 500 * dt;
	if (mInput->GetKey(SDL_SCANCODE_LSHIFT) == KeyState::KEY_REPEAT || mInput->GetKey(SDL_SCANCODE_RSHIFT) == KeyState::KEY_REPEAT)
		speed *= 3.0f;
	//Pan
	if (mInput->GetKey(SDL_SCANCODE_LEFT) == KeyState::KEY_REPEAT)
		camera.Translate(glm::vec3(speed, 0.0f, 0.0f));
	if (mInput->GetKey(SDL_SCANCODE_RIGHT) == KeyState::KEY_REPEAT)
		camera.Translate(glm::vec3(-speed, 0.0f, 0.0f));
	if (mInput->GetKey(SDL_SCANCODE_DOWN) == KeyState::KEY_REPEAT)
		camera.Translate(glm::vec3(0.0f, -speed, 0.0f));
	if (mInput->GetKey(SDL_SCANCODE_UP) == KeyState::KEY_REPEAT)
		camera.Translate(glm::vec3(0.0f, speed, 0.0f));

	if (mInput->GetKey(SDL_SCANCODE_A) == KeyState::KEY_REPEAT)
		camera.Translate(camera.GetRight() * -speed);
	if (mInput->GetKey(SDL_SCANCODE_D) == KeyState::KEY_REPEAT)
		camera.Translate(camera.GetRight() * speed);
	if (mInput->GetKey(SDL_SCANCODE_S) == KeyState::KEY_REPEAT)
		camera.Translate(camera.GetFoward() * -speed);
	if (mInput->GetKey(SDL_SCANCODE_W) == KeyState::KEY_REPEAT)
		camera.Translate(camera.GetFoward() * speed);

	float motionX, motionY;
	if (mInput->GetMouseKey(MouseKey::BUTTON_RIGHT) == KeyState::KEY_REPEAT && mInput->GetMouseMotion(motionX, motionY))
	{
		//TODO: Handle camera rotation
		//glm::rotate(glm::mat3(1.0f), camera.GetRight(), glm::radians(motionY));
		//float3x3 rotationX = float3x3::RotateAxisAngle(mEditorCameraGameObject->GetRight(), DegToRad(mY));
		//float3x3 rotationY = float3x3::RotateAxisAngle(float3::unitY, DegToRad(-mX));
		//float3x3 rotation = rotationY.Mul(rotationX);
		//
		//Quat quatOriginal = mEditorCameraGameObject->GetWorldRotation();
		//Quat newQuat = Quat(rotation);
		//newQuat = newQuat * quatOriginal;
		////float3 eulerRotation = newQuat.ToEulerXYZ();
		//mEditorCameraGameObject->SetWorldRotation(newQuat);

	}

	return UpdateStatus::UPDATE_CONTINUE;
}

void Camera::SetPerspective(float fovy, float aspectRatio, float near, float far)
{
	nearPlane = near;
	farPlane = far; this->fovy = fovy;
	fovx = 2.0f * glm::atan(glm::tan(fovy * 0.5f) / aspectRatio);
	proj = glm::perspective(fovy, aspectRatio, near, far);
	////Edits to perspective matrix for vulkan: the projection matrix does not need a -1 because it does not need to flip the z to arrive al clip coordinates
	////vulkan perspective matrix is right handed but aplying a 180 degree rotation on the X axis
	//proj[2][3] = 1.0f;// this glm rotate function does not influence the last row so we manually set the -1
	//proj = glm::rotate(proj, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 rotateX180
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	const float halfFovTangent = glm::tan(fovy * 0.5f);
	glm::mat4 perspectiveProj
	{
		(1.0f / aspectRatio) / halfFovTangent, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / halfFovTangent, 0.0f, 0.0f,
		0.0f, 0.0f, far / (far - near), 1.0f,
		0.0f, 0.0f, -(near * far) / (far - near), 0.0f
	};
	proj = perspectiveProj * rotateX180;
}
void Camera::LookAt(const glm::vec3& position, const glm::vec3& target)
{
	pos = position;
	view = glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
}
void Camera::LookAt(const glm::vec3& target)
{
	view = glm::lookAt(pos, target, GetUp());
}
void Camera::Translate(const glm::vec3& newVector)
{
	pos += newVector;
	view[3][0] = -glm::dot(GetRight(), pos);
	view[3][1] = -glm::dot(GetUp(), pos);
	view[3][2] = glm::dot(GetFoward(), pos);
}

glm::vec4 Camera::NearPlane() const
{
	const glm::vec3 foward = GetFoward();
	return glm::vec4(-foward, glm::dot(pos + foward * nearPlane, -foward));
}

glm::vec4 Camera::FarPlane() const
{
	const glm::vec3 foward = GetFoward();
	return glm::vec4(foward, glm::dot(pos + foward * farPlane, foward));
}

glm::vec4 Camera::LeftPlane() const
{
	//glm::vec3 left = glm::cross(GetUp(), GetFoward());
	glm::vec3 left = -GetRight();
	//glm::normalize(left) * glm::tan(fovy * 0.5f);
	left *= glm::tan(fovy * 0.5f);
	const glm::vec3 leftSide = GetFoward() + left;
	const glm::vec3 leftSideNormal = glm::normalize(glm::cross(GetUp(), leftSide));
	return glm::vec4(leftSideNormal, glm::dot(pos, leftSideNormal));
}

glm::vec4 Camera::RightPlane() const
{
	glm::vec3 right = GetRight();
	//glm::normalize(right) * glm::tan(fovy * 0.5f);
	right *= glm::tan(fovy * 0.5f);
	const glm::vec3 rightSide = GetFoward() + right;
	const glm::vec3 rightSideNormal = glm::normalize(glm::cross(rightSide, GetUp()));
	return glm::vec4(rightSideNormal, glm::dot(pos, rightSideNormal));
}

glm::vec4 Camera::TopPlane() const
{
	const glm::vec3 topSide = GetFoward() + glm::tan(fovx * 0.5f) * GetUp();
	const glm::vec3 topSideNormal = glm::normalize(glm::cross(GetRight(), topSide));
	return glm::vec4(topSideNormal, glm::dot(pos, topSideNormal));
}

glm::vec4 Camera::BottomPlane() const
{
	const glm::vec3 bottomSide = GetFoward() - glm::tan(fovx * 0.5f) * GetUp();
	//float3 left = glm::cross(GetUp(), GetFoward());
	const glm::vec3 bottomSideNormal = glm::normalize(glm::cross(-GetRight(), bottomSide));
	return glm::vec4(bottomSideNormal, glm::dot(pos, bottomSideNormal));
}

void Camera::GetPlanes(glm::vec4(&planes)[6]) const
{
	planes[0] = NearPlane();
	planes[1] = FarPlane();
	planes[2] = LeftPlane();
	planes[3] = RightPlane();
	planes[4] = TopPlane();
	planes[5] = BottomPlane();
}
