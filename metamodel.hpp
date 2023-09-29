#pragma once

namespace metahsm {

struct SimpleStateType {};
struct CompositeStateType {};
struct TopStateType {};

struct EntityBase {};
struct StateBase : EntityBase {};
struct SimpleStateBase : StateBase {};
struct CompositeStateBase : StateBase {};
struct TopStateBase : CompositeStateBase {};

template <typename _Entity, typename _SFINAE = void, typename _Enable = void>
struct MetaType
{
    using Type = SimpleStateType;
};

template <typename _Entity>
struct MetaType<_Entity, std::void_t<typename _Entity::SubStates>, 
    std::enable_if_t<std::is_base_of_v<StateBase, _Entity>
                 && !std::is_base_of_v<TopStateBase, _Entity>
    >>
{
    using Type = CompositeStateType;
};

template <typename _Entity>
struct MetaType<_Entity, std::void_t<typename _Entity::SubStates>, 
    std::enable_if_t<std::is_base_of_v<TopStateBase, _Entity>>>
{
    using Type = TopStateType;
};


}