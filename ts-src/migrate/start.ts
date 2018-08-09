import * as eosic from "eosic";
import Eos from "eosjs";

export default async function execute() {
  const btcusd_id =
    "0xa671e4d5c2daf92bd8b157e766e2c65010e55098cccde25fbb16eab53d8ae4e3";
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
  let betAccount,
    betContract,
    masterAccount,
    masterContract,
    tokenAccount,
    tokenContract;

  ({
    account: tokenAccount,
    contract: tokenContract
  } = await eosic.createContract(pub, eos, "eosio.token", {
    contractName: "eosio.token"
  }));

  await (<any>tokenContract).create("eosio", "1000000.0000 EOS", {
    authorization: ["eosio", tokenAccount]
  });
  await (<any>tokenContract).issue("eosio", "1000000.0000 EOS", "initial", {
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
    "betoraclize"
  ));

  console.log("Create oracle account");
  await eosic.createAccount(eos, pub, oracle);
  await eosic.allowContract(eos, "eosio", pub, betAccount);

  console.log("Setup bet contract");
  await (<any>betContract).setup("eosio", oracle, masterAccount, {
    authorization: ["eosio"]
  });

  console.log(
    await eos.getTableRows({
      code: betAccount,
      scope: betAccount,
      table: "state",
      json: true
    })
  );

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

  // account_name player, eosio::asset eos_tokens, bool raise, std::string memo
  await (<any>betContract).makebet("eosio", "10.0000 EOS", 1, "help me", {
    authorization: ["eosio"]
  });

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
}
