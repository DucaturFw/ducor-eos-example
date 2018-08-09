#include <eosiolib/eosio.hpp>
#include "carboncopy.hpp"

using namespace eosio;

class carboncopy : public eosio::contract {
  public:
      using contract::contract;

      /// @abi action 
      void exec() {
         print("Output from carboncopy");
      }
};

EOSIO_ABI( carboncopy, (exec) )
