#include "gtest/gtest.h"
#include "Hooks.h"
#include "Logging.h"

#include <Glacier/ZScene.h>
#include <Glacier/ZActor.h>
#include <Glacier/ZSpatialEntity.h>
#include <Glacier/EntityFactory.h>
#include <Glacier/ZCollision.h>
#include <Glacier/ZCameraEntity.h>
#include <Glacier/ZGameLoopManager.h>
#include <Glacier/ZModule.h>
#include <Glacier/ZHitman5.h>
#include <Glacier/ZHttp.h>
#include <Glacier/ZPhysics.h>
#include <Glacier/ZSetpieceEntity.h>
#include <Glacier/ZContentKitManager.h>
#include <Glacier/ZAction.h>
#include <Glacier/ZItem.h>
#include <Glacier/ZInventory.h>
#include <Glacier/ZHM5CrippleBox.h>
#include <IO/ZBinaryReader.h>
#include <IO/ZBinaryDeserializer.h>
#include <Crypto.h>

#include <Functions.h>
#include <Globals.h>


class GameEnvironment {

public:
    GameEnvironment() {
    }

    void wait(int i) {

    }

    TPair<const ZRepositoryID, TEntityRef<ZGlobalOutfitKit>> *retrieveRandomOutfit() {
        auto it = s_ContentKitManager->m_repositoryGlobalOutfitKits.begin();
        while (true) {
            if (it == s_ContentKitManager->m_repositoryGlobalOutfitKits.end()) {
                return s_ContentKitManager->m_repositoryGlobalOutfitKits.begin().m_pCurrent;
            }
            if (rand() > 0.5) {
                return it.m_pCurrent;
            }
            ++it;
        }
    }


    auto spawnNPC(
            const char *p_NpcName,
            const ZRepositoryID &repositoryID,
            const TEntityRef<ZGlobalOutfitKit> *p_GlobalOutfitKit,
            const char *p_CurrentCharacterSetIndex,
            const char *p_CurrentOutfitVariationIndex
    ) -> ZActor * {
        const auto s_Scene = Globals::Hitman5Module->m_pEntitySceneContext->m_pScene;

        if (!s_Scene) {
            Logger::Debug("Scene not loaded.");
            return nullptr;
        }

        const auto s_RuntimeResourceId = ResId<"[assembly:/templates/gameplay/ai2/actors.template?/npcactor.entitytemplate].pc_entitytype">;

        TResourcePtr<ZTemplateEntityFactory> s_Resource;
        Globals::ResourceManager->GetResourcePtr(s_Resource, s_RuntimeResourceId, 0);

        if (!s_Resource) {
            Logger::Debug("Resource is not loaded.");
            return nullptr;
        }

        ZEntityRef s_NewEntity;
        Functions::ZEntityManager_NewEntity->Call(Globals::EntityManager, s_NewEntity, "", s_Resource, s_Scene.m_ref,
                                                  nullptr, -1);

        if (!s_NewEntity) {
            Logger::Debug("Could not spawn entity.");
            return nullptr;
        }

        TEntityRef<ZHitman5> s_LocalHitman;
        Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

        if (!s_LocalHitman) {
            Logger::Debug("No local hitman.");
            return nullptr;
        }

        ZActor *actor = s_NewEntity.QueryInterface<ZActor>();

        actor->m_sActorName = p_NpcName;
        actor->m_bStartEnabled = true;
        actor->m_nOutfitCharset = std::stoi(p_CurrentCharacterSetIndex);
        actor->m_nOutfitVariation = std::stoi(p_CurrentOutfitVariationIndex);
        actor->m_OutfitRepositoryID = repositoryID;
        actor->m_eRequiredVoiceVariation = EActorVoiceVariation::eAVV_Undefined;

        actor->Activate(0);

        ZSpatialEntity *s_ActorSpatialEntity = s_NewEntity.QueryInterface<ZSpatialEntity>();
        ZSpatialEntity *s_HitmanSpatialEntity = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();

        s_ActorSpatialEntity->SetWorldMatrix(s_HitmanSpatialEntity->GetWorldMatrix());

        if (p_GlobalOutfitKit) {
            equipOutfit(*p_GlobalOutfitKit, std::stoi(p_CurrentCharacterSetIndex), "Actor",
                        std::stoi(p_CurrentOutfitVariationIndex), actor);
        }
        return actor;
    }

    void equipOutfit(
            const TEntityRef<ZGlobalOutfitKit> &p_GlobalOutfitKit,
            unsigned int p_CurrentCharSetIndex,
            const char *p_CurrentCharSetCharacterType,
            unsigned int p_CurrentOutfitVariationIndex,
            ZActor *p_Actor
    ) {
        std::vector<ZRuntimeResourceID> s_ActorOutfitVariations;

        if (strcmp(p_CurrentCharSetCharacterType, "Actor") != 0) {
            const ZOutfitVariationCollection *s_OutfitVariationCollection = p_GlobalOutfitKit.m_pInterfaceRef->m_aCharSets[p_CurrentCharSetIndex].m_pInterfaceRef;

            const TEntityRef<ZCharsetCharacterType> *s_CharsetCharacterType2 = &s_OutfitVariationCollection->m_aCharacters[0];
            const TEntityRef<ZCharsetCharacterType> *s_CharsetCharacterType = nullptr;

            if (strcmp(p_CurrentCharSetCharacterType, "Nude") == 0) {
                s_CharsetCharacterType = &s_OutfitVariationCollection->m_aCharacters[1];
            } else if (strcmp(p_CurrentCharSetCharacterType, "HeroA") == 0) {
                s_CharsetCharacterType = &s_OutfitVariationCollection->m_aCharacters[2];
            }

            for (size_t i = 0; i < s_CharsetCharacterType2->m_pInterfaceRef->m_aVariations.size(); ++i) {
                s_ActorOutfitVariations.push_back(
                        s_CharsetCharacterType2->m_pInterfaceRef->m_aVariations[i].m_pInterfaceRef->m_Outfit);
            }

            if (s_CharsetCharacterType) {
                for (size_t i = 0; i < s_CharsetCharacterType2->m_pInterfaceRef->m_aVariations.size(); ++i) {
                    s_CharsetCharacterType2->m_pInterfaceRef->m_aVariations[i].m_pInterfaceRef->m_Outfit = s_CharsetCharacterType->m_pInterfaceRef->m_aVariations[i].m_pInterfaceRef->m_Outfit;
                }
            }
        }

        Functions::ZActor_SetOutfit->Call(p_Actor, p_GlobalOutfitKit, p_CurrentCharSetIndex,
                                          p_CurrentOutfitVariationIndex, false);

        if (strcmp(p_CurrentCharSetCharacterType, "Actor") != 0) {
            const ZOutfitVariationCollection *s_OutfitVariationCollection = p_GlobalOutfitKit.m_pInterfaceRef->m_aCharSets[p_CurrentCharSetIndex].m_pInterfaceRef;
            const TEntityRef<ZCharsetCharacterType> *s_CharsetCharacterType = &s_OutfitVariationCollection->m_aCharacters[0];

            for (size_t i = 0; i < s_ActorOutfitVariations.size(); ++i) {
                s_CharsetCharacterType->m_pInterfaceRef->m_aVariations[i].m_pInterfaceRef->m_Outfit = s_ActorOutfitVariations[i];
            }
        }
    }

private:
    ZContentKitManager *s_ContentKitManager = Globals::ContentKitManager;

};

class NPC {
public:
    NPC(GameEnvironment env, char *name) {
        _env = env;
        auto pair = _env.retrieveRandomOutfit();
        _repositoryID = pair->first;
        _globalOutfitKit = &pair->second;
        _actor = _env.spawnNPC(name, _repositoryID, _globalOutfitKit, _currentCharacterSetIndex,
                               _currentOutfitVariationIndex);
        if (_actor == nullptr) { throw std::runtime_error("Actor is null"); }
    }

    void setObservantForOutfits() {

    }

    bool isAlerted() {
        return false;
    }

private:
    GameEnvironment _env;
    ZActor *_actor;
    TEntityRef<ZGlobalOutfitKit> *_globalOutfitKit = nullptr;
    ZRepositoryID _repositoryID = ZRepositoryID("");
    char _currentCharacterSetIndex[3]{"0"};
    char _currentOutfitVariationIndex[3]{"0"};
};

class Hitman {
public:
    Hitman(GameEnvironment env) {
        _env = env;
    }

    void walkTowards(NPC npc) {

    }

private:
    GameEnvironment _env;

};

// Spawn Observant NPC in front, walk towards their line of sight, and alert them
TEST(EndToEndTest, AgentCanAlertObservantNPC) {
    GameEnvironment env;
    NPC npc(env, "Random NPC"); // Spawn NPC in front of Hitman
    npc.setObservantForOutfits({"Agent 47 Signature Suit"});  // How do I even get the outfit without know its id??
    Hitman agent47(env);

    agent47.walkIntoLineOfSight(npc);
    env.wait(1000);

    ASSERT_TRUE(npc.isAlerted());
}