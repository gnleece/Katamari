#include "Scene\ResourceManager.h"

#include "Util.h"
#include "Rendering\Material.h"
#include "Rendering\Mesh.h"
#include "Rendering\MeshInstance.h"
#include "Rendering\Texture.h"

#include <ctime>
#include "tinyxml2.h"

using namespace tinyxml2;

struct TextureResourceInfo : ResourceInfo
{ 
    virtual Resource* Load()
    {
        string absolutePath = ResourceManager::Singleton().GetResourceBasePath() + path;
        Texture* texture = new Texture(absolutePath, this);
        return texture;
    }

    virtual string TypeName()
    {
        return "Texture";
    }

    virtual void AddToGameObject(GameObject* gameObject)
    {
        // TODO implement me
    }
};

struct MeshResourceInfo : ResourceInfo
{
    virtual Resource* Load()
    {
        string absolutePath = ResourceManager::Singleton().GetResourceBasePath() + path;
        Mesh* mesh = new Mesh(absolutePath, this);
        return mesh;
    }

    virtual string TypeName()
    {
        return "Mesh";
    }

    virtual void AddToGameObject(GameObject* gameObject)
    {
        Mesh* mesh = ResourceManager::Singleton().GetMesh(guid);
        MeshInstance* meshInstance = gameObject->GetMesh();
        if (meshInstance == NULL)
        {
            meshInstance = new MeshInstance();
            gameObject->SetMesh(meshInstance);
        }
        meshInstance->SetMesh(mesh);
        Material* material = new Material();
        meshInstance->SetMaterial(material);
        ShaderProgram* shader = (ShaderProgram*)ResourceManager::Singleton().GetDefaultResource("shader_gouraud");
        material->SetShader(shader);
    }
};

struct ShaderResourceInfo : ResourceInfo
{
    string vertexpath;
    string fragmentpath;

    virtual Resource* Load()
    {
        string absolutePathVert = ResourceManager::Singleton().GetResourceBasePath() + vertexpath;
        string absolutePathFrag = ResourceManager::Singleton().GetResourceBasePath() + fragmentpath;
        ShaderProgram* shader = new ShaderProgram(absolutePathVert, absolutePathFrag, this);
        return shader;
    }

    virtual string TypeName()
    {
        return "Shader";
    }

    void AddToMap(XMLElement* element, unordered_map<unsigned int, ResourceInfo*> & map)
    {
        vertexpath = element->Attribute("vertex-path");
        fragmentpath = element->Attribute("fragment-path");
        guid = element->UnsignedAttribute("guid");
        map[guid] = this;
    }

    void Serialize(XMLDocument& rootDoc, XMLElement* parent)
    {
        XMLElement* xml = rootDoc.NewElement(TypeName().c_str());
        xml->SetAttribute("guid", guid);
        xml->SetAttribute("vertex-path", vertexpath.c_str());
        xml->SetAttribute("fragment-path", fragmentpath.c_str());
        parent->InsertEndChild(xml);
    }

    virtual void AddToGameObject(GameObject* gameObject)
    {
        // TODO implement me
    }
};

void ResourceInfo::AddToMap(XMLElement* element, unordered_map<unsigned int, ResourceInfo*> & map)
{
    path = element->Attribute("path");
    guid = element->UnsignedAttribute("guid");
    map[guid] = this;
}

void ResourceInfo::Serialize(XMLDocument& rootDoc, XMLElement* parent)
{
    XMLElement* xml = rootDoc.NewElement(TypeName().c_str());
    xml->SetAttribute("guid", guid);
    xml->SetAttribute("path", path.c_str());
    parent->InsertEndChild(xml);
}

void ResourceInfo::Unload()
{
    // TODO implement me
}

void ResourceManager::Startup()
{
    m_lookupTableLoaded = false;
}

void ResourceManager::Shutdown()
{
    // Unload currently loaded resources, if any
    UnloadSceneResources();

    // Unload resource map
    ClearResourceMap();
}

void ResourceManager::LoadResourceMap(XMLElement* resources)
{
    // Unload previous project
    ClearResourceMap();

    // Process each resource type
    AddResourcesToMap<TextureResourceInfo>(resources, "Textures");
    AddResourcesToMap<MeshResourceInfo>(resources, "Meshes");
    AddResourcesToMap<ShaderResourceInfo>(resources, "Shaders");

    // Load default resource map
    XMLElement* elements = resources->FirstChildElement("DefaultResources");
    if (elements)
    {
        XMLElement* element = elements->FirstChildElement();
        while (element)
        {
            string name = element->Attribute("name");
            unsigned int guid = element->UnsignedAttribute("guid");
            m_defaultResources[name] = guid;
            element = element->NextSiblingElement();
        }
    }

    m_lookupTableLoaded = true;
}

void ResourceManager::ClearResourceMap()
{
    UnloadSceneResources();

    unordered_map<unsigned int, ResourceInfo*>::iterator iter;
    for (iter = m_resourceMap.begin(); iter != m_resourceMap.end(); iter++)
    {
        ResourceInfo* info = iter->second;
        delete info;
    }
    m_resourceMap.clear();
    m_defaultResources.clear();

    m_lookupTableLoaded = false;
}

void ResourceManager::SerializeResourceMap(XMLDocument& rootDoc, XMLElement* parent)
{
    // Create parent node for each resource type
    XMLElement* textureXML = rootDoc.NewElement("Textures");
    XMLElement* meshXML = rootDoc.NewElement("Meshes");
    XMLElement* shaderXML = rootDoc.NewElement("Shaders");
    parent->InsertEndChild(textureXML);
    parent->InsertEndChild(meshXML);
    parent->InsertEndChild(shaderXML);

    // Serialize lookup table
    unordered_map<unsigned int, ResourceInfo*>::iterator iter;
    for (iter = m_resourceMap.begin(); iter != m_resourceMap.end(); iter++)
    {
        // TODO this is pretty ugly
        XMLElement* resourceParent = NULL;
        if (strcmp(iter->second->TypeName().c_str(), "Texture") == 0)
        {
            resourceParent = textureXML;
        }
        else if (strcmp(iter->second->TypeName().c_str(), "Mesh") == 0)
        {
            resourceParent = meshXML;
        }
        else if (strcmp(iter->second->TypeName().c_str(), "Shader") == 0)
        {
            resourceParent = shaderXML;
        }

        if (resourceParent != NULL)
        {
            iter->second->Serialize(rootDoc, resourceParent);
        }
    }

    // Serialize default resource lookup
    XMLElement* defaultXML = rootDoc.NewElement("DefaultResources");
    parent->InsertEndChild(defaultXML);
    unordered_map<string, unsigned int>::iterator defIter;
    for (defIter = m_defaultResources.begin(); defIter != m_defaultResources.end(); defIter++)
    {
        XMLElement* xml = rootDoc.NewElement("Asset");
        xml->SetAttribute("name", defIter->first.c_str());
        xml->SetAttribute("guid", defIter->second);
        defaultXML->InsertEndChild(xml);
    }
}

unsigned int ResourceManager::ImportResource(string& filepath, string type)
{
    ResourceInfo* resource = NULL;

    if (strcmp(type.c_str(), "obj") == 0)
    {
        // Mesh
        resource = new MeshResourceInfo();
    }
    else if (strcmp(type.c_str(), "bmp") == 0)
    {
        // Texture
        resource = new TextureResourceInfo();
    }

    if (resource == NULL)
    {
        return 0;
    }

    resource->path = filepath;
    resource->guid = MakeGuid(filepath);

    return Import(resource);
}

unsigned int ResourceManager::ImportShader(string vertpath, string fragpath)
{
    ShaderResourceInfo* resource = new ShaderResourceInfo();
    resource->vertexpath = vertpath;
    resource->fragmentpath = fragpath;
    resource->guid = MakeGuid(vertpath + fragpath);

    return Import(resource);
}

unsigned int ResourceManager::Import(ResourceInfo* resource)
{
    if (m_resourceMap.count(resource->guid) != 0)
    {
        // Because the guid is a hash of filepath + timestamp, this should basically never happen
        printf("ResourceManager error: trying to add a resource with an exisiting guid.\n");
        delete resource;
        return 0;
    }

    // TODO also check whether this filepath already exists in the table

    m_resourceMap[resource->guid] = resource;
    resource->Load();
    return resource->guid;
}

void ResourceManager::ImportDefaultResources()
{
    XMLDocument assetsDoc;
    XMLError result = assetsDoc.LoadFile("..\\Engine\\Assets\\DefaultAssets.xml");
    if (result != XML_SUCCESS)
    {
        printf("Error reading default assets file.\nXMLError %d\n", result);
    }
    XMLElement* assetsXML = assetsDoc.FirstChildElement("Dogwood-Default-Resources");

    // Import each asset, by type
    XMLElement* meshesXML = assetsXML->FirstChildElement("Meshes");
    ImportDefaultResourceType(meshesXML, "Meshes", "obj");
    XMLElement* texturesXML = assetsXML->FirstChildElement("Textures");
    ImportDefaultResourceType(texturesXML, "Textures", "bmp");
    XMLElement* shadersXML = assetsXML->FirstChildElement("Shaders");
    ImportDefaultShaders(shadersXML);
}

void ResourceManager::ImportDefaultResourceType(tinyxml2::XMLElement* subtree, string folderName, string extension)
{
    if (subtree != NULL)
    {
        XMLElement* resourceXML = subtree->FirstChildElement();
        while (resourceXML != NULL)
        {
            // Copy the asset to the project's asset folder
            string srcfile = resourceXML->Attribute("path");
            string name = resourceXML->Attribute("name");
            string destfile = m_resourceBasePath + folderName + "/" + name + "." + extension;
            FileCopy(srcfile, destfile);

            // Import the asset to the project
            string relativePath = AbsolutePathToProjectPath(destfile);
            unsigned int guid = ImportResource(relativePath, extension);
            m_defaultResources[name] = guid;

            resourceXML = resourceXML->NextSiblingElement();
        }
    }
}

void ResourceManager::ImportDefaultShaders(tinyxml2::XMLElement* subtree)
{
    if (subtree != NULL)
    {
        XMLElement* shaderXML = subtree->FirstChildElement();
        while (shaderXML != NULL)
        {
            // Copy the asset to the project's asset folder
            string vertSrcfile = shaderXML->Attribute("vertex-path");
            string fragSrcfile = shaderXML->Attribute("fragment-path");
            string name = shaderXML->Attribute("name");
            string vertDestfile = m_resourceBasePath + "Shaders/" + name + ".vert.glsl";
            FileCopy(vertSrcfile, vertDestfile);
            string fragDestFile = m_resourceBasePath + "Shaders/" + name + ".frag.glsl";
            FileCopy(fragSrcfile, fragDestFile);

            // Import the asset to the project
            string relativeVertPath = AbsolutePathToProjectPath(vertDestfile);
            string relativeFragPath = AbsolutePathToProjectPath(fragDestFile);
            unsigned int guid = ImportShader(relativeVertPath, relativeFragPath);
            m_defaultResources[name] = guid;

            shaderXML = shaderXML->NextSiblingElement();
        }
    }
}

void ResourceManager::LoadSceneResources(XMLElement* resources)
{
    XMLElement* element = resources->FirstChildElement();
    while (element)
    {
        unsigned int guid = element->UnsignedAttribute("guid");
        ResourceInfo* info = m_resourceMap[guid];
        if (info)
        {
            printf("Loading guid %d\n", guid);
            m_loadedResources[guid] = info->Load();
        }
        else
        {
            printf("Error! Could not find guid %d in database\n", guid);
        }
        element = element->NextSiblingElement();
    }
}

void ResourceManager::UnloadSceneResources()
{
    // TODO implement me
}

Resource* ResourceManager::GetResource(unsigned int guid, bool load)
{
    Resource* resource = m_loadedResources[guid];
    if (resource == NULL && load)
    {
        resource = m_resourceMap[guid]->Load();
        m_loadedResources[guid] = resource;
    }
    return resource;
}

Texture* ResourceManager::GetTexture(unsigned int guid, bool load)
{
    return (Texture*)GetResource(guid, load);
}

Mesh* ResourceManager::GetMesh(unsigned int guid, bool load)
{
    return (Mesh*)GetResource(guid, load);
}

ShaderProgram* ResourceManager::GetShader(unsigned int guid, bool load)
{
    return (ShaderProgram*)GetResource(guid, load);
}

Resource* ResourceManager::GetDefaultResource(string name, bool load)
{
    unsigned int guid = m_defaultResources[name];
    return GetResource(guid, load);
}

void ResourceManager::SetResourceBasePath(string& path)
{
    m_resourceBasePath = path;
}

string ResourceManager::GetResourceBasePath()
{
    return m_resourceBasePath;
}

string ResourceManager::AbsolutePathToProjectPath(string& absolutePath)
{
    int basePathLen = strlen(m_resourceBasePath.c_str());
    int absolutePathLen = strlen(absolutePath.c_str());

    // Check whether base project path is a prefix of the absolute path
    int result = strncmp(absolutePath.c_str(), m_resourceBasePath.c_str(), basePathLen);
    if (result != 0)
    {
        // Base path isn't a prefix, so we can't convert the absolute path
        return "";
    }

    // Return the suffix following the project base path
    string projectPath = absolutePath.substr(basePathLen, absolutePathLen - basePathLen);
    return projectPath;
}

ResourceMap& ResourceManager::GetResourceMap()
{
    return m_resourceMap;
}

unsigned int ResourceManager::MakeGuid(string str)
{
    // Get timestamp
    time_t timer;
    time(&timer);
    string timestamp = std::to_string(timer);

    // Use filepath + timestamp to create guid for this resource
    unsigned int guid = std::hash<string>()(str + timestamp);

    return guid;
}

template<typename T>
void ResourceManager::AddResourcesToMap(XMLElement* resources, string typeName)
{
    XMLElement* elements = resources->FirstChildElement(typeName.c_str());
    if (elements)
    {
        XMLElement* element = elements->FirstChildElement();
        while (element)
        {
            T* info = new T();
            info->AddToMap(element, m_resourceMap);
            element = element->NextSiblingElement();
        }
    }
}