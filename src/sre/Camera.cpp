/*
 *  SimpleRenderEngine (https://github.com/mortennobel/SimpleRenderEngine)
 *
 *  Created by Morten Nobel-Jørgensen ( http://www.nobel-joergensen.com/ )
 *  License: MIT
 */

#include "sre/Camera.hpp"

#include "sre/impl/GL.hpp"

#include "sre/Renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <sre/Camera.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "sre/Log.hpp"

namespace sre {
    Camera::Camera()
    : viewTransform{1.0f}
    {
        projectionValue.orthographic.orthographicSize = 1;
        projectionValue.orthographic.nearPlane = -1;
        projectionValue.orthographic.farPlane = 1;
    }

    void Camera::setPerspectiveProjection(float fieldOfViewY, float nearPlane, float farPlane) {
        projectionValue.perspective.fieldOfViewY = glm::radians( fieldOfViewY);
        projectionValue.perspective.nearPlane    = nearPlane;
        projectionValue.perspective.farPlane     = farPlane;
        projectionType = ProjectionType::Perspective;
    }

    void Camera::setOrthographicProjection(float orthographicSize, float nearPlane, float farPlane) {
        projectionValue.orthographic.orthographicSize  = orthographicSize;
        projectionValue.orthographic.nearPlane = nearPlane;
        projectionValue.orthographic.farPlane  = farPlane;
        projectionType = ProjectionType::Orthographic;
    }

    void Camera::setWindowCoordinates(){
        projectionType = ProjectionType::OrthographicWindow;
    }

    void Camera::lookAt(glm::vec3 eye, glm::vec3 at, glm::vec3 up) {
        if (glm::length(eye-at)<std::numeric_limits<float>::epsilon()){
            auto eyeStr = glm::to_string(eye);
            auto atStr = glm::to_string(at);
            LOG_WARNING("Camera::lookAt() invalid parameters. eye (%s) must be different from at (%s)",eyeStr.c_str(),atStr.c_str());
        }
        setViewTransform(glm::lookAt<float>(eye, at, up));
    }

    glm::mat4 Camera::getViewTransform() {
        return viewTransform;
    }

    glm::mat4 Camera::getProjectionTransform(glm::uvec2 viewportSize) {
        switch (projectionType){
            case ProjectionType::Custom:
                return glm::make_mat4(projectionValue.customProjectionMatrix);
            case ProjectionType::Orthographic:
            {
                float aspect = viewportSize.x/(float)viewportSize.y;
                float sizeX = aspect * projectionValue.orthographic.orthographicSize;
                return glm::ortho<float>	(-sizeX, sizeX, -projectionValue.orthographic.orthographicSize, projectionValue.orthographic.orthographicSize, projectionValue.orthographic.nearPlane, projectionValue.orthographic.farPlane);
            }
            case ProjectionType::OrthographicWindow:
                return glm::ortho<float>	(0, float(viewportSize.x), 0, float(viewportSize.y), 1.0f,-1.0f);
            case ProjectionType::Perspective:
                return glm::perspectiveFov<float>(projectionValue.perspective.fieldOfViewY,
                                                  float(viewportSize.x),
                                                  float(viewportSize.y),
                                                  projectionValue.perspective.nearPlane,
                                                  projectionValue.perspective.farPlane);
            default:
                return glm::mat4(1);
        }
    }

    glm::mat4 Camera::getInfiniteProjectionTransform(glm::uvec2 viewportSize) {
        switch (projectionType){
            case ProjectionType::Perspective:
                return glm::tweakedInfinitePerspective(projectionValue.perspective.fieldOfViewY,float(viewportSize.x)/float(viewportSize.y),projectionValue.perspective.nearPlane);
            default:
                return getProjectionTransform(viewportSize);
        }
    }

    void Camera::setViewTransform(const glm::mat4 &viewTransform) {
        Camera::viewTransform = viewTransform;
    }

    void Camera::setProjectionTransform(const glm::mat4 &projectionTransform) {
        memcpy(projectionValue.customProjectionMatrix, glm::value_ptr(projectionTransform), sizeof(glm::mat4));
        projectionType = ProjectionType::Custom;
    }

    void Camera::setViewport(glm::vec2 offset, glm::vec2 size) {
        viewportOffset = offset;
        viewportSize = size;
    }

    void Camera::setPositionAndRotation(glm::vec3 position, glm::vec3 rotationEulersDegrees) {
        auto rotationEulersRadians = glm::radians(rotationEulersDegrees);
        auto viewTransform = glm::translate(position) * glm::eulerAngleXYZ(rotationEulersRadians.x, rotationEulersRadians.y, rotationEulersRadians.z);
        setViewTransform(glm::inverse(viewTransform));
    }

    glm::vec3 Camera::getPosition() {
        glm::vec3 scale;
        glm::quat orientation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(glm::inverse(viewTransform),
                scale,
                orientation,
                translation,
                skew,
                perspective);
        return translation;
    }

    glm::vec3 Camera::getRotationEuler() {
        glm::vec3 scale;
        glm::quat orientation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(glm::inverse(viewTransform),
                           scale,
                           orientation,
                           translation,
                           skew,
                           perspective);

        return glm::degrees( -glm::eulerAngles(orientation));
    }

    std::array<glm::vec3, 2> Camera::screenPointToRay(glm::vec2 position) {
        glm::vec2 scaledWindowsSize = (glm::vec2)Renderer::instance->getWindowSize() * viewportSize;

        position = (position / scaledWindowsSize - viewportOffset/viewportSize)*2.0f-glm::vec2(1.0f);

        auto viewProjection = getProjectionTransform(scaledWindowsSize) * viewTransform;
        auto invViewProjection = glm::inverse(viewProjection);

        glm::vec4 originClipSpace{position,-1,1};
        glm::vec4 destClipSpace{position,1,1};
        glm::vec4 originClipSpaceWS = invViewProjection * originClipSpace;
        glm::vec4 destClipSpaceWS   = invViewProjection * destClipSpace;
        glm::vec3 originClipSpaceWS3 = glm::vec3(originClipSpaceWS)/originClipSpaceWS.w;
        glm::vec3 destClipSpaceWS3   = glm::vec3(destClipSpaceWS)/destClipSpaceWS.w;

        return {originClipSpaceWS3, glm::normalize(destClipSpaceWS3-originClipSpaceWS3)};

    }

// CustomCamera Class Method Implementations ==================================

CustomCamera::CustomCamera():Camera(),
							 position {0.0, 0.0, 0.0},
							 direction {0.0, 0.0, -1.0},
							 up {0.0, 1.0, 0.0},
							 speed {1.0f},
							 rotationSpeed {1.0f},
							 fieldOfView {45.0f},
							 nearPlane {0.1f},
							 farPlane {100.0f} {
	maxFieldOfView = fieldOfView;
}	
	
void CustomCamera::init() {
	direction = glm::normalize(direction);
	up = glm::normalize(up);
	right = glm::cross(direction, up);
    setPerspectiveProjection(fieldOfView, nearPlane, farPlane);
    lookAt(position, position + direction, up);
}

float CustomCamera::getSpeed() {
	return speed;
}

void CustomCamera::setSpeed(float speedIn) {
	speed = speedIn;
}

float CustomCamera::getRotationSpeed() {
	return rotationSpeed;
}

void CustomCamera::setRotationSpeed(float rotationSpeedIn) {
	rotationSpeed = rotationSpeedIn;
}

float CustomCamera::getFieldOfView() {
	return fieldOfView;
}

void CustomCamera::setFieldOfView(float fieldOfViewIn) {
	fieldOfView = fieldOfViewIn;
}

float CustomCamera::getMaxFieldOfView() {
	return maxFieldOfView;
}

void CustomCamera::setMaxFieldOfView(float maxFieldOfViewIn) {
	maxFieldOfView = maxFieldOfViewIn;
}

float CustomCamera::getNearPlane() {
	return nearPlane;
}

void CustomCamera::setNearPlane(float nearPlaneIn) {
	nearPlane = nearPlaneIn;
}

float CustomCamera::getFarPlane() {
	return farPlane;
}

void CustomCamera::setFarPlane(float farPlaneIn) {
	farPlane = farPlaneIn;
}

void CustomCamera::zoom(float zoomIncrement) {
	fieldOfView -= zoomIncrement; // Decreasing the FOV increases the "zoom"
	fieldOfView = glm::min(fieldOfView, maxFieldOfView);
	fieldOfView = glm::max(fieldOfView, 1.0f);
	setPerspectiveProjection(fieldOfView, nearPlane, farPlane);
}

// FlightCamera Class Method Implementations ==================================

FlightCamera::FlightCamera(): CustomCamera() {
}	
	
void
FlightCamera::move(float distance) {
	// Add the movement vector to the camera position
	position += distance * direction;
	// Set the camera view transform
	lookAt(position, position + direction, up);
}

void
FlightCamera::pitchAndYaw(float pitchIncrement, float yawIncrement) {
    // Note that pitch and yaw are expected to be in degrees
    float pitch = glm::radians(pitchIncrement);
    float yaw = glm::radians(yawIncrement);
    // Rotate direction & up vectors according to pitch around cameraRight
    direction = glm::normalize(direction + glm::tan(pitch)*up);
    up = glm::cross(right, direction);
    // Rotate cameraDir & cameraRight according to yaw around cameraUp
    direction = glm::normalize(direction + glm::tan(yaw)*right);
    right = glm::cross(direction, up);
    // Calculate the view transform
    lookAt(position, position + direction, up);
}

void
FlightCamera::roll(float rollIncrement) {
    // Note that roll is in degrees, and that what the camera sees will
    // roll in the opposite direction of the camera
    float roll = glm::radians(rollIncrement);
    // Rotate up & right vectors according to roll around direction vector
    up = glm::normalize(up + glm::tan(roll)*right);
    right = glm::cross(direction, up);
    // Calculate the view transform (note that direction vector does not change)
    lookAt(position, position + direction, up);
}

FlightCamera::FlightCameraBuilder FlightCamera::create() {
	return FlightCameraBuilder();
}

FlightCamera::FlightCameraBuilder::FlightCameraBuilder()
{
	camera = new FlightCamera();
}

FlightCamera::FlightCameraBuilder::~FlightCameraBuilder()
{
	delete camera;
}

FlightCamera FlightCamera::FlightCameraBuilder::build() {
	camera->init();
	FlightCamera copyOfCamera = *camera;
	return copyOfCamera;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withPosition(glm::vec3 position) {
	camera->position = position;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withDirection(glm::vec3 direction) {
	camera->direction = direction;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withUpDirection(glm::vec3 upDirection) {
	camera->up = upDirection;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withSpeed(float speed) {
	camera->speed = speed;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withRotationSpeed(float rotationSpeed) {
	camera->rotationSpeed = rotationSpeed;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withFieldOfView(float fieldOfView) {
	camera->fieldOfView = fieldOfView;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withMaxFieldOfView(float maxFieldOfView) {
	camera->maxFieldOfView = maxFieldOfView;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withNearPlane(float nearPlane) {
	camera->nearPlane = nearPlane;
	return *this;
}

FlightCamera::FlightCameraBuilder&
FlightCamera::FlightCameraBuilder::withFarPlane(float farPlane) {
	camera->farPlane = farPlane;	
	return *this;
}

// FPS_Camera Class Method Implementations =====================================

FPS_Camera::FPS_Camera(): CustomCamera() {
}	
	
void FPS_Camera::init() {
	direction = glm::normalize(direction);
	up = glm::normalize(up);
	worldUp = up;
	right = glm::cross(direction, up);
	forward = direction - glm::dot(direction, worldUp) * worldUp;
	forwardLen = glm::length(forward);
	forward = glm::normalize(forward);
	CustomCamera::init();
}

void FPS_Camera::move(float distance, Direction directionToMove) {
	glm::vec3 moveDirection;
	switch(directionToMove) {
		case Direction::Forward:
			moveDirection = forward;	
			break;
		case Direction::Backward:
			moveDirection = -forward;	
			break;
		case Direction::Left:
			moveDirection = -right;	
			break;
		case Direction::Right:
			moveDirection = right;	
			break;
		case Direction::Up:
			moveDirection = up;	
			break;
		case Direction::Down:
			moveDirection = -up;	
			break;
	}

	// Add the movement vector to the camera position
	position += distance * moveDirection;
	// Set the camera view transform
	lookAt(position, position + direction, up);
}

void FPS_Camera::pitchAndYaw(float pitchIncrement, float yawIncrement) {
    // Note that pitch and yaw are expected to be in degrees
    float pitch = glm::radians(pitchIncrement);
	// Scale the amount of yaw by the un-normalized length of the forward vector
    float yaw = forwardLen * glm::radians(yawIncrement);

    // Rotate direction & up according to pitch around right
    direction = glm::normalize(direction + glm::tan(pitch)*up);
    up = glm::cross(right, direction);

	// Check if the angle theta between forward and up > thetaMax
	float cosThetaMax = glm::cos(glm::radians(89.9));
	auto cosTheta = glm::dot(forward, direction);
	if (cosTheta < cosThetaMax) { // cos(theta) gets smaller as theta > 90
		direction = direction + (cosThetaMax - cosTheta) * forward;
		direction = glm::normalize(direction);
		up = glm::cross(right, direction);
	}

    // Rotate direction & right according to yaw around up vector
    direction = glm::normalize(direction + glm::tan(yaw)*right);
	// Use worldUp to ensure that right vector stays on horizontal plane
    right = glm::cross(direction, worldUp);
	// Normalize because cross product was not with ortho-normal vectors
	right = glm::normalize(glm::cross(direction, worldUp));

	// Adjust up vector to be consistent with right and direction
	up = glm::cross(right, direction);

	// Calculate projection and length of direction onto horizontal plane 
	forward = direction - glm::dot(direction, worldUp) * worldUp;
	forwardLen = glm::length(forward);
	forward = glm::normalize(forward);

    // Calculate the view transform
    lookAt(position, position + direction, up);
}

FPS_Camera::FPS_CameraBuilder FPS_Camera::create() {
	return FPS_CameraBuilder();
}

FPS_Camera::FPS_CameraBuilder::FPS_CameraBuilder()
{
	camera = new FPS_Camera();
}

FPS_Camera::FPS_CameraBuilder::~FPS_CameraBuilder()
{
	delete camera;
}

FPS_Camera FPS_Camera::FPS_CameraBuilder::build() {
	camera->init();
	FPS_Camera copyOfCamera = *camera;
	return copyOfCamera;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withPosition(glm::vec3 position) {
	camera->position = position;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withDirection(glm::vec3 direction) {
	camera->direction = direction;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withUpDirection(glm::vec3 upDirection) {
	camera->up = upDirection;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withWorldUpDirection(glm::vec3 worldUp) {
	camera->worldUp = worldUp;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withSpeed(float speed) {
	camera->speed = speed;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withRotationSpeed(float rotationSpeed) {
	camera->rotationSpeed = rotationSpeed;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withFieldOfView(float fieldOfView) {
	camera->fieldOfView = fieldOfView;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withMaxFieldOfView(float maxFieldOfView) {
	camera->maxFieldOfView = maxFieldOfView;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withNearPlane(float nearPlane) {
	camera->nearPlane = nearPlane;
	return *this;
}

FPS_Camera::FPS_CameraBuilder&
FPS_Camera::FPS_CameraBuilder::withFarPlane(float farPlane) {
	camera->farPlane = farPlane;	
	return *this;
}

}
