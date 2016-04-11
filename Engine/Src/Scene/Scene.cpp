#include "Scene\Scene.h"

#include "Scene\ResourceManager.h"
#include "GameComponentFactory.h"
#include "GameObject.h"
#include "GameObjectBase.h"
#include "GameProject.h"
#include "ToolsideGameComponent.h"
#include "ToolsideGameObject.h"
#include "Util.h"
#include "Physics\Collider.h"
#include "Physics\CollisionEngine.h"
#include "Rendering\Camera.h"
#include "Rendering\Mesh.h"
#include "Rendering\MeshInstance.h"
#include "Rendering\RenderManager.h"
#include "Rendering\Texture.h"
#include "Serialization\HierarchicalSerializer.h"

using namespace tinyxml2;

Scene::Scene()
: m_loaded(false)
{ }

Scene* Scene::New(string filename)
{
    if (!GameProject::Singleton().IsToolside())
    {
        printf("Scene error: can't create new scene in game mode!\n");
        return false;
    }

    Scene* scene = new Scene();

    if (scene != NULL)
    {
        scene->m_filename = filename;
        scene->m_guid = MakeGuid(filename);
        unsigned int guid = MakeGuid("ROOT");
        scene->m_rootObject = new ToolsideGameObject(guid, "ROOT");
        scene->m_loaded = true;

        // TODO more default scene setup
    }

    return scene;
}

Scene* Scene::Load(string filename)
{
    printf("LOADING SCENE: %s\n", filename);

    // Load the file into the deserializer
    HierarchicalDeserializer deserializer;
    bool success = deserializer.Load(filename);
    if (!success)
    {
        return NULL;
    }

    Scene* scene = new Scene();
    scene->m_filename = filename;

    success = deserializer.PushScope("Scene");
    if (!success)
    {
        delete scene;
        return NULL;
    }

    // Read the guid
    success = deserializer.GetAttribute<unsigned int>("guid", scene->m_guid);
    //if (scene->m_guid == XML_NO_ATTRIBUTE || scene->m_guid == 0)
    if (!success || scene->m_guid == 0)
    {
        scene->m_guid = MakeGuid(scene->m_filename);
    }

    // Load the required resources
    printf("Loading scene resources...\n");
    success = deserializer.PushScope("Resources");
    if (!success)
    {
        printf("Error parsing scene file. Could not find resource list.\n");
        delete scene;
        return NULL;
    }
    //ResourceManager::Singleton().LoadSceneResources(&deserializer);
    deserializer.PopScope();

    // Apply global settings (camera, light, etc.)
    scene->LoadGlobalSettings(&deserializer);

    // Build the game object hierarchy
    scene->LoadHierarchy(&deserializer);

    printf("DONE LOADING SCENE!\n");

    scene->m_loaded = true;

    return scene;
}

bool Scene::Save(string filename)
{
    if (!m_loaded)
    {
        printf("Scene error: can't save because no scene is loaded.\n");
        return false;
    }

    if (!GameProject::Singleton().IsToolside())
    {
        printf("Scene error: can't save scene in game mode!\n");
        return false;
    }

    // If a non-empty filename is provided, use it (for "Save As")
    if (filename.compare("") != 0)
    {
        m_filename = filename;
    }

    // Root setup
    HierarchicalSerializer serializer;
    serializer.PushScope("Scene");
    serializer.SetAttribute("guid", m_guid);

    // Track which resources are needed by the scene
    unordered_set<unsigned int> resourceGuids;

    // Serialize body
    SaveGlobalSettings(&serializer);
    SaveHierarchy(&serializer, (ToolsideGameObject*)m_rootObject, resourceGuids);
    SaveResourceList(&serializer, resourceGuids);

    // Save it!
    serializer.Save(m_filename);

    return true;
}

void Scene::Unload(Scene* scene)
{
    if (scene == NULL)
        return;

    if (!scene->m_loaded)
    {
        printf("Scene error: can't unload because scene is not loaded.\n");
    }
    else
    {
        // Unload resources
        ResourceManager::Singleton().UnloadSceneResources();

        // Tear down the hierarchy
        // TODO implement me
    }

    delete scene;
}

GameObjectBase* Scene::GetRootObject()
{
    return m_rootObject;
}

GameObject* Scene::GetRuntimeRootObject()
{
    return (GameObject*)m_rootObject;
}

ToolsideGameObject* Scene::GetToolsideRootObject()
{
    return (ToolsideGameObject*)m_rootObject;
}

bool Scene::IsLoaded()
{
    return m_loaded;
}


void Scene::SaveGlobalSettings(HierarchicalSerializer* serializer)
{
    // Camera
    serializer->PushScope("Camera");
    serializer->InsertLeafVector3("Position",   m_mainCamera.position);
    serializer->InsertLeafVector3("Direction",  m_mainCamera.direction);
    serializer->InsertLeafVector3("Up",         m_mainCamera.up);
    serializer->PopScope();

    // Light
    serializer->PushScope("Light");
    serializer->InsertLeafVector3("Position",   m_light.position);
    serializer->InsertLeafColorRGB("Color",     m_light.color);
    serializer->InsertLeaf("Power", "value",    m_light.power);
    serializer->PopScope();
}

void Scene::SaveHierarchy(HierarchicalSerializer* serializer, ToolsideGameObject* gameObject, unordered_set<unsigned int>& guids)
{
    if (gameObject == NULL)
        return;

    // Create node
    serializer->PushScope("GameObject");
    serializer->SetAttribute("guid", gameObject->GetID());
    serializer->SetAttribute("name", gameObject->GetName().c_str());

    // Serialize components
    SaveTransform(serializer, gameObject);
    SaveMesh(serializer, gameObject, guids);
    SaveColliders(serializer, gameObject);
    SaveComponents(serializer, gameObject, guids);

    // Serialize children
    std::vector<GameObjectBase*>::iterator childIter;
    for (childIter = gameObject->GetChildren().begin(); childIter != gameObject->GetChildren().end(); childIter++)
    {
        ToolsideGameObject* child = (ToolsideGameObject*)*childIter;
        SaveHierarchy(serializer, child, guids);
    }

    serializer->PopScope();
}
void Scene::SaveTransform(HierarchicalSerializer* serializer, ToolsideGameObject* gameObject)
{
    if (gameObject == NULL)
        return;

    serializer->PushScope("Transform");
    serializer->InsertLeafVector3("Position",   gameObject->GetTransform().GetLocalPosition());
    serializer->InsertLeafVector3("Rotation",   gameObject->GetTransform().GetLocalRotation());
    serializer->InsertLeafVector3("Scale",      gameObject->GetTransform().GetLocalScale());
    serializer->PopScope();
}

void Scene::SaveMesh(HierarchicalSerializer* serializer, ToolsideGameObject* gameObject, unordered_set<unsigned int>& guids)
{
    if (gameObject == NULL || gameObject->GetMesh() == NULL)
        return;

    serializer->PushScope("Mesh");

    unsigned int guid = gameObject->GetMesh()->GetMesh()->GetResourceInfo()->guid;
    serializer->SetAttribute("guid", guid);
    guids.insert(guid);

    SaveMaterial(serializer, gameObject, guids);

    serializer->PopScope();
}

void Scene::SaveMaterial(HierarchicalSerializer* serializer, ToolsideGameObject* gameObject, unordered_set<unsigned int>& guids)
{
    if (gameObject == NULL || gameObject->GetMesh() == NULL || gameObject->GetMesh()->GetMaterial() == NULL)
        return;

    Material* material = gameObject->GetMesh()->GetMaterial();

    serializer->PushScope("Material");

    // Serialize shader info
    if (material->GetShader() != NULL)
    {
        serializer->PushScope("Shader");
        unsigned int guid = material->GetShader()->GetResourceInfo()->guid;
        serializer->SetAttribute("guid", guid);
        guids.insert(guid);
        serializer->PopScope();
    }

    // Serialize Color info
    SaveMaterialColors(serializer, material);

    // Serialize texture info
    SaveMaterialTextures(serializer, material, guids);

    serializer->PopScope();
}

void Scene::SaveMaterialColors(HierarchicalSerializer* serializer, Material* material)
{
    unordered_map<GLint, ColorRGB> colors = material->GetColorList();
    unordered_map<GLint, ColorRGB>::iterator iter = colors.begin();

    ShaderProgram* shader = material->GetShader();

    for (; iter != colors.end(); iter++)
    {
        serializer->PushScope("Color");

        string paramName = shader->GetUniformName(iter->first);
        serializer->SetAttribute("name", paramName.c_str());
        serializer->SetAttributeColorRGB(iter->second);

        serializer->PopScope();
    }
}

void Scene::SaveMaterialTextures(HierarchicalSerializer* serializer, Material* material, unordered_set<unsigned int>& guids)
{
    unordered_map<GLint, Texture*> textures = material->GetTextureList();
    unordered_map<GLint, Texture*>::iterator iter = textures.begin();

    ShaderProgram* shader = material->GetShader();

    for (; iter != textures.end(); iter++)
    {
        serializer->PushScope("Texture");

        unsigned int guid = 0;
        if (iter->second != NULL && iter->second != Texture::DefaultTexture())
        {
            guid = iter->second->GetResourceInfo()->guid;
        }

        serializer->SetAttribute("guid", guid);
        guids.insert(guid);

        string paramName = shader->GetUniformName(iter->first);
        serializer->SetAttribute("name", paramName.c_str());

        serializer->PopScope();
    }
}

void Scene::SaveColliders(HierarchicalSerializer* serializer, ToolsideGameObject* gameObject)
{
    if (gameObject == NULL)
        return;

    serializer->PushScope("Colliders");

    std::vector<Collider*> colliderList = gameObject->GetColliders();
    std::vector<Collider*>::iterator iter;
    for (iter = colliderList.begin(); iter != colliderList.end(); iter++)
    {
        Collider* collider = *iter;
        collider->Serialize(serializer);
    }

    serializer->PopScope();
}

void Scene::SaveComponents(HierarchicalSerializer* serializer, ToolsideGameObject* gameObject, unordered_set<unsigned int>& guids)
{
    if (gameObject == NULL)
        return;

    serializer->PushScope("Components");

    std::vector<ToolsideGameComponent*> compList = gameObject->GetComponentList();
    std::vector<ToolsideGameComponent*>::iterator compIter;
    for (compIter = compList.begin(); compIter != compList.end(); compIter++)
    {
        ToolsideGameComponent* comp = *compIter;
        comp->Serialize(serializer, guids);
    }

    serializer->PopScope();
}

void Scene::SaveResourceList(HierarchicalSerializer* serializer, unordered_set<unsigned int>& guids)
{
    serializer->PushScope("Resources");

    unordered_set<unsigned int>::iterator iter;
    for (iter = guids.begin(); iter != guids.end(); iter++)
    {
        serializer->InsertLeaf("Resource", "guid", *iter);
    }

    serializer->PopScope();
}

void Scene::LoadGlobalSettings(HierarchicalDeserializer* deserializer)
{
    printf("Doing camera & light setup...\n");

    if (deserializer->PushScope("Camera"))
    {
        deserializer->ReadLeafVector3("Position",   m_mainCamera.position);
        deserializer->ReadLeafVector3("Direction",  m_mainCamera.direction);
        deserializer->ReadLeafVector3("Up",         m_mainCamera.up);

        RenderManager::Singleton().SetCamera(m_mainCamera);
        deserializer->PopScope();
    }
    else
    {
        printf("Warning: no camera specified in scene file.\n");
    }

    if (deserializer->PushScope("Light"))
    {
        deserializer->ReadLeafVector3("Position",   m_light.position);
        deserializer->ReadLeafColorRGB("Color",     m_light.color);

        //TODO FIX ME
        //deserializer->ReadLeaf("Power", "value",    m_light.power);
        m_light.power = 5000;

        RenderManager::Singleton().SetLight(m_light);
        deserializer->PopScope();
    }
    else
    {
        printf("Warning: no light specified in scene file.\n");
    }
}

void Scene::LoadHierarchy(HierarchicalDeserializer* deserializer)
{
    printf("Building game object hierarchy...\n");

    if (deserializer->PushScope("GameObject"))
    {
        // Process the tree of game objects (recursively, depth first)
        m_rootObject = LoadHierarchySubtree(deserializer);
        deserializer->PopScope();
    }
    else
    {
        printf("Warning: no gameobject hierarchy found in scene file!\n");
    }
}

GameObjectBase* Scene::LoadHierarchySubtree(HierarchicalDeserializer* deserializer)
{
    // Build & add components on this node
    string name;
    deserializer->GetAttribute("name", name);

    unsigned int guid;
    deserializer->GetAttribute("guid", guid);
    if (guid == 0)
    {
        guid = MakeGuid(name);
    }

    GameObjectBase* go = NULL;
    if (GameProject::Singleton().IsToolside())
    {
        go = new ToolsideGameObject(guid, name);
    }
    else
    {
        go = new GameObject(guid, name);
    }

    LoadTransform(deserializer, go);
    LoadMesh(deserializer, go);
    LoadColliders(deserializer, go);
    LoadGameComponents(deserializer, go);

    // Recursively process child game objects
    bool childrenToProcess = deserializer->PushScope("GameObject");
    while (childrenToProcess)
    {
        GameObjectBase* childgo = LoadHierarchySubtree(deserializer);
        childgo->SetParent(go);
        childrenToProcess = deserializer->NextSiblingScope("GameObject");
    }

    return go;
}

void Scene::LoadTransform(HierarchicalDeserializer* deserializer, GameObjectBase* go)
{
    if (deserializer->PushScope("Transform"))
    {
        Vector3 position;
        deserializer->ReadLeafVector3("Position", position);

        Vector3 rotation;
        deserializer->ReadLeafVector3("Rotation", rotation);

        Vector3 scale;
        deserializer->ReadLeafVector3("Scale", scale);

        // TODO cleanup with transform functions
        Matrix4x4 matrix = Translation(position);
        matrix = matrix*RotationEulerAngles(rotation);
        matrix = matrix*Scaling(scale);

        go->GetTransform().SetLocalMatrix(matrix);

        deserializer->PopScope();
    }
}

void Scene::LoadMesh(HierarchicalDeserializer* deserializer, GameObjectBase* go)
{
    if (deserializer->PushScope("Mesh"))
    {
        // Find the mesh resource by guid
        unsigned int guid;
        deserializer->GetAttribute("guid", guid);

        Mesh* mesh = ResourceManager::Singleton().GetMesh(guid);
        if (mesh != NULL)
        {
            // Attach the mesh instance component & do material setup
            MeshInstance* meshInstance = new MeshInstance();
            meshInstance->SetMesh(mesh);
            LoadMaterial(deserializer, meshInstance);
            go->SetMesh(meshInstance);
        }
        else
        {
            printf("Warning: mesh referenced by game object is not loaded\n");
        }

        deserializer->PopScope();
    }
}

void Scene::LoadMaterial(HierarchicalDeserializer* deserializer, MeshInstance* meshInstance)
{
    if (meshInstance == NULL)
        return;

    if (deserializer->PushScope("Material"))
    {
        // Get material component
        Material* material = meshInstance->GetMaterial();

        // Attach shader
        if (deserializer->PushScope("Shader"))
        {
            unsigned int guid;
            deserializer->GetAttribute("guid", guid);
            ShaderProgram* shader = ResourceManager::Singleton().GetShader(guid);
            if (shader == NULL)
            {
                printf("Warning: shader referenced by game object is not loaded\n");
            }
            material->SetShader(shader);

            deserializer->PopScope();
        }

        // Add colors & textures
        LoadMaterialColors(deserializer, material);
        LoadMaterialTextures(deserializer, material);

        deserializer->PopScope();
    }
}

void Scene::LoadMaterialColors(HierarchicalDeserializer* deserializer, Material* material)
{
    bool colorsToProcess = deserializer->PushScope("Color");
    while (colorsToProcess)
    {
        string name;
        deserializer->GetAttribute("name", name);

        ColorRGB color;
        deserializer->GetAttributeColorRGB(color);

        material->SetColor(name, color);

        colorsToProcess = deserializer->NextSiblingScope("Color");
    }
}

void Scene::LoadMaterialTextures(HierarchicalDeserializer* deserializer, Material* material)
{
    bool texturesToProcess = deserializer->PushScope("Texture");
    while (texturesToProcess)
    {
        unsigned int guid;
        deserializer->GetAttribute("guid", guid);

        string name;
        deserializer->GetAttribute("name", name);

        Texture* texture = ResourceManager::Singleton().GetTexture(guid);
        if (texture == NULL)
        {
            printf("Warning: texture referenced by game object is not loaded\n");
        }

        material->SetTexture(name, texture);

        texturesToProcess = deserializer->NextSiblingScope("Texture");
    }
}

void Scene::LoadColliders(HierarchicalDeserializer* deserializer, GameObjectBase* go)
{
    if (deserializer->PushScope("Colliders"))
    {
        bool collidersToProcess = deserializer->PushScope("Collider");
        while (collidersToProcess)
        {
            // TODO FIX ME
            //Collider* collider = Collider::LoadFromXML(go, colliderXML);
            //go->AddCollider(collider);
            //
            //if (!GameProject::Singleton().IsToolside())
            //{
            //    CollisionEngine::Singleton().RegisterCollider(collider);
            //}

            collidersToProcess = deserializer->NextSiblingScope("Collider");
        }

        deserializer->PopScope();
    }
}

void Scene::LoadGameComponents(HierarchicalDeserializer* deserializer,GameObjectBase* go)
{
    if (deserializer->PushScope("Components"))
    {
        bool componentsToProcess = deserializer->PushScope("Component");
        while (componentsToProcess)
        {
            // TODO FIX ME
            //if (GameProject::Singleton().IsToolside())
            //{
            //    ToolsideGameComponent* component = new ToolsideGameComponent();
            //    component->Load(componentXML);
            //    ((ToolsideGameObject*)go)->AddComponent(component);
            //}
            //else
            //{
            //    unsigned int guid = componentXML->UnsignedAttribute("guid");
            //    bool isEngineComponent = componentXML->BoolAttribute("engine");
            //    GameComponentFactory* factory = GameProject::Singleton().GetRuntimeComponentFactory(isEngineComponent);
            //    GameComponent* component = factory->CreateComponent(guid);
            //    RuntimeParamList params = ComponentValue::ParseRuntimeParams(componentXML);
            //    factory->SetParams(guid, component, &params);
            //    ((GameObject*)go)->AddComponent(component);
            //}

            componentsToProcess = deserializer->NextSiblingScope("Component");
        }

        deserializer->PopScope();
    }
}