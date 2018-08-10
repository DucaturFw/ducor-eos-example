#include <eosiolib/eosio.hpp>
#include "carboncopy.hpp"

void transfer(uint64_t receiver, uint64_t code)
{
  eosio::print("CARBOOOON");
}

extern "C"
{
  void apply(uint64_t receiver, uint64_t code, uint64_t action)
  {
    switch (action)
    {
    case N(transfer):
      return transfer(receiver, code);
    }
  }
}