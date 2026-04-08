#ifndef NEON_OUBLIETTE_DIALOGUE_ATOMS_H
#define NEON_OUBLIETTE_DIALOGUE_ATOMS_H

#include <entt/entt.hpp>
#include <string>
#include <functional>
#include <vector>

namespace NeonOubliette {
namespace Systems {

/**
 * @brief [NEW CLASS] A discrete, queryable fact derived from ECS state.
 * Used by the DialogueSystem to determine what topics an NPC can talk about.
 */
struct DialogueAtom {
    std::string tag;
    std::function<bool(const entt::registry&, entt::entity)> condition;
    int weight = 50;
};

class DialogueAtomLibrary {
public:
    static DialogueAtomLibrary& instance() {
        static DialogueAtomLibrary inst;
        return inst;
    }

    void register_atom(const DialogueAtom& atom) {
        m_atoms.push_back(atom);
    }

    const std::vector<DialogueAtom>& get_atoms() const {
        return m_atoms;
    }

    /**
     * @brief Query all atoms for a specific entity and return those whose conditions are met.
     */
    std::vector<const DialogueAtom*> query_active_atoms(const entt::registry& registry, entt::entity entity) const {
        std::vector<const DialogueAtom*> active;
        for (const auto& atom : m_atoms) {
            if (atom.condition(registry, entity)) {
                active.push_back(&atom);
            }
        }
        return active;
    }

private:
    DialogueAtomLibrary() = default;
    std::vector<DialogueAtom> m_atoms;
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_DIALOGUE_ATOMS_H
