#include "ToolsideGameComponent.h"
#include "Scene\ResourceManager.h"
#include "Util.h"
#include <sstream>

using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;
using tinyxml2::XMLError;

bool ComponentParameter::operator==(const ComponentParameter &other) const
{
    return ((Name.compare(other.Name) == 0) && Type == other.Type);
}

ComponentValue::ComponentValue()
{
    // set default values
    i = 0;
    f = 0.0f;
    b = false;
    str = "";
    v = Vector3::Zero;
    c = ColourRGB::Black;
    g = 0;
    mesh = 0;
    shdr = 0;
    tex = 0;
}

ComponentValue::ComponentValue(ComponentParameter::ParameterType type, tinyxml2::XMLElement* xml)
{
    SetValue(type, xml);
}

void ComponentValue::SetValue(ComponentParameter::ParameterType type, tinyxml2::XMLElement* xml)
{
    char* valueStr = "value";
    if (xml->Attribute(valueStr))
    {
        switch (type)
        {
            case ComponentParameter::TYPE_INT:
                i = xml->IntAttribute(valueStr);
                break;
            case ComponentParameter::TYPE_FLOAT:
                f = xml->FloatAttribute(valueStr);
                break;
            case ComponentParameter::TYPE_BOOL:
                b = xml->BoolAttribute(valueStr);
                break;
            case ComponentParameter::TYPE_STRING:
                str = xml->Attribute(valueStr);
                break;
            case ComponentParameter::TYPE_VECTOR3:
            {
                XMLElement* vectorXML = xml->FirstChildElement(valueStr);
                v = ReadVector3FromXML(vectorXML);
                break;
            }
            case ComponentParameter::TYPE_COLOR:
            {
                XMLElement* colorXML = xml->FirstChildElement(valueStr);
                c = ReadColourFromXML(colorXML);
                break;
            }
            case ComponentParameter::TYPE_GAMEOBJECT:
                g = xml->UnsignedAttribute(valueStr);
                break;
            case ComponentParameter::TYPE_MESH:
                mesh = xml->UnsignedAttribute(valueStr);
                break;
            case ComponentParameter::TYPE_SHADER:
                shdr = xml->UnsignedAttribute(valueStr);
                break;
            case ComponentParameter::TYPE_TEXTURE:
                tex = xml->UnsignedAttribute(valueStr);
                break;
        }
    }
}

void ComponentValue::SetValue(ComponentParameter::ParameterType type, string text)
{
    switch (type)
    {
        case ComponentParameter::TYPE_INT:
            i = std::stoi(text);
            break;
        case ComponentParameter::TYPE_FLOAT:
            f = std::stof(text);
            break;
        case ComponentParameter::TYPE_BOOL:
            b = (text.compare("true") == 0);    // TODO this field should be a checkbox in the editor      
            break;
        case ComponentParameter::TYPE_STRING:
            str = text;
            break;
        case ComponentParameter::TYPE_VECTOR3:
            v = ReadVector3FromString(text);
            break;
        case ComponentParameter::TYPE_COLOR:
            c = ReadVector3FromString(text);
            break;
        case ComponentParameter::TYPE_GAMEOBJECT:
            g = std::stoul(text);
            break;
        case ComponentParameter::TYPE_MESH:
            mesh = std::stoul(text);
            break;
        case ComponentParameter::TYPE_SHADER:
            shdr = std::stoul(text);
            break;
        case ComponentParameter::TYPE_TEXTURE:
            tex = std::stoul(text);
            break;
    }
}

void ComponentValue::SerializeValue(ComponentParameter::ParameterType type, tinyxml2::XMLElement* parentNode, tinyxml2::XMLDocument& rootDoc)
{
    switch (type)
    {
        case ComponentParameter::TYPE_INT:
            parentNode->SetAttribute("value", i);
            break;
        case ComponentParameter::TYPE_FLOAT:
            parentNode->SetAttribute("value", f);
            break;
        case ComponentParameter::TYPE_BOOL:
            parentNode->SetAttribute("value", b);
            break;
        case ComponentParameter::TYPE_STRING:
            parentNode->SetAttribute("value", str.c_str());
            break;
        case ComponentParameter::TYPE_VECTOR3:
            parentNode->SetAttribute("value", "");
            parentNode->InsertEndChild(WriteVector3ToXML(v, "value", rootDoc));
            break;
        case ComponentParameter::TYPE_COLOR:
            parentNode->SetAttribute("value", "");
            parentNode->InsertEndChild(WriteColourToXML(c, "value", rootDoc));
            break;
        case ComponentParameter::TYPE_GAMEOBJECT:
            parentNode->SetAttribute("value", g);
            break;
        case ComponentParameter::TYPE_MESH:
            parentNode->SetAttribute("value", mesh);
            break;
        case ComponentParameter::TYPE_SHADER:
            parentNode->SetAttribute("value", shdr);
            break;
        case ComponentParameter::TYPE_TEXTURE:
            parentNode->SetAttribute("value", tex);
            break;
    }
}

string ComponentValue::GetValueString(ComponentParameter::ParameterType type)
{
    switch (type)
    {
        case ComponentParameter::TYPE_INT:          return std::to_string(i);
        case ComponentParameter::TYPE_FLOAT:        return std::to_string(f);
        case ComponentParameter::TYPE_BOOL:         return b ? "true" : "false";
        case ComponentParameter::TYPE_STRING:       return str;
        case ComponentParameter::TYPE_VECTOR3:      return WriteVector3ToString(v);
        case ComponentParameter::TYPE_COLOR:        return WriteVector3ToString(c.ToVector());
        case ComponentParameter::TYPE_GAMEOBJECT:   return std::to_string(g);
        case ComponentParameter::TYPE_MESH:         return std::to_string(mesh);
        case ComponentParameter::TYPE_SHADER:       return std::to_string(shdr);
        case ComponentParameter::TYPE_TEXTURE:      return std::to_string(tex);
    }

    return "";
}

RuntimeParamList ComponentValue::ParseRuntimeParams(tinyxml2::XMLElement* xml)
{
    RuntimeParamList params;

    XMLElement* paramXML = xml->FirstChildElement("Param");
    while (paramXML)
    {
        ComponentParameter::ParameterType type = (ComponentParameter::ParameterType)(paramXML->IntAttribute("type"));
        ComponentValue value(type, paramXML);
        params.push_back(value);

        paramXML = paramXML->NextSiblingElement("Param");
    }

    return params;
}

void ToolsideGameComponent::Create(unsigned int guid)
{
    m_guid = guid;
    m_paramList = *(ResourceManager::Singleton().GetComponentParamList(m_guid));
    SetDisplayName();
}

void ToolsideGameComponent::Load(XMLElement* componentXML)
{
    m_guid = componentXML->UnsignedAttribute("guid");

    // Build parameter list
    XMLElement* paramXML = componentXML->FirstChildElement("Param");
    while (paramXML)
    {
        AddParameterToList(paramXML);
        paramXML = paramXML->NextSiblingElement("Param");
    }
    SetDisplayName();
    ValidateParameters();
}

void ToolsideGameComponent::Serialize(tinyxml2::XMLNode* parentNode, tinyxml2::XMLDocument& rootDoc)
{
    XMLElement* componentNode = rootDoc.NewElement("Component");
    parentNode->InsertEndChild(componentNode);
    componentNode->SetAttribute("guid", m_guid);

    ParamList::iterator iter = m_paramList.begin();
    for (; iter != m_paramList.end(); iter++)
    {
        ParamPair pair = *iter;

        XMLElement* paramNode = rootDoc.NewElement("Param");
        componentNode->InsertEndChild(paramNode);

        paramNode->SetAttribute("name", pair.first.Name.c_str());
        paramNode->SetAttribute("type", pair.first.Type);
        pair.second.SerializeValue(pair.first.Type, paramNode, rootDoc);
    }
}

GameObject* ToolsideGameComponent::GetGameObject()
{
    return m_gameObject;
}

void ToolsideGameComponent::SetGameObject(GameObject* go)
{
    m_gameObject = go;
}

unsigned int ToolsideGameComponent::GetGuid()
{
    return m_guid;
}

string ToolsideGameComponent::GetDisplayName()
{
    return m_displayName;
}

ParamList& ToolsideGameComponent::GetParameterList()
{
    return m_paramList;
}

void ToolsideGameComponent::SetParameter(ComponentParameter param, ComponentValue value)
{
    ParamList::iterator iter = m_paramList.begin();
    for (; iter != m_paramList.end(); iter++)
    {
        ComponentParameter existingParam = iter->first;
        if (existingParam == param)
        {
            iter->second = value;
            break;
        }
    }
}

void ToolsideGameComponent::ValidateParameters()
{
    // Validate parameter list by removing any parameters that no longer exist in the schema, and adding
    // any new values that did not exist before (while preserving non-default values for params that remain)

    ParamList currentParams = *(ResourceManager::Singleton().GetComponentParamList(m_guid));

    // This is pretty inefficient, but the list of parameters will typically be small
    ParamList::iterator oldIter = m_paramList.begin();
    for (; oldIter != m_paramList.end(); oldIter++)
    {
        // If this entry from the old list still exists in the new list, copy over its value
        ComponentParameter oldParam = oldIter->first;
        ParamList::iterator newIter = currentParams.begin();
        for (; newIter != currentParams.end(); newIter++)
        {
            ComponentParameter newParam = newIter->first;
            if (newParam == oldParam)
            {
                newIter->second = oldIter->second;
                continue;
            }
        }
    }
    m_paramList = currentParams;
}

void ToolsideGameComponent::AddParameterToList(XMLElement* paramXML)
{
    string name = paramXML->Attribute("name");

    ComponentParameter::ParameterType type = (ComponentParameter::ParameterType)(paramXML->IntAttribute("type"));
    ComponentParameter key(name, type);

    ComponentValue value;
    value.SetValue(type, paramXML);

    ParamPair pair(key, value);
    m_paramList.push_back(pair);
}

void ToolsideGameComponent::SetDisplayName()
{
    string path = ResourceManager::Singleton().GetResourceInfo(m_guid)->path;
    unsigned int startpos = path.find_last_of('/') + 1;
    unsigned int endpos = path.find_last_of('.') - 1;
    unsigned int length = endpos - startpos + 1;
    m_displayName = path.substr(startpos, length);
}

bool ToolsideComponentSchema::Load(string filename)
{
    Unload();

    // Open the schema doc
    XMLDocument schemaDoc;
    XMLError result = schemaDoc.LoadFile(filename.c_str());
    if (result != tinyxml2::XML_SUCCESS)
    {
        printf("Error reading component schema file %s.\nXMLError %d\n", filename.c_str(), result);
        return false;
    }

    // Process each script in the schema
    XMLElement* scriptsXML = schemaDoc.FirstChildElement("Scripts");
    XMLElement* scriptXML = scriptsXML->FirstChildElement("Script");
    while (scriptXML != NULL)
    {
        // Build the param map for this script
        ParamList* paramList = new ParamList();
        XMLElement* paramXML = scriptXML->FirstChildElement("Param");
        while (paramXML != NULL)
        {
            ComponentParameter param;
            param.Name = paramXML->Attribute("name");
            param.Type = (ComponentParameter::ParameterType)(paramXML->IntAttribute("type"));

            ComponentValue value;
            value.SetValue(param.Type, paramXML);

            ParamPair pair(param, value);
            paramList->push_back(pair);
            paramXML = paramXML->NextSiblingElement("Param");
        }

        // Add the map to the schema
        unsigned int guid = scriptXML->UnsignedAttribute("guid");
        m_schema[guid] = paramList;

        // Move to the next script
        scriptXML = scriptXML->NextSiblingElement("Script");
    }

    return true;
}

void ToolsideComponentSchema::Unload()
{
    unordered_map<unsigned int, ParamList*>::iterator iter;
    for (iter = m_schema.begin(); iter != m_schema.end(); iter++)
    {
        delete m_schema[iter->first];
    }
    m_schema.clear();
}

ParamList* ToolsideComponentSchema::GetDefaultParameterList(unsigned int guid)
{
    return m_schema[guid];
}