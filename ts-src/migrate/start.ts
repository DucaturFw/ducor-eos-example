import * as eosic from "eosic";
import Eos from "eosjs";

export default async function execute() {
  const btcusd_id =
    "0x5d4e98a94bf3e6c7d539f4988cc5f7557fe12c8e53ec6a193b7c0ad92dafe188";
  const [pub, wif] = [
    "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV",
    "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"
  ];

  const eos = Eos({
    // chainId: "038f4b0fc8ff18a4f0842a8f0564611f6e96e8535901dd45e43ac8691a1c4dca",
    // httpEndpoint: "https://jungle.eos.smartz.io:443",
    httpEndpoint: "http://0.0.0.0:8888",
    keyProvider: wif
  });

  const oracle = "oracle";
  const winner = "winner";
  const loser = "loser";

  let betAccount,
    betContract,
    masterAccount,
    masterContract,
    vulnerableAccount,
    vulnerableContract,
    tokenAccount,
    tokenContract;

  ({
    account: tokenAccount,
    contract: tokenContract
  } = await eosic.createContract(pub, eos, "eosio.token", {
    contractName: "eosio.token"
  }));

  console.log(tokenAccount);

  console.log("Create accounts");
  await eosic.createAccount(eos, pub, winner);
  await eosic.createAccount(eos, pub, loser);
  await eosic.createAccount(eos, pub, oracle);

  await (<any>tokenContract).create("eosio", "1000000.0000 EOS", {
    authorization: ["eosio", tokenAccount]
  });
  await (<any>tokenContract).issue("eosio", "200.0000 EOS", "initial", {
    authorization: ["eosio", tokenAccount]
  });
  await (<any>tokenContract).transfer("eosio", winner, "100.0000 EOS", "memo", {
    authorization: ["eosio", tokenAccount]
  });
  await (<any>tokenContract).transfer("eosio", loser, "100.0000 EOS", "memo", {
    authorization: ["eosio", tokenAccount]
  });

  console.log("Create bet contract");
  ({
    account: masterAccount,
    contract: masterContract
  } = await eosic.createContract(pub, eos, "masteroracle"));

  ({ account: betAccount, contract: betContract } = await eosic.createContract(
    pub,
    eos,
    "betoraclize",
    {
      contractName: "bet"
    }
  ));


  ({ account: vulnerableAccount, contract: vulnerableContract } = await eosic.createContract(
    pub,
    eos,
    "vulnerable",
    {
      contractName: "vulnerable"
    }
  ));

  await eosic.allowContract(eos, "eosio", pub, betAccount);
  await eosic.allowContract(eos, betAccount, pub, betAccount);
  console.log("Setup bet contract");
  await (<any>betContract).setup(oracle, masterAccount, {
    authorization: ["eosio", betAccount]
  });

  console.log(
    await eos.getTableRows({
      code: betAccount,
      scope: betAccount,
      table: "state",
      json: true
    })
  );
  console.log("push first price");
  await (<any>betContract).pushprice(
    oracle,
    btcusd_id,
    {
      value: 20000 * 1e8,
      decimals: 8
    },
    {
      authorization: [oracle]
    }
  );

  console.log(
    JSON.stringify(
      await eos.getTableRows({
        code: betAccount,
        scope: betAccount,
        table: "state",
        json: true
      })
    )
  );

  console.log(
    await eos.getCurrencyBalance({
      code: "eosio.token",
      account: "eosio",
      symbol: "EOS"
    })
  );

  // account_name player, eosio::asset eos_tokens, bool raise, std::string memo
  // await (<any>betContract).makebet(winner, "50.0000 EOS", 1, "help me", {
  //   authorization: [winner]
  // });
  // await (<any>betContract).makebet(loser, "30.0000 EOS", 1, "help me", {
  //   authorization: [loser]
  // });

  console.log("make bets");
  await eos.transfer(winner, betAccount, "50.0000 EOS", "1");
  await eos.transfer(loser, betAccount, "30.0000 EOS", "");

  console.log(
    JSON.stringify(
      await eos.getTableRows({
        code: betAccount,
        scope: betAccount,
        table: "state",
        json: true
      })
    )
  );

  console.log("wait for end");
  await new Promise(resolve => setTimeout(resolve, 5000));

  await (<any>betContract).pushprice(
    oracle,
    btcusd_id,
    {
      value: 22000 * 1e8,
      decimals: 8
    },
    {
      authorization: [oracle]
    }
  );

  console.log(
    await eos.getCurrencyBalance({
      code: "eosio.token",
      account: winner,
      symbol: "EOS"
    })
  );
  
  console.log('withdrawal process');
  await (<any>betContract).withdrawal(loser, {
    authorization: [winner]
  });
  await (<any>betContract).withdrawal(winner, {
    authorization: [winner]
  });

  console.log(
    await eos.getCurrencyBalance({
      code: "eosio.token",
      account: winner,
      symbol: "EOS"
    })
  );


  console.log(
    await eos.getTableRows({
      code: betAccount,
      scope: betAccount,
      table: "bet",
      json: true,
      limit: 9999,
      lower_bound: 0
    })
  );
}
