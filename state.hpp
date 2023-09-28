#pragma once

#include <type_traits>
#include <tuple>
#include <variant>
#include <functional>

namespace metahsm {

class Entity {};

namespace metamodel {
namespace entitytypes {
struct SimpleStateType {};
struct CompositeStateType {};
struct TopStateType {};
}
}

namespace basemodel {
namespace entitybases {
struct Entity {};
struct EState : Entity {};
struct SimpleState : EState {};
struct CompositeState : EState {};
struct TopState : CompositeState {};
}
}

template <typename EntityType, typename TopStateDefinition, typename SFINAE = void, typename Enable = void>
struct MetaType
{
    using Type = metamodel::entitytypes::SimpleStateType;
};

template <typename EntityType, typename TopStateDefinition>
struct MetaType<EntityType, TopStateDefinition, std::void_t<typename EntityType::SubStates>, 
    std::enable_if_t<std::is_base_of_v<basemodel::entitybases::EState, EntityType>
                 && !std::is_base_of_v<basemodel::entitybases::TopState, EntityType>
    >>
{
    using Type = metamodel::entitytypes::CompositeStateType;
};

template <typename EntityType, typename TopStateDefinition>
struct MetaType<EntityType, TopStateDefinition, std::void_t<typename EntityType::SubStates>, 
    std::enable_if_t<std::is_base_of_v<basemodel::entitybases::TopState, EntityType>>>
{
    using Type = metamodel::entitytypes::TopStateType;
};

template <typename T, typename TopStateDefinition>
using meta_type_t = typename MetaType<T, TopStateDefinition>::Type;

template <typename Definition, typename TopStateDefinition, typename MetaType>
class MixinImpl;

template <typename Definition, typename TopStateDefinition>
using Mixin = typename MixinImpl<Definition, TopStateDefinition, meta_type_t<Definition, TopStateDefinition>>::Type;

template <typename StateDefinition, typename Event, typename Invocable = void>
struct HasReactionToEvent : public std::false_type
{};

template <typename StateDefinition, typename Event>
struct HasReactionToEvent<StateDefinition, Event, std::void_t<std::is_invocable<decltype(&StateDefinition::react), StateDefinition&, const Event&>>>
: std::true_type
{};

template <typename ... Tuple>
using TupleJoin = decltype(std::tuple_cat(std::declval<Tuple>()...));

template<typename SourceStateDefinition>
class TypeErasedTransition
{
public:
    virtual bool execute(Mixin<SourceStateDefinition, typename SourceStateDefinition::SMD>& source) const = 0;
};

template <typename SourceStateDefinition, typename TargetStateDefinition>
class RegularTransition : public TypeErasedTransition<SourceStateDefinition>
{
public:
    bool execute(Mixin<SourceStateDefinition, typename SourceStateDefinition::SMD>& source) const override {
        source.template executeTransition<TargetStateDefinition>();
        return true;
    }
};

template <typename SourceStateDefinition>
class NoTransition : public TypeErasedTransition<SourceStateDefinition>
{
public:
    bool execute(Mixin<SourceStateDefinition, typename SourceStateDefinition::SMD>&) const override {
        return true;
    }
};

template <typename SourceStateDefinition>
class ConditionNotMet : public TypeErasedTransition<SourceStateDefinition>
{
public:
    bool execute(Mixin<SourceStateDefinition, typename SourceStateDefinition::SMD>&) const override {
        return false;
    }
};

template <typename SourceStateDefinition, typename TargetStateDefinition>
const RegularTransition<SourceStateDefinition, TargetStateDefinition> regular_transition_;

template <typename SourceStateDefinition>
const NoTransition<SourceStateDefinition> no_transition_;

template <typename SourceStateDefinition>
const ConditionNotMet<SourceStateDefinition> condition_not_met_;

template <typename StateDefinition, typename ContextDefinition, typename Enable = void>
struct IsInContext : std::false_type
{};

template <typename StateDefinition>
struct IsInContext<StateDefinition, StateDefinition, void> : std::true_type
{};

template <typename StateDefinition, typename ... ContextDefinition>
struct IsInContext<StateDefinition, std::tuple<ContextDefinition...>> :
std::disjunction<IsInContext<StateDefinition, ContextDefinition>...>
{};

template <typename StateDefinition, typename ContextDefinition>
struct IsInContext<StateDefinition, ContextDefinition, std::enable_if_t<
        !std::is_same_v<StateDefinition, ContextDefinition> &&
        !std::is_void_v<typename ContextDefinition::SubStates>
    >>
: IsInContext<StateDefinition, typename ContextDefinition::SubStates>
{};

template <typename ... T>
struct Collapse;

template <>
struct Collapse<>
{
    using Type = void;
};

template <typename T>
struct Collapse<T>
{
    using Type = T;
};

template <typename ... T>
struct Collapse<void, T...>
{
    using Type = typename Collapse<T...>::Type;
};

template <typename T, typename ... Us>
struct Collapse<T, Us...>
{
    using Type = T;
};

template <typename StateDefinition1, typename StateDefinition2, typename ContextDefinition, typename Enable = void>
struct LCAImpl
{
    using Type = void;
};

template <typename StateDefinition, typename ContextDefinition>
struct LCAImpl<StateDefinition, StateDefinition, ContextDefinition, void>
{
    using Type = StateDefinition;
};

template <typename StateDefinition1, typename StateDefinition2, typename ... ContextDefinition>
struct LCAImpl<StateDefinition1, StateDefinition2, std::tuple<ContextDefinition...>, void>
{
    using Type = typename Collapse<typename LCAImpl<StateDefinition1, StateDefinition2, ContextDefinition>::Type...>::Type;
};

template <typename StateDefinition1, typename StateDefinition2, typename ContextDefinition>
struct LCAImpl<StateDefinition1, StateDefinition2, ContextDefinition, std::enable_if_t<
        !std::is_void_v<typename ContextDefinition::SubStates> &&
        !std::is_same_v<StateDefinition1, StateDefinition2> &&
        IsInContext<StateDefinition1, ContextDefinition>::value &&
        IsInContext<StateDefinition2, ContextDefinition>::value
    >>
{
    using SubType = typename LCAImpl<StateDefinition1, StateDefinition2, typename ContextDefinition::SubStates>::Type;
    using Type = typename Collapse<SubType, ContextDefinition>::Type;
};

template <typename StateDefinition1, typename StateDefinition2>
using LCA = typename LCAImpl<StateDefinition1, StateDefinition2, typename StateDefinition1::SMD>::Type;

template <typename StateDefinition, typename ContextDefinition, typename SuperContextDefinition, typename Enable = void>
struct ContextImpl
{
    using Type = void;
};

template <typename StateDefinition, typename ... ContextDefinition, typename SuperContextDefinition>
struct ContextImpl<StateDefinition, std::tuple<ContextDefinition...>, SuperContextDefinition, void>
{
    using Type = typename Collapse<typename ContextImpl<StateDefinition, ContextDefinition, SuperContextDefinition>::Type...>::Type;
};

template <typename StateDefinition, typename ContextDefinition, typename SuperContextDefinition>
struct ContextImpl<StateDefinition, ContextDefinition, typename SuperContextDefinition, std::enable_if_t<
        !std::is_void_v<typename ContextDefinition::SubStates> &&
        !std::is_same_v<StateDefinition, ContextDefinition> &&
        IsInContext<StateDefinition, ContextDefinition>::value
    >>
{
    using SubType = typename ContextImpl<StateDefinition, typename ContextDefinition::SubStates, ContextDefinition>::Type;
    using Type = typename Collapse<SubType, ContextDefinition>::Type;
};

template <typename StateDefinition, typename SuperContextDefinition>
struct ContextImpl<StateDefinition, StateDefinition, SuperContextDefinition, void>
{
    using Type = SuperContextDefinition;
};

template <typename StateDefinition, typename TopStateDefinition>
using Context  = typename ContextImpl<StateDefinition, TopStateDefinition, TopStateDefinition>::Type;

template <typename SourceStateDefinition, typename TargetStateDefinition>
struct IsValidTransition
{
    static constexpr bool value = IsInContext<TargetStateDefinition, typename SourceStateDefinition::SMD>::value;
};

template <typename StateDefinition, typename TopStateDefinition>
class StateCrtp : public basemodel::entitybases::EState
{
public:
    template <typename SubStateDefinition>
    using State = StateCrtp<SubStateDefinition, TopStateDefinition>;
    using SMD = TopStateDefinition;
    //using ThisMixin = Mixin<StateDefinition, TopStateDefinition>;
    using Initial = void;

    StateCrtp() {
        onEntry();
    }

    ~StateCrtp() {
        onExit();
    }

    template <typename ContextDefinition>
    decltype(auto) context()  {
        static_assert(IsInContext<StateDefinition, ContextDefinition>::value, "Requested context is not available in the state!");
        return mixin().template context<ContextDefinition>();
    }

    template <typename TargetStateDefinition>
    const TypeErasedTransition<StateDefinition>* transition() {
        static_assert(IsValidTransition<StateDefinition, TargetStateDefinition>::value);
        return &regular_transition_<StateDefinition, TargetStateDefinition>;
    }

    const TypeErasedTransition<StateDefinition>* no_transition() {
        return &no_transition_<StateDefinition>;
    }

    const TypeErasedTransition<StateDefinition>* condition_not_met() {
        return &condition_not_met_<StateDefinition>;
    }

    void onEntry() { }
    void onExit() { }

private:
    decltype(auto) mixin() {
        return static_cast<Mixin<StateDefinition, TopStateDefinition>&>(*this);
    }
};

template <typename StateDefinition>
struct StateSpec
{
    using Type = StateDefinition;
};

template <typename StateDefinitions, typename TopStateDefinition>
class SubStatesHelper;

template <typename ... StateDefinition, typename TopStateDefinition>
struct SubStatesHelper<std::tuple<StateDefinition...>, TopStateDefinition>
{
public:
    using Tuple = std::tuple<Mixin<StateDefinition, TopStateDefinition>...>;
    using Variant = std::variant<Mixin<StateDefinition, TopStateDefinition>...>;
    using Default = std::tuple_element_t<0, std::tuple<StateDefinition...>>;
    template <typename SubStateDefinition>
    using ContainingState = typename Collapse<
        std::conditional_t<
            IsInContext<SubStateDefinition, StateDefinition>::value,
            StateDefinition,
            void
        >...>::Type;
};


template <typename StateDefinition>
class TopStateMixin;

template <typename StateDefinition, typename TopStateDefinition>
class StateMixin : public StateDefinition
{
public:
    using TopStateMixin = TopStateMixin<TopStateDefinition>;
    using Definition = StateDefinition;
    using ContextMixin = Mixin<Context<StateDefinition, TopStateDefinition>, TopStateDefinition>;
    using ThisMixin = Mixin<StateDefinition, TopStateDefinition>;

    StateMixin(TopStateMixin& top_state_mixin, ContextMixin& context_mixin)
    : top_state_mixin_{top_state_mixin},
      context_mixin_{context_mixin}
    {}

    template <typename Event>
    bool handleEvent(const Event& e) {
        if constexpr(HasReactionToEvent<StateDefinition, Event>::value) {
            auto type_erased_transition = this->react(e);
            return type_erased_transition->execute(mixin());
        }
        return false;
    }

    template <typename ContextDefinition>
    ContextDefinition& context() {
        if constexpr(std::is_same_v<ContextDefinition, StateDefinition>) {
            return *this;
        }
        else {
            return context_mixin_.template context<ContextDefinition>();
        }
    }

protected:
    TopStateMixin& top_state_mixin_;
    ContextMixin& context_mixin_;

    decltype(auto) mixin() {
        return static_cast<ThisMixin&>(*this);
    }
};

template <typename StateDefinition, typename TopStateDefinition>
class CompositeStateMixin : public StateMixin<StateDefinition, TopStateDefinition>
{
public:
    using SubStates = SubStatesHelper<typename StateDefinition::SubStates, TopStateDefinition>;
    using Initial = typename Collapse<typename StateDefinition::Initial, typename SubStates::Default>::Type;

    template <typename SubStateSpec>
    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin,
        SubStateSpec spec,
        std::enable_if_t<std::is_same_v<meta_type_t<typename SubStateSpec::Type, TopStateDefinition>, metamodel::entitytypes::CompositeStateType>>* = nullptr)
    : StateMixin(top_state_mixin, context_mixin),
      active_sub_state_{ 
        std::in_place_type<Mixin<typename SubStates::ContainingState<typename SubStateSpec::Type>, TopStateDefinition>>,
        top_state_mixin,
        mixin(),
        spec
      }
    {}

    template <typename SubStateSpec>
    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin,
        SubStateSpec spec,
        std::enable_if_t<std::is_same_v<meta_type_t<typename SubStateSpec::Type, TopStateDefinition>, metamodel::entitytypes::SimpleStateType>>* = nullptr)
    : StateMixin(top_state_mixin, context_mixin),
      active_sub_state_{ 
        std::in_place_type<Mixin<typename SubStates::ContainingState<typename SubStateSpec::Type>, TopStateDefinition>>,
        top_state_mixin,
        mixin()
      }
    {}

    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin,
        StateSpec<StateDefinition> spec)
    : CompositeStateMixin(top_state_mixin, context_mixin, StateSpec<Initial>{})
    {}

    CompositeStateMixin(
        TopStateMixin& top_state_mixin,
        ContextMixin& context_mixin)
    : CompositeStateMixin(top_state_mixin, context_mixin, StateSpec<Initial>{})
    {}

    template <typename Event>
    bool handleEvent(const Event& e) {
        auto do_handle_event = [&](auto& active_sub_state){ return active_sub_state.handleEvent(e); };
        bool substate_handled_the_event = std::visit(do_handle_event, active_sub_state_);
        if(substate_handled_the_event) {
            return true;
        }
        else {
            return StateMixin::handleEvent<Event>(e);
        }
    }

    template <typename TargetStateDefinition>
    void executeTransition() {
        auto do_execute_transition = [](auto& active_sub_state){ active_sub_state.template executeTransition<TargetStateDefinition>(); };
        std::visit(do_execute_transition, active_sub_state_);       
    }

    template <typename LCA, typename TargetStateDefinition>
    void executeTransition() {
        if constexpr (std::is_same_v<LCA, StateDefinition>) {
            using Spec = StateSpec<TargetStateDefinition>;
            using SubStateDefinition = SubStates::ContainingState<typename Spec::Type>;
            active_sub_state_.emplace<Mixin<SubStateDefinition, typename SubStateDefinition::SMD>>(
                top_state_mixin_,
                mixin());
        }
        else {
            auto do_execute_transition = [](auto& active_sub_state){ active_sub_state.template executeTransition<LCA, TargetStateDefinition>(); };
            std::visit(do_execute_transition, active_sub_state_);       
        }
    }

private:
    typename SubStates::Variant active_sub_state_;
};

template <typename StateDefinition, typename TopStateDefinition>
class SimpleStateMixin : public StateMixin<StateDefinition, TopStateDefinition>
{
public:
    SimpleStateMixin(TopStateMixin& top_state_mixin, ContextMixin& context_mixin)
    : StateMixin(top_state_mixin, context_mixin)
    {}

    template <typename TargetStateDefinition>
    void executeTransition() {
        using LCA = LCA<StateDefinition, TargetStateDefinition>;
        top_state_mixin_.template executeTransition<LCA, TargetStateDefinition>();             
    }

    template <typename LCA, typename TargetStateDefinition>
    void executeTransition() { }
};

template <typename TopStateDefinition>
class TopState : public StateCrtp<TopStateDefinition, TopStateDefinition> , public basemodel::entitybases::TopState
{};

template <typename TopStateDefinition>
class StateMachine;

template <typename StateDefinition>
class TopStateMixin : public CompositeStateMixin<StateDefinition, StateDefinition>
{
public:
    TopStateMixin(StateMachine<StateDefinition>& state_machine)
    : CompositeStateMixin(*this, *this),
      state_machine_{state_machine}
    {}

    template <typename ContextDefinition>
    ContextDefinition& context();

private:
    StateMachine<StateDefinition>& state_machine_;
};

template <typename Definition, typename TopStateDefinition, typename MetaType>
struct MixinImpl;

template <typename Definition, typename TopStateDefinition>
struct MixinImpl<Definition, TopStateDefinition, metamodel::entitytypes::SimpleStateType>
{
    using Type = SimpleStateMixin<Definition, TopStateDefinition>;
};

template <typename Definition, typename TopStateDefinition>
struct MixinImpl<Definition, TopStateDefinition, metamodel::entitytypes::CompositeStateType>
{
    using Type = CompositeStateMixin<Definition, TopStateDefinition>;
};

template <typename Definition>
struct MixinImpl<Definition, Definition, metamodel::entitytypes::TopStateType>
{
    using Type = TopStateMixin<Definition>;
};


}