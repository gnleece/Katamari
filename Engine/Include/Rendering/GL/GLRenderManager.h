#pragma once

#include "Rendering/RenderManager.h"

class DebugDraw;

class GLRenderManager : public RenderManager
{
public:
    friend class RenderManager;
    friend class DebugDraw;

    virtual void Startup(int viewportWidth, int viewportHeight);
    virtual void Shutdown();

    virtual void SetRootObject(GameObjectBase* rootObject);
    virtual void SetLight(Light light);

    virtual Camera& GetCamera();
    virtual void SetCamera(Camera& camera);

    virtual void RenderScene();
    virtual void ApplyGlobalParams(ShaderProgram* shader);

    virtual int GetViewportWidth();
    virtual int GetViewportHeight();

    virtual bool SettingsDirty();

    virtual DebugDraw* GetDebugDraw();

    GLShaderProgram* GetCommonShader(eCommonShader name);

private:

    void            LoadCommonShaders();

    PlatSpecificRenderer* m_platSpecificRenderer;

    int             m_viewportWidth;
    int             m_viewportHeight;
    Camera          m_camera;                   // TODO support multiple cameras
    Light           m_light;                    // TODO support multiple light sources
    GameObjectBase* m_rootObject;

    bool            m_dirty;

    GLShaderProgram*  m_commonShaders[NUM_COMMON_SHADERS];

    GLDebugDraw*  m_debugDraw;
};