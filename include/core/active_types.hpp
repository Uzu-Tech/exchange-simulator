#pragma once

#include "maker.hpp"
#include "pure_market_maker.hpp"
#include "sim_price.hpp"
#include "taker.hpp"

template<typename... Ts>
struct TypeList {};

using ActiveStrategy = PureMarketMaker;
using ActivePriceModel = SimpleRandomWalk;
using ActiveMakers = TypeList<SymmetricMaker, SymmetricMaker>;
using ActiveTakers = TypeList<RandomTaker>;