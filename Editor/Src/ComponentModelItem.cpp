#include "ComponentModelItem.h"

#include "AssetMimeData.h"
#include "ComponentModel.h"
#include "DebugLogger.h"
#include "EditorUtil.h"
#include "GameObjectMimeData.h"
#include "GameObjectReference.h"
#include "ToolsideGameObject.h"
#include "Rendering/Material.h"
#include "Rendering/Mesh.h"
#include "Rendering/MeshInstance.h"
#include "Rendering/Texture.h"
#include "Scene/ResourceManager.h"
#include "Util.h"

#include <QBrush>
#include <QColor>
#include <QColorDialog>

ComponentModelItem::ComponentModelItem(string name)
    : m_name(name), m_isHeader(true), m_parent(NULL)
{}

ComponentModelItem::~ComponentModelItem()
{
    Clear();
}

void ComponentModelItem::AddChild(ComponentModelItem* child)
{
    if (child == NULL)
        return;

    m_children.push_back(child);
    child->m_parent = this;
}

ComponentModelItem* ComponentModelItem::GetChild(int index)
{
    if (index < 0 || index >= (int)m_children.size())
        return NULL;

    return m_children[index];
}

ComponentModelItem* ComponentModelItem::GetParent()
{
    return m_parent;
}

int ComponentModelItem::GetChildCount()
{
    return m_children.size();
}

int ComponentModelItem::GetIndexInParent()
{
    // determine the index of this game object in the parent's list of children
    if (m_parent)
    {
        vector<ComponentModelItem*>::iterator iter = std::find(m_parent->m_children.begin(), m_parent->m_children.end(), const_cast<ComponentModelItem*>(this));
        return std::distance(m_parent->m_children.begin(), iter);
    }

    return 0;
}

void ComponentModelItem::Refresh()
{
    vector<ComponentModelItem*>::iterator iter = m_children.begin();
    for (; iter != m_children.end(); iter++)
    {
        (*iter)->Refresh();
    }
}

QVariant ComponentModelItem::GetData(ColumnType columnType, int role)
{
    if (role == Qt::BackgroundRole)
    {
        return GetBackgroundData(columnType);
    }
    else if (role == Qt::TextAlignmentRole)
    {
        if (m_isHeader && columnType == VALUE_COLUMN)
            return Qt::AlignRight;
    }
    else
    {
        switch (columnType)
        {
        case NAME_COLUMN:
            return QVariant(m_name.c_str());
        case VALUE_COLUMN:
            return GetValueData();
        }
    }
    return QVariant();
}

void ComponentModelItem::Clear()
{
    while (m_children.size() > 0)
    {
        ComponentModelItem* child = m_children.back();
        m_children.pop_back();
        delete child;
    }
    m_children.clear();
}

QVariant ComponentModelItem::GetValueData()
{
    if (m_isHeader)
    {
        return QVariant("...");
    }
    return QVariant("");
}

QVariant ComponentModelItem::GetBackgroundData(ColumnType /*columnType*/)
{
    if (m_isHeader)
    {
        QColor color(230, 230, 230);
        return QBrush(color);
    }
    return QVariant();
}

bool ComponentModelItem::SetData(QVariant value)
{
    return false;
}

bool ComponentModelItem::IsEditable()
{
    return !m_isHeader;
}

bool ComponentModelItem::DropData(const QMimeData* /*data*/)
{
    return false;
}

void ComponentModelItem::OnDoubleClick(ColumnType /*columnType*/)
{}

MenuOptions ComponentModelItem::GetMenuOptions()
{
    return CONTEXTMENU_NONE;
}

bool ComponentModelItem::HandleMenuSelection(ContextMenuOption selection)
{
    return false;
}

//--------------------------------------------------------------------------------

ComponentModelTransformItem::ComponentModelTransformItem(ToolsideGameObject* gameObject)
    : ComponentModelItem("Transform"), m_gameObject(gameObject)
{
    m_isHeader = true;

    // Add position, rotation, and scale items
    ComponentModelTransformItem* positionItem = new ComponentModelTransformItem("Position", m_gameObject, eVector_Position);
    AddChild(positionItem);
    ComponentModelTransformItem* rotationItem = new ComponentModelTransformItem("Rotation", m_gameObject, eVector_Rotation);
    AddChild(rotationItem);
    ComponentModelTransformItem* scaleItem = new ComponentModelTransformItem("Scale", m_gameObject, eVector_Scale);
    AddChild(scaleItem);
}

ComponentModelTransformItem::ComponentModelTransformItem(string name, ToolsideGameObject* gameObject, TransformVectorType type)
    : ComponentModelItem(name), m_gameObject(gameObject), m_vectorType(type)
{
    m_isHeader = false;
    Refresh();
}

void ComponentModelTransformItem::Refresh()
{
    ComponentModelItem::Refresh();

    if (m_isHeader)
        return;

    m_vector = m_gameObject->GetLocalTransform().GetVector(m_vectorType);
}

QVariant ComponentModelTransformItem::GetValueData()
{
    if (m_isHeader)
        return ComponentModelItem::GetValueData();

    string str = WriteVector3ToString(m_vector);
    return QVariant(str.c_str());
}

bool ComponentModelTransformItem::SetData(QVariant value)
{
    if (m_isHeader)
        return ComponentModelItem::SetData(value);

    m_vector = ReadVector3FromString(value.toString().toStdString());

    EditorCommands::ModifyTransformCommand* command = new EditorCommands::ModifyTransformCommand(m_gameObject, m_vector, m_vectorType);
    CommandManager::Singleton().ExecuteCommand(command);

    return true;
}

MenuOptions ComponentModelTransformItem::GetMenuOptions()
{
    if (!m_isHeader)
        return CONTEXTMENU_NONE;

    return CONTEXTMENU_COPY;
}

bool ComponentModelTransformItem::HandleMenuSelection(ContextMenuOption selection)
{
    switch (selection)
    {
    case CONTEXTMENU_COPY:
        // TODO implement me
        DebugLogger::Singleton().Log("copy transform -- not implemented");
        break;
    }

    return false;
}

//--------------------------------------------------------------------------------

ComponentModelMeshItem::ComponentModelMeshItem(MeshInstance* mesh, bool header)
    : ComponentModelItem("Mesh"), m_mesh(mesh)
{
    m_isHeader = header;

    if (m_isHeader)
    {
        ComponentModelMeshItem* meshObjItem = new ComponentModelMeshItem(m_mesh, false);
        AddChild(meshObjItem);
    }
    else
    {
        ComponentModelShaderItem* shaderItem = new ComponentModelShaderItem(m_mesh->GetMaterial());
        AddChild(shaderItem);
    }
}

QVariant ComponentModelMeshItem::GetValueData()
{
    if (m_isHeader)
        return ComponentModelItem::GetValueData();

    string str;
    if (m_mesh == NULL)
    {
        str = "<MISSING REF>";
    }
    else
    {
        ResourceInfo* info = m_mesh->GetMesh()->GetResourceInfo();
        str = GetFriendlyAssetNameFromPath(info->path) + " (" + std::to_string(info->guid) + ")";
    }
    return QVariant(str.c_str());
}

bool ComponentModelMeshItem::IsEditable()
{
    return false;
}

bool ComponentModelMeshItem::DropData(const QMimeData* data)
{
    if (m_isHeader)
        return false;

    AssetMimeData* mimeData = (AssetMimeData*)data;
    if (mimeData != NULL)
    {
        ResourceInfo* info = mimeData->GetResourceInfo();
        if (strcmp(info->TypeName().c_str(), "Mesh") == 0)       // TODO use type enum to avoid strcmp
        {
            Mesh* mesh = ResourceManager::Singleton().GetMesh(info->guid);
            m_mesh->SetMesh(mesh);
            return true;
        }
    }
    return false;
}

MenuOptions ComponentModelMeshItem::GetMenuOptions()
{
    if (!m_isHeader)
        return CONTEXTMENU_NONE;

    return CONTEXTMENU_COPY | CONTEXTMENU_DELETE;
}

bool ComponentModelMeshItem::HandleMenuSelection(ContextMenuOption selection)
{
    switch (selection)
    {
    case CONTEXTMENU_COPY:
        // TODO implement me
        DebugLogger::Singleton().Log("copy mesh component -- not implemented");
        break;
    case CONTEXTMENU_DELETE:
        DebugLogger::Singleton().Log("delete mesh component");
        GameObjectBase* go = m_mesh->GetGameObject();
        go->SetMesh(NULL);
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------

ComponentModelShaderItem::ComponentModelShaderItem(Material* material)
    : ComponentModelItem("Shader"), m_material(material)
{
    Refresh();
}

void ComponentModelShaderItem::Refresh()
{
    Clear();

    m_isHeader = false;

    // Colors
    unordered_map<GLint, ColourRGB> colorList = m_material->GetColorList();
    unordered_map<GLint, ColourRGB>::iterator colorIter = colorList.begin();
    for (; colorIter != colorList.end(); colorIter++)
    {
        int colorID = colorIter->first;
        string colorName = m_material->GetShader()->GetUniformName(colorID);
        ComponentModelColorItem* colorItem = new ComponentModelColorItem(colorName, m_material, colorID);
        AddChild(colorItem);
    }

    // Textures
    unordered_map<GLint, Texture*> textureList = m_material->GetTextureList();
    unordered_map<GLint, Texture*>::iterator texIter = textureList.begin();
    for (; texIter != textureList.end(); texIter++)
    {
        int textureID = texIter->first;
        string textureName = m_material->GetShader()->GetUniformName(textureID);
        ComponentModelTextureItem* textureItem = new ComponentModelTextureItem(textureName, m_material, textureID);
        AddChild(textureItem);
    }
}

QVariant ComponentModelShaderItem::GetValueData()
{
    string str;
    if (m_material == NULL)
    {
        str = "<MISSING REF>";
    }
    else
    {
        ResourceInfo* info = m_material->GetShader()->GetResourceInfo();
        str = GetFriendlyAssetNameFromPath(info->path) + " (" + std::to_string(info->guid) + ")";
    }
    return QVariant(str.c_str());
}

bool ComponentModelShaderItem::IsEditable()
{
    return false;
}

bool ComponentModelShaderItem::DropData(const QMimeData* data)
{
    AssetMimeData* mimeData = (AssetMimeData*)data;
    if (mimeData != NULL)
    {
        ResourceInfo* info = mimeData->GetResourceInfo();
        if (strcmp(info->TypeName().c_str(), "Shader") == 0)       // TODO use type enum to avoid strcmp
        {
            info->AddToGameObject((ToolsideGameObject*)(m_material->GetMesh()->GetGameObject()));
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------------

ComponentModelTextureItem::ComponentModelTextureItem(string name, Material* material, int paramID)
    : ComponentModelItem(name), m_material(material), m_paramID(paramID)
{
    m_isHeader = false;
}

QVariant ComponentModelTextureItem::GetValueData()
{
    string str;
    if (m_material == NULL)
    {
        str = "<MISSING REF>";
    }
    else
    {
        Texture* texture = m_material->GetTexture(m_paramID);
        if (texture != Texture::DefaultTexture())
        {
            ResourceInfo* info = m_material->GetTexture(m_paramID)->GetResourceInfo();
            str = GetFriendlyAssetNameFromPath(info->path) + " (" + std::to_string(info->guid) + ")";
        }
        else
        {
            str = "<None>";
        }
    }
    return QVariant(str.c_str());
}

bool ComponentModelTextureItem::IsEditable()
{
    return false;
}

bool ComponentModelTextureItem::DropData(const QMimeData* data)
{
    AssetMimeData* mimeData = (AssetMimeData*)data;
    if (mimeData != NULL)
    {
        ResourceInfo* info = mimeData->GetResourceInfo();
        if (strcmp(info->TypeName().c_str(), "Texture") == 0)       // TODO use type enum to avoid strcmp
        {
            Texture* texture = ResourceManager::Singleton().GetTexture(info->guid);
            m_material->SetTexture(m_paramID, texture);
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------------

ComponentModelColorItem::ComponentModelColorItem(string name, Material* material, int paramID)
    : ComponentModelItem(name), m_material(material), m_paramID(paramID)
{
    m_isHeader = false;
}

QVariant ComponentModelColorItem::GetValueData()
{
    return QVariant();
}

QVariant ComponentModelColorItem::GetBackgroundData(ColumnType columnType)
{
    if (m_isHeader)
    {
        return ComponentModelItem::GetBackgroundData(columnType);
    }

    switch (columnType)
    {
    case ComponentModelItem::NAME_COLUMN:
        return QVariant();
        break;
    case ComponentModelItem::VALUE_COLUMN:
        ColourRGB c = m_material->GetColor(m_paramID);
        QColor qcolor(c.r * 255, c.g * 255, c.b * 255);
        return QBrush(qcolor);
        break;
    }

    return ComponentModelItem::GetBackgroundData(columnType);
}

bool ComponentModelColorItem::IsEditable()
{
    return false;
}

bool ComponentModelColorItem::DropData(const QMimeData* /*data*/)
{
    return false;
}

void ComponentModelColorItem::OnDoubleClick(ColumnType /*columnType*/)
{
    // Open a ColorDialog to let the user pick a color
    ColourRGB oldColor = m_material->GetColor(m_paramID);
    QColor newColor = QColorDialog::getColor(ColourRGBToQColor(oldColor));

    if (newColor.isValid())                             // color will be invalid if user hits "cancel" on dialog
    {
        // TODO set data using Editor Commands
        m_material->SetColor(m_paramID, QColorToColourRGB(newColor));
    }
}

//--------------------------------------------------------------------------------

ComponentModelScriptItem::ComponentModelScriptItem(ToolsideGameComponent* component)
    : ComponentModelItem(component->GetDisplayName()), m_component(component)
{
    m_isHeader = true;

    // Add item for each parameter
    ParamList params = component->GetParameterList();
    for (int i = 0; i < (int)params.size(); i++)
    {
        ComponentModelScriptItem* paramItem = new ComponentModelScriptItem(component, i);
        AddChild(paramItem);
    }
}

ComponentModelScriptItem::ComponentModelScriptItem(ToolsideGameComponent* component, int paramIndex)
    : ComponentModelItem(""), m_component(component), m_paramIndex(paramIndex)
{
    m_isHeader = false;
    Refresh();
}

void ComponentModelScriptItem::Refresh()
{
    ComponentModelItem::Refresh();

    if (m_isHeader)
        return;

    ParamList params = m_component->GetParameterList();
    ParamPair& pair = params[m_paramIndex];

    m_name = pair.first.Name;
    m_valueType = pair.first.Type;
    m_value = pair.second;
}

QVariant ComponentModelScriptItem::GetValueData()
{
    if (m_isHeader)
        return ComponentModelItem::GetValueData();

    string str;

    switch(m_valueType)
    {
    case ComponentParameter::TYPE_GAMEOBJECT:
    {
        // For game object values, we show the object's name in addition to the guid
        ToolsideGameObject* go = GameObjectReference::GetToolsideGameObject(m_value.go);
        if (go == NULL)
            str = "<MISSING REF>";
        else
            str = go->GetName() + " (" + std::to_string(m_value.go) + ")";
        break;
    }
    case ComponentParameter::TYPE_MESH:
    case ComponentParameter::TYPE_SHADER:
    case ComponentParameter::TYPE_TEXTURE:
        str = GetValueDataForResourceParam(m_value.ref);
        break;
    case ComponentParameter::TYPE_COLOR:
        str = "";
        break;
    default:
        str = m_value.GetValueString(m_valueType);
        break;
    }

    return QVariant(str.c_str());
}

QVariant ComponentModelScriptItem::GetBackgroundData(ColumnType columnType)
{
    if (m_valueType == ComponentParameter::TYPE_COLOR)
    {
        if (m_isHeader)
        {
            return ComponentModelItem::GetBackgroundData(columnType);
        }

        switch (columnType)
        {
        case ComponentModelItem::NAME_COLUMN:
            return QVariant();
            break;
        case ComponentModelItem::VALUE_COLUMN:
            ColourRGB c = m_value.c;
            QColor qcolor(c.r*255, c.g*255, c.b*255);
            return QBrush(qcolor);
            break;
        }
    }

    return ComponentModelItem::GetBackgroundData(columnType);
}

bool ComponentModelScriptItem::SetData(QVariant value)
{
    if (m_isHeader)
        return ComponentModelItem::SetData(value);

    // TODO set data using Editor Commands
    m_value.SetValue(m_valueType, value.toString().toStdString());
    m_component->SetParameter(ComponentParameter(m_name, m_valueType), m_value);

    return false;
}

bool ComponentModelScriptItem::IsEditable()
{
    if (m_isHeader)
        return false;

    if (m_valueType == ComponentParameter::TYPE_COLOR       ||      // colors are edited using the color window
        IsDropEnabled())                                            // fields with drag/drop enabled can't be edited directly
        return false;

    return true;
}

bool ComponentModelScriptItem::DropData(const QMimeData* data)
{
    if (m_isHeader)
        return false;

    if (!IsDropEnabled())
        return false;

    if (m_valueType == ComponentParameter::TYPE_GAMEOBJECT)
    {
        GameObjectMimeData* goMimeData = (GameObjectMimeData*)(data);
        if (goMimeData != NULL)
        {
            // TODO set data using Editor Commands
            m_value.SetValue(m_valueType, std::to_string(goMimeData->getGameObject()->GetID()));
            m_component->SetParameter(ComponentParameter(m_name, m_valueType), m_value);
            return true;
        }
    }
    else
    {
        AssetMimeData* assetMimeData = (AssetMimeData*)data;
        if (assetMimeData != NULL)
        {
            // TODO set data using Editor Commands
            ResourceInfo* info = assetMimeData->GetResourceInfo();

            // TODO ugh, clean this up
            if ((m_valueType == ComponentParameter::TYPE_MESH && (strcmp(info->TypeName().c_str(), "Mesh") == 0)) ||
                (m_valueType == ComponentParameter::TYPE_SHADER && (strcmp(info->TypeName().c_str(), "Shader") == 0)) ||
                (m_valueType == ComponentParameter::TYPE_TEXTURE && (strcmp(info->TypeName().c_str(), "Texture") == 0)))
                {
                    m_value.SetValue(m_valueType, std::to_string(info->guid));
                    m_component->SetParameter(ComponentParameter(m_name, m_valueType), m_value);
                    return true;
                }
        }
    }
    return true;
}

void ComponentModelScriptItem::OnDoubleClick(ColumnType /*columnType*/)
{
    if (m_isHeader)
        return;

    switch (m_valueType)
    {
    case ComponentParameter::TYPE_COLOR:
    {
        // Open a ColorDialog to let the user pick a color
        QColor newColor = QColorDialog::getColor(ColourRGBToQColor(m_value.c));

        if (newColor.isValid())                             // color will be invalid if user hits "cancel" on dialog
        {
            // TODO set data using Editor Commands
            m_value.c = QColorToColourRGB(newColor);
            m_component->SetParameter(ComponentParameter(m_name, m_valueType), m_value);
        }

        break;
    }
    default:
        break;
    }
}

MenuOptions ComponentModelScriptItem::GetMenuOptions()
{
    if (!m_isHeader)
        return CONTEXTMENU_NONE;

    return CONTEXTMENU_COPY | CONTEXTMENU_DELETE;
}

bool ComponentModelScriptItem::HandleMenuSelection(ContextMenuOption selection)
{
    switch (selection)
    {
    case CONTEXTMENU_COPY:
        DebugLogger::Singleton().Log("copy script component -- not implemented");
        break;
    case CONTEXTMENU_DELETE:
        DebugLogger::Singleton().Log("delete script component");
        ToolsideGameObject* go = m_component->GetGameObject();
        go->RemoveComponent(m_component);
        return true;
        break;
    }
    return false;
}

bool ComponentModelScriptItem::IsDropEnabled()
{
    return (m_valueType == ComponentParameter::TYPE_GAMEOBJECT   ||
            m_valueType == ComponentParameter::TYPE_MESH         ||
            m_valueType == ComponentParameter::TYPE_SHADER       ||
            m_valueType == ComponentParameter::TYPE_TEXTURE);
}

string ComponentModelScriptItem::GetValueDataForResourceParam(unsigned int guid)
{
    string str;

    ResourceInfo* info = ResourceManager::Singleton().GetResourceInfo(guid);
    if (info == NULL)
    {
        str = "<MISSING REF>";
    }
    else
    {
        str = GetFriendlyAssetNameFromPath(info->path) + " (" + std::to_string(guid) + ")";
    }

    return str;
}