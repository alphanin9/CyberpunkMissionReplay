#include <Raw/Quest/NodeFuncs.hpp>

#include "InkSystem.hpp"

raw::Ink::InkSystem* raw::Ink::InkSystem::Get() noexcept
{
    return InkSystemInstance;
}

raw::Ink::InkLayerManager* raw::Ink::InkSystem::GetLayerManager() noexcept
{
    return m_layerManagers[0].instance;
}

Handle<ink::LoadingLayer> raw::Ink::InkLayerManager::GetLoadingLayer() noexcept
{
    for (auto& layer : layers)
    {
        if (auto& asLoadingLayer = Cast<ink::LoadingLayer>(layer))
        {
            return asLoadingLayer;
        }
    }
    
    return {};
}

void raw::Ink::InkSystem::SetInitialLoadingScreenTDBID(TweakDBID aId) noexcept
{
    auto loadingLayer = GetLayerManager()->GetLoadingLayer();

    if (!loadingLayer)
    {
        return;
    }
    
    auto nodeDefinition = MakeHandle<quest::SetSaveDataLoadingScreen_NodeType>();

    nodeDefinition->selectedLoading = aId;

    raw::Quest::SetNextLoadingScreen::Execute(nodeDefinition);
    raw::Ink::LoadingScreen::SetUnknownVarInInitialLoadingScreen(loadingLayer, 2u);
}