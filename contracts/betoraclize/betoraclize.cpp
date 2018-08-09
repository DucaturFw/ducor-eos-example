#include <eosiolib/asset.hpp>
#include "oraclized.hpp"

// @abi table btcusdt i64
struct price
{
  uint64_t value;
  uint8_t decimals;

  EOSLIB_SERIALIZE(price, (value)(decimals))
};

// @abi table state i64
struct state
{
  price price_start;
  price price_end;
  uint64_t time_start;
  uint64_t time_end;
  uint64_t total_raise;
  uint64_t total_fall;

  EOSLIB_SERIALIZE(state, (price_start)(price_end)(time_start)(time_end)(total_raise)(total_fall))
};

// @abi table bet i64
struct bet
{
  account_name player;
  uint64_t amount;
  bool raise;

  uint64_t primary_key() const
  {
    return player;
  }

  EOSLIB_SERIALIZE(bet, (player)(amount)(raise))
};

typedef oraclized<N(btcusdt), 11, 10, price> btcusdt_data;
typedef singleton<N(master), account_name> oraclize_master;
typedef singleton<N(state), state> state_def;
typedef multi_index<N(bet), bet> bets_def;

class YOUR_CONTRACT_NAME : public eosio::contract
{
private:
  btcusdt_data btcusdt;
  account_name master;
  state_def current_state;
  bets_def bets;

public:
  using contract::contract;

  YOUR_CONTRACT_NAME(account_name s) : contract(s), btcusdt(_self, _self), current_state(_self, _self), bets(_self, _self)
  {
    master = oraclize_master(_self, _self).get_or_create(_self, N(undefined));
  }

  void setup(account_name administrator, account_name master, account_name registry)
  {
    require_auth(administrator);
    oraclize_master(_self, _self).set(master, _self);
    ask_data(administrator, registry, "0xa671e4d5c2daf92bd8b157e766e2c65010e55098cccde25fbb16eab53d8ae4e3");
  }

  void ask_data(account_name administrator, account_name registry, std::string data)
  {
    action(permission_level{administrator, N(active)},
           registry, N(ask),
           std::make_tuple(administrator, _self, data))
        .send();
  }

  void pushprice(account_name oracle, std::string data_id, price data)
  {
    require_auth(oracle);

    if (strcmp(data_id.c_str(), "0xa671e4d5c2daf92bd8b157e766e2c65010e55098cccde25fbb16eab53d8ae4e3") == 0)
    {
      btcusdt.set(data, _self);

      if (!current_state.exists())
      {
        // setup bettings at begin
        state initial;
        initial.price_start = data;
        initial.time_start = now();
        initial.time_end = initial.time_start + 60 * 60 * 24 * 7; // week after

        current_state.set(initial, _self);
      }
      else if (current_state.get().time_end <= now())
      {
        state before = current_state.get();
        before.price_end = data;
        current_state.set(before, _self);
      }
    }
  }

  void makebet(account_name player, eosio::asset eos_tokens, bool raise, std::string memo)
  {
    eosio_assert(current_state.get().time_start > 0, "Start time isn't setuped yet");
    eosio_assert(current_state.get().time_start <= now(), "Start time in future");
    eosio_assert(current_state.get().time_end > now(), "Deadline is achieved already");

    eosio_assert(eos_tokens.symbol == S(4,EOS), "Requires EOS tokens to bet");
    eosio_assert(eos_tokens.amount > 0, "Bet should be at least 1 token");
    require_auth(player);

    auto itt = bets.find(player);
    eosio_assert(itt == bets.end(), "Player already made decision");

    bets.emplace(player, [&](bet &b) {
      b.player = player;
      b.amount = eos_tokens.amount;
      b.raise = raise;
    });

    if(raise){ 
      increase_raise(eos_tokens.amount);
    }else {
      increase_fall(eos_tokens.amount);
    }

    action(
        permission_level{player, N(active)},
        N(eosio.token), N(transfer),
        std::make_tuple(player, _self, eos_tokens.amount, memo));
  }

  void withdrawal(account_name player, std::string memo) 
  {
    state end_state = current_state.get();
    eosio_assert(end_state.time_start > 0, "Start time isn't setuped yet");
    eosio_assert(end_state.time_end <= now(), "Deadline is achieved already");
    eosio_assert(end_state.price_end.value > 0, "Price isn't pushed yet");

    auto player_bet = bets.find(player);
    eosio_assert(player_bet != bets.end(), "Player didn't bet anything");

    bool actual_raise = end_state.price_start.value < end_state.price_end.value;
    if (player_bet->raise == actual_raise) {
      uint64_t total = actual_raise ? end_state.total_raise : end_state.total_fall;
      uint64_t stake = total / player_bet->amount;
      uint64_t prize = (end_state.total_raise + end_state.total_fall) / stake;

      bets.modify(player_bet, _self, [&](bet &b) {
        b.amount = 0;
      });

      action(
          permission_level{_self, N(active)},
          N(eosio.token), N(transfer),
          std::make_tuple(_self, player, prize, memo));
    }
  }

  void increase_raise(uint64_t amount) {
    auto state = current_state.get();
    state.total_raise += amount;
    current_state.set(state, _self);
  }

  void increase_fall(uint64_t amount) {
    auto state = current_state.get();
    state.total_fall += amount;
    current_state.set(state, _self);
  }
};

EOSIO_ABI(YOUR_CONTRACT_NAME, (setup)(pushprice)(makebet))