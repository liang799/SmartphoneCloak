#include "gtest/gtest.h"

// Walk towards observant NPC, do nothing, and alert them
TEST(EndToEndTest, AgentCanAlertObservantNPC) {
    GameEnvironment env;
    NPC observantNPC(env);
    Hitman agent47(env);

    agent47.walkTowards(observantNPC);
    env.wait(1000);

    ASSERT_TRUE(observantNPC.isAlerted());
}