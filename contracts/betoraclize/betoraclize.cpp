#include <eosiolib/eosio.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/system.h>

using namespace eosio;

constexpr uint64_t DECIMAL_MULTIPLIER = 1e4;

struct price
{
  uint64_t value;
  uint8_t decimals;

  EOSLIB_SERIALIZE(price, (value)(decimals))
};

typedef singleton<N(master), account_name> oraclize_master;
typedef singleton<N(oracle), account_name> oracle_account;

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

typedef singleton<N(state), state> state_def;

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

typedef multi_index<N(bet), bet> bets_def;

class betoraclize : public eosio::contract
{
private:
  account_name known_master;
  account_name known_oracle;
  state_def current_state;
  bets_def bets;

public:
  using contract::contract;

  betoraclize(account_name s) : contract(s),
                                current_state(_self, _self),
                                bets(_self, _self)
  {
    known_master = oraclize_master(_self, _self).get_or_create(_self, N(undefined));
    known_oracle = oracle_account(_self, _self).get_or_create(_self, N(undefined));
  }

  //@abi action
  void setup(account_name oracle, account_name master)
  {
    require_auth(_self);
    oracle_account(_self, _self).set(oracle, _self);
    oraclize_master(_self, _self).set(master, _self);
    ask_data(_self, master, "0x5d4e98a94bf3e6c7d539f4988cc5f7557fe12c8e53ec6a193b7c0ad92dafe188");
  }

  void ask_data(account_name administrator, account_name master, std::string data)
  {
    action(permission_level{administrator, N(active)},
           master, N(ask),
           std::make_tuple(administrator, _self, data))
        .send();
  }

  //@abi action
  void pushprice(account_name oracle, std::string data_id, price data)
  {
    require_auth(oracle);
    eosio_assert(known_oracle == oracle, "unkown oracle");

    if (strcmp(data_id.c_str(), "0x5d4e98a94bf3e6c7d539f4988cc5f7557fe12c8e53ec6a193b7c0ad92dafe188") == 0)
    {
      if (!current_state.exists())
      {
        // setup bettings at begin
        state initial;
        initial.price_start = data;
        initial.time_start = now();
        initial.time_end = initial.time_start + 3; //60 * 60 * 24 * 7; // week after

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

  void transfer(uint64_t self, uint64_t code)
  {
    auto data = unpack_action_data<currency::transfer>();
    if (data.from == self || data.to != self)
    {
      return;
    }
    eosio_assert(current_state.get().time_start > 0, "Start time isn't setuped yet");
    eosio_assert(current_state.get().time_start <= now(), "Start time in future");
    eosio_assert(current_state.get().time_end > now(), "Deadline is achieved already");
    eosio_assert(code == N(eosio.token), "I reject your non-eosio.token deposit");
    eosio_assert(data.quantity.symbol == S(4, EOS), "I think you're looking for another contract");
    eosio_assert(data.quantity.is_valid(), "Are you trying to corrupt me?");
    eosio_assert(data.quantity.amount > 0, "When pigs fly");
    require_auth(data.from);

    auto itt = bets.find(data.from);
    eosio_assert(itt == bets.end(), "Player already made decision");

    bool raise = data.memo.length() > 0;

    bets.emplace(data.from, [&](bet &b) {
      b.player = data.from;
      b.amount = data.quantity.amount;
      b.raise = raise;
    });

    auto state = current_state.get();
    if (raise)
    {
      state.total_raise += data.quantity.amount;
    }
    else
    {
      state.total_fall += data.quantity.amount;
    }

    current_state.set(state, _self);
  }

  //@abi action
  void withdrawal(account_name player)
  {
    state end_state = current_state.get();
    eosio_assert(end_state.time_start > 0, "Start time isn't setuped yet");
    eosio_assert(end_state.time_end <= now(), "Deadline isn't achieved yet");
    eosio_assert(end_state.price_end.value > 0, "Price isn't pushed yet");

    auto player_bet = bets.find(player);
    eosio_assert(player_bet != bets.end(), "Player didn't bet anything");

    bool actual_raise = end_state.price_start.value < end_state.price_end.value;
    if (player_bet->raise == actual_raise)
    {
      uint64_t total = actual_raise ? end_state.total_raise : end_state.total_fall;
      uint64_t stake = total * DECIMAL_MULTIPLIER / player_bet->amount;
      uint64_t prize = (end_state.total_raise + end_state.total_fall) * stake / DECIMAL_MULTIPLIER;

      bets.modify(player_bet, player_bet->player, [&](bet &b) {
        b.amount = 0;
      });

      action(
          permission_level{_self, N(active)},
          N(eosio.token), N(transfer),
          std::make_tuple(_self, player, eosio::asset{static_cast<int64_t>(prize), S(4, EOS)}, string()))
          .send();
    }
  }
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
  uint64_t self = receiver;
  if (action == N(onerror))
  {
    /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */
    eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account");
  }

  betoraclize thiscontract(self);

  if (code == self || action == N(onerror))
  {
    switch (action)
    {
      EOSIO_API(betoraclize, (setup)(pushprice)(withdrawal))
    }
  }

  if (code == N(eosio.token) && action == N(transfer))
  {
    thiscontract.transfer(receiver, code);
  }
}
