#pragma once
#include <cstdint>

namespace UsedAddressHashes
{
#pragma region Assert
constexpr auto Assert_RaiseAssert = 4285205681u;
#pragma endregion

#pragma region GameDefinition
constexpr auto GameDefinition_ToWorldID = 1002050525u;
#pragma endregion

#pragma region Ink
constexpr auto InkSystem_Instance = 2635337807u;
constexpr auto InkSystemRequestsHandler_StartSession = 1357913210u;
constexpr auto InkSystemRequestsHandler_ExitToMainMenu = 3105624475u;

constexpr auto InkLoadingScreenHandler_SetUnknownVar = 3771669141u;
constexpr auto InkLoadingLayer_SetUnknownVar = 1370036335u;
#pragma endregion

#pragma region PlayerSystem
constexpr auto PlayerSystem_GetPlayerControlledGameObject = 3420005096u;
#pragma endregion

#pragma region ScriptableSystem
constexpr auto ScriptableSystem_QueueRequest = 376512876u;
#pragma endregion

#pragma region ScriptableSystemsContainer
constexpr auto ScriptableSystemsContainer_GetSystemByName = 655630607u;
#pragma endregion

#pragma region Session
constexpr auto SessionData_dtor = 3460762581u;
constexpr auto SessionData_AddArgumentToList = 188748408u;
#pragma endregion

#pragma region Quest
constexpr auto Quest_SetSaveDataLoadingScreen_NodeType_ExecuteNode = 3489736669u;
constexpr auto Quest_FactsDBManagerNodeDefinition_ExecuteNode = 3679006322u;
#pragma endregion
} // namespace UsedAddressHashes
