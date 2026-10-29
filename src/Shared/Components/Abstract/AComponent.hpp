#pragma once

#include <cstdint>
#include <vector>

class AComponent {
  public:
    enum class eType : std::uint8_t {
        POSITION = 1,
        VELOCITY,
        HEALTH,
        PLAYER,
        PROJECTILE,
        COLLIDER,
        DRAWABLE_SHAPE,
        DRAWABLE_SPRITE,
        DRAWABLE_SPRITE_ATLAS,
        ENTITY_TYPE,
        CHAT_MESSAGE,
    };

    AComponent()          = default;
    virtual ~AComponent() = default;
    explicit AComponent([[maybe_unused]] std::vector<std::uint8_t> data) {}

    [[nodiscard]] virtual eType type() const                                     = 0;
    virtual void                serialize(std::vector<std::uint8_t>& data) const = 0;
    [[nodiscard]] bool          valid() const {
        return valid_;
    }

    AComponent(const AComponent&)            = default;
    AComponent& operator=(const AComponent&) = default;
    AComponent(AComponent&&)                 = default;
    AComponent& operator=(AComponent&&)      = default;

  protected:
    explicit AComponent(bool valid) : valid_(valid) {}
    void setInvalid() {
        valid_ = false;
    }

  private:
    bool valid_ = true;
};
