#include "Shared/ECS/ECS.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

namespace {

    struct SPosition {
        float x = 0.0F;
        float y = 0.0F;
    };

    struct SVelocity {
        float dx = 0.0F;
        float dy = 0.0F;
    };

    struct SHealth {
        int hp = 100;
    };

    struct SName {
        std::string value;
    };

} // namespace

// ── Entity lifecycle ─────────────────────────────────────────────────────────

TEST(EcsEntity, CreateIsAlive) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();

    EXPECT_TRUE(e.valid());
    EXPECT_TRUE(reg.alive(e));
    EXPECT_EQ(reg.size(), 1U);
}

TEST(EcsEntity, DestroyMakesHandleDead) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();

    reg.destroy(e);

    EXPECT_FALSE(reg.alive(e));
    EXPECT_EQ(reg.size(), 0U);
}

TEST(EcsEntity, DestroyIsIdempotent) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();

    reg.destroy(e);
    reg.destroy(e);

    EXPECT_EQ(reg.size(), 0U);
}

TEST(EcsEntity, SlotReuseBumpsGeneration) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity first = reg.create();
    reg.destroy(first);

    const Ecs::SEntity second = reg.create();

    EXPECT_EQ(second.index, first.index);
    EXPECT_NE(second.generation, first.generation);
    EXPECT_FALSE(reg.alive(first));
    EXPECT_TRUE(reg.alive(second));
}

TEST(EcsEntity, DefaultHandleIsInvalid) {
    const Ecs::SEntity e{};
    EXPECT_FALSE(e.valid());
}

TEST(EcsEntity, OutOfRangeAndStaleHandlesAreNotAlive) {
    Ecs::CRegistry reg;
    const auto      live = reg.create();

    EXPECT_FALSE(reg.alive(Ecs::SEntity{.index = 99, .generation = 0}));
    EXPECT_FALSE(reg.alive(Ecs::SEntity{.index = live.index, .generation = live.generation + 1}));
}

TEST(EcsEntity, HashDistinguishesGeneration) {
    const Ecs::SEntity            a{.index = 1, .generation = 0};
    const Ecs::SEntity            b{.index = 1, .generation = 1};
    const std::hash<Ecs::SEntity> hasher;

    EXPECT_NE(hasher(a), hasher(b));
}

// ── Component storage ────────────────────────────────────────────────────────

TEST(EcsComponent, EmplaceAndGet) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();

    auto&              pos = reg.emplace<SPosition>(e, SPosition{.x = 1.0F, .y = 2.0F});

    EXPECT_FLOAT_EQ(pos.x, 1.0F);
    EXPECT_TRUE(reg.has<SPosition>(e));
    EXPECT_FALSE(reg.has<SVelocity>(e));

    const auto* fetched = reg.get<SPosition>(e);
    ASSERT_NE(fetched, nullptr);
    EXPECT_FLOAT_EQ(fetched->y, 2.0F);
}

TEST(EcsComponent, EmplaceOverwritesExisting) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();

    reg.emplace<SPosition>(e, SPosition{.x = 1.0F, .y = 1.0F});
    reg.emplace<SPosition>(e, SPosition{.x = 5.0F, .y = 6.0F});

    EXPECT_EQ(reg.pool<SPosition>().size(), 1U);
    EXPECT_FLOAT_EQ(reg.get<SPosition>(e)->x, 5.0F);
}

TEST(EcsComponent, RemoveComponent) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();
    reg.emplace<SPosition>(e);

    reg.remove<SPosition>(e);

    EXPECT_FALSE(reg.has<SPosition>(e));
    EXPECT_EQ(reg.get<SPosition>(e), nullptr);
}

TEST(EcsComponent, RemovingMissingComponentIsSafe) {
    Ecs::CRegistry reg;
    const auto     e = reg.create();

    reg.remove<SPosition>(e);

    EXPECT_FALSE(reg.has<SPosition>(e));
}

TEST(EcsComponent, MissingPoolReturnsNullFromMutableAndConstGet) {
    Ecs::CRegistry reg;
    const auto      e = reg.create();

    EXPECT_EQ(reg.get<SName>(e), nullptr);
    const auto& constReg = reg;
    EXPECT_EQ(constReg.get<SName>(e), nullptr);
}

TEST(EcsComponent, DestroyRemovesAllComponents) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();
    reg.emplace<SPosition>(e);
    reg.emplace<SVelocity>(e);

    reg.destroy(e);

    EXPECT_EQ(reg.pool<SPosition>().size(), 0U);
    EXPECT_EQ(reg.pool<SVelocity>().size(), 0U);
}

TEST(EcsComponent, SwapRemoveKeepsPoolPacked) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity a = reg.create();
    const Ecs::SEntity b = reg.create();
    const Ecs::SEntity c = reg.create();

    reg.emplace<SHealth>(a, SHealth{10});
    reg.emplace<SHealth>(b, SHealth{20});
    reg.emplace<SHealth>(c, SHealth{30});

    reg.remove<SHealth>(a); // swap-removes c into a's slot

    EXPECT_EQ(reg.pool<SHealth>().size(), 2U);
    EXPECT_FALSE(reg.has<SHealth>(a));
    EXPECT_EQ(reg.get<SHealth>(b)->hp, 20);
    EXPECT_EQ(reg.get<SHealth>(c)->hp, 30);
}

TEST(EcsComponent, HandlesNonTrivialComponent) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();

    reg.emplace<SName>(e, SName{"player"});

    ASSERT_TRUE(reg.has<SName>(e));
    EXPECT_EQ(reg.get<SName>(e)->value, "player");
}

TEST(EcsComponent, StaleHandleDoesNotSeeRecycledComponent) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity first = reg.create();
    reg.emplace<SHealth>(first, SHealth{5});
    reg.destroy(first);

    const Ecs::SEntity second = reg.create();
    reg.emplace<SHealth>(second, SHealth{99});

    EXPECT_FALSE(reg.has<SHealth>(first));
    EXPECT_EQ(reg.get<SHealth>(first), nullptr);
    EXPECT_EQ(reg.get<SHealth>(second)->hp, 99);
}

// ── each() ───────────────────────────────────────────────────────────────────

TEST(EcsEach, VisitsOnlyMatchingEntities) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity a = reg.create();
    const Ecs::SEntity b = reg.create();
    const Ecs::SEntity c = reg.create();

    reg.emplace<SPosition>(a);
    reg.emplace<SPosition>(b);
    reg.emplace<SPosition>(c);
    reg.emplace<SVelocity>(a);
    reg.emplace<SVelocity>(b);
    // c has a position but no velocity

    std::vector<Ecs::SEntity> visited;
    reg.each<SPosition, SVelocity>(
        [&](Ecs::SEntity e, SPosition&, SVelocity&) { visited.push_back(e); });

    EXPECT_EQ(visited.size(), 2U);
}

TEST(EcsEach, MutatesComponents) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();
    reg.emplace<SPosition>(e, SPosition{.x = 0.0F, .y = 0.0F});
    reg.emplace<SVelocity>(e, SVelocity{.dx = 2.0F, .dy = 3.0F});

    reg.each<SPosition, SVelocity>([](Ecs::SEntity, SPosition& p, SVelocity& v) {
        p.x += v.dx;
        p.y += v.dy;
    });

    EXPECT_FLOAT_EQ(reg.get<SPosition>(e)->x, 2.0F);
    EXPECT_FLOAT_EQ(reg.get<SPosition>(e)->y, 3.0F);
}

TEST(EcsEach, SingleComponent) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();
    reg.emplace<SHealth>(e, SHealth{42});

    int seen = 0;
    reg.each<SHealth>([&](Ecs::SEntity, SHealth& h) {
        ++seen;
        EXPECT_EQ(h.hp, 42);
    });

    EXPECT_EQ(seen, 1);
}

TEST(EcsEach, EmptyRegistryDoesNothing) {
    Ecs::CRegistry reg;

    int            seen = 0;
    reg.each<SPosition>([&](Ecs::SEntity, SPosition&) { ++seen; });

    EXPECT_EQ(seen, 0);
}

TEST(EcsEach, MissingComponentTypeDoesNothing) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();
    reg.emplace<SPosition>(e);

    int seen = 0;
    reg.each<SPosition, SVelocity>([&](Ecs::SEntity, SPosition&, SVelocity&) { ++seen; });

    EXPECT_EQ(seen, 0);
}

// ── eachEntity() ─────────────────────────────────────────────────────────────

TEST(EcsEachEntity, VisitsAllAlive) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity a = reg.create();
    const Ecs::SEntity b = reg.create();
    reg.destroy(a);

    std::vector<Ecs::SEntity> visited;
    reg.eachEntity([&](Ecs::SEntity e) { visited.push_back(e); });

    ASSERT_EQ(visited.size(), 1U);
    EXPECT_EQ(visited.front(), b);
}

TEST(EcsEachEntity, ConstRegistryVisitsAliveEntities) {
    Ecs::CRegistry reg;
    const auto     a = reg.create();
    const auto     b = reg.create();
    reg.destroy(a);
    const auto& constReg = reg;

    std::size_t count = 0;
    constReg.eachEntity([&](Ecs::SEntity) { ++count; });

    EXPECT_EQ(count, 1U);
    EXPECT_TRUE(constReg.alive(b));
}

// ── view() ───────────────────────────────────────────────────────────────────

TEST(EcsView, IteratesMatchingEntities) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity a = reg.create();
    const Ecs::SEntity b = reg.create();

    reg.emplace<SPosition>(a);
    reg.emplace<SVelocity>(a);
    reg.emplace<SPosition>(b);

    std::vector<Ecs::SEntity> visited;
    for (const Ecs::SEntity e : reg.view<SPosition, SVelocity>())
        visited.push_back(e);

    ASSERT_EQ(visited.size(), 1U);
    EXPECT_EQ(visited.front(), a);
}

TEST(EcsView, EmptyWhenNothingMatches) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity a = reg.create();
    reg.emplace<SPosition>(a);

    int count = 0;
    for ([[maybe_unused]] const Ecs::SEntity e : reg.view<SPosition, SVelocity>())
        ++count;

    EXPECT_EQ(count, 0);
}

TEST(EcsView, FiltersEntitiesUsingTheSmallestDriverPool) {
    Ecs::CRegistry reg;
    const auto     withBoth     = reg.create();
    const auto     positionOnly = reg.create();
    reg.emplace<SPosition>(withBoth);
    reg.emplace<SPosition>(positionOnly);
    reg.emplace<SVelocity>(withBoth);

    std::vector<Ecs::SEntity> visited;
    for (const auto entity : reg.view<SPosition, SVelocity>())
        visited.push_back(entity);

    ASSERT_EQ(visited.size(), 1U);
    EXPECT_EQ(visited.front(), withBoth);
}

TEST(EcsView, SkipsNonMatchingEntitiesInTheDriverPool) {
    Ecs::CRegistry reg;
    const auto      matching = reg.create();
    const auto      positionOnly = reg.create();
    const auto      velocityOnly = reg.create();
    const auto      anotherVelocityOnly = reg.create();
    reg.emplace<SPosition>(matching);
    reg.emplace<SPosition>(positionOnly);
    reg.emplace<SVelocity>(matching);
    reg.emplace<SVelocity>(velocityOnly);
    reg.emplace<SVelocity>(anotherVelocityOnly);

    std::vector<Ecs::SEntity> visited;
    for (const auto entity : reg.view<SPosition, SVelocity>())
        visited.push_back(entity);

    ASSERT_EQ(visited.size(), 1U);
    EXPECT_EQ(visited.front(), matching);
}

// ── Registry-wide ────────────────────────────────────────────────────────────

TEST(EcsRegistry, ClearResetsEverything) {
    Ecs::CRegistry     reg;
    const Ecs::SEntity e = reg.create();
    reg.emplace<SPosition>(e);

    reg.clear();

    EXPECT_EQ(reg.size(), 0U);
    EXPECT_FALSE(reg.alive(e));
    EXPECT_EQ(reg.pool<SPosition>().size(), 0U);
}
